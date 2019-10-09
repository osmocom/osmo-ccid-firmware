/* Card (ICC) UART driver for simple serial readers attached to tty.
 * This allows you to use the CCID core in Linux userspace against a serial reader */

#include <unistd.h>
#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <osmocom/core/select.h>
#include <osmocom/core/serial.h>
#include <osmocom/core/utils.h>

#include "cuart.h"
#include "utils_ringbuffer.h"

/***********************************************************************
 * low-level helper routines
 ***********************************************************************/

static int _init_uart(int fd)
{
	struct termios tio;
	int rc;

	rc = tcgetattr(fd, &tio);
	if (rc < 0) {
		perror("tcgetattr()");
		return -EIO;
	}

	tio.c_iflag = 0;
	tio.c_oflag = 0;
	tio.c_lflag = 0;
	tio.c_cflag = CREAD | CLOCAL | CSTOPB | PARENB | CS8 | B9600;

	rc = tcsetattr(fd, TCSANOW, &tio);
	if (rc < 0) {
		perror("tcsetattr()");
		return -EIO;
	}

	return 0;
}

static void _set_dtr(int fd, bool dtr)
{
	int status, rc;

	rc = ioctl(fd, TIOCMGET, &status);
	OSMO_ASSERT(rc == 0);
	if (dtr) /* set DTR */
		status |= TIOCM_DTR;
	else
		status &= ~TIOCM_DTR;
	rc = ioctl(fd, TIOCMSET, &status);
	OSMO_ASSERT(rc == 0);
}

static void _set_rts(int fd, bool rts)
{
	int status, rc;

	rc = ioctl(fd, TIOCMGET, &status);
	OSMO_ASSERT(rc == 0);
	if (rts) /* set RTS */
		status |= TIOCM_RTS;
	else
		status &= ~TIOCM_RTS;
	rc = ioctl(fd, TIOCMSET, &status);
	OSMO_ASSERT(rc == 0);
}

static int read_timeout(int fd, uint8_t *out, size_t len, unsigned long timeout_ms)
{
	struct timeval tv = { .tv_sec = timeout_ms / 1000, .tv_usec = (timeout_ms % 1000) * 1000 };
	fd_set rd_set;
	int rc;

	FD_ZERO(&rd_set);
	FD_SET(fd, &rd_set);
	rc = select(fd+1, &rd_set, NULL, NULL, &tv);
	if (rc == 0)
		return -ETIMEDOUT;
	else if (rc < 0)
		return rc;

	return read(fd, out, len);
}

static int _flush(int fd)
{
#if 0
	uint8_t buf[1];
	int rc;

	while (1) {
		rc = read_timeout(fd, buf, sizeof(buf), 10);
		if (rc == -ETIMEDOUT)
			return 0;
		else if (rc < 0)
			return rc;
	}
#else
	return tcflush(fd, TCIFLUSH);
#endif
}

/***********************************************************************
 * Interface with card_uart (cuart) core
 ***********************************************************************/

/* forward-declaration */
static struct card_uart_driver tty_uart_driver;
static int tty_uart_close(struct card_uart *cuart);

static int tty_uart_fd_cb(struct osmo_fd *ofd, unsigned int what)
{
	struct card_uart *cuart = (struct card_uart *) ofd->data;
	uint8_t buf[256];
	int rc;

	if (what & OSMO_FD_READ) {
		int i;
		/* read any pending bytes and feed them into ring buffer */
		rc = read(ofd->fd, buf, sizeof(buf));
		OSMO_ASSERT(rc > 0);
		for (i = 0; i < rc; i++) {
#ifndef CREAD_ACTUALLY_WORKS
			/* work-around for https://bugzilla.kernel.org/show_bug.cgi?id=205033 */
			if (cuart->tx_busy) {
				if (cuart->u.tty.rx_count_during_tx < cuart->u.tty.tx_buf_len) {
					/* FIXME: compare! */
					cuart->u.tty.rx_count_during_tx += 1;
					if (cuart->u.tty.rx_count_during_tx == cuart->u.tty.tx_buf_len) {
						cuart->tx_busy = false;
						card_uart_notification(cuart, CUART_E_TX_COMPLETE,
									(void *)cuart->u.tty.tx_buf);
					}
					continue;
				}
			}
#endif
			if (!cuart->rx_enabled)
				continue;

			if (cuart->rx_threshold == 1) {
				/* bypass ringbuffer and report byte directly */
				card_uart_notification(cuart, CUART_E_RX_SINGLE, &buf[i]);
			} else {
				/* go via ringbuffer and notify only after threshold */
				ringbuffer_put(&cuart->u.tty.rx_ringbuf, buf[i]);
				if (ringbuffer_num(&cuart->u.tty.rx_ringbuf) >= cuart->rx_threshold)
					card_uart_notification(cuart, CUART_E_RX_COMPLETE, NULL);
			}
		}
	}
	if (what & OSMO_FD_WRITE) {
		unsigned int to_tx;
		OSMO_ASSERT(cuart->u.tty.tx_buf_len > cuart->u.tty.tx_index);
		/* push as many pending transmit bytes as possible */
		to_tx = cuart->u.tty.tx_buf_len - cuart->u.tty.tx_index;
		rc = write(ofd->fd, cuart->u.tty.tx_buf + cuart->u.tty.tx_index, to_tx);
		OSMO_ASSERT(rc > 0);
		cuart->u.tty.tx_index += rc;

		/* if no more bytes to transmit, disable OSMO_FD_WRITE */
		if (cuart->u.tty.tx_index >= cuart->u.tty.tx_buf_len) {
			ofd->when &= ~BSC_FD_WRITE;
#ifndef CREAD_ACTUALLY_WORKS
			/* don't immediately notify user; first wait for characters to be received */
#else
			/* ensure everything is written (tx queue/fifo drained) */
			tcdrain(cuart->u.tty.ofd.fd);
			cuart->tx_busy = false;
			/* notify */
			card_uart_notification(cuart, CUART_E_TX_COMPLETE, (void *)cuart->u.tty.tx_buf);
#endif
		}
	}
	return 0;
}

static int tty_uart_open(struct card_uart *cuart, const char *device_name)
{
	int rc;

	rc = ringbuffer_init(&cuart->u.tty.rx_ringbuf, cuart->u.tty.rx_buf, sizeof(cuart->u.tty.rx_buf));
	if (rc < 0)
		return rc;

	cuart->u.tty.ofd.fd = -1;
	rc = osmo_serial_init(device_name, B9600);
	if (rc < 0)
		return rc;

	osmo_fd_setup(&cuart->u.tty.ofd, rc, BSC_FD_READ, tty_uart_fd_cb, cuart, 0);
        cuart->u.tty.baudrate = B9600;

	rc = _init_uart(cuart->u.tty.ofd.fd);
	if (rc < 0) {
		tty_uart_close(cuart);
		return rc;
	}

	_flush(cuart->u.tty.ofd.fd);

	osmo_fd_register(&cuart->u.tty.ofd);

        return 0;
}

static int tty_uart_close(struct card_uart *cuart)
{
	OSMO_ASSERT(cuart->driver == &tty_uart_driver);
	if (cuart->u.tty.ofd.fd != -1) {
		osmo_fd_unregister(&cuart->u.tty.ofd);
		close(cuart->u.tty.ofd.fd);
		cuart->u.tty.ofd.fd = -1;
	}
	return 0;
}

static int tty_uart_async_tx(struct card_uart *cuart, const uint8_t *data, size_t len)
{
	OSMO_ASSERT(cuart->driver == &tty_uart_driver);

	cuart->u.tty.tx_buf = data;
	cuart->u.tty.tx_buf_len = len;
	cuart->u.tty.tx_index = 0;
	cuart->u.tty.rx_count_during_tx = 0;
	cuart->tx_busy = true;
	cuart->u.tty.ofd.when |= OSMO_FD_WRITE;

	return 0;
}

static int tty_uart_async_rx(struct card_uart *cuart, uint8_t *data, size_t len)
{
	int rc, i;
	OSMO_ASSERT(cuart->driver == &tty_uart_driver);

	/* FIXME: actually do this asynchronously */
	for (i = 0; i < len; i++) {
		rc = ringbuffer_get(&cuart->u.tty.rx_ringbuf, &data[i]);
		if (rc < 0)
			return i;
	}

	return i;
}

static int tty_uart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, bool enable)
{
	struct termios tio;
	int rc;

	switch (ctl) {
	case CUART_CTL_RX:
		rc = tcgetattr(cuart->u.tty.ofd.fd, &tio);
		if (rc < 0) {
			perror("tcgetattr()");
			return -EIO;
		}
		/* We do our best here, but lots of [USB] serial drivers seem to ignore
		 * CREAD, see https://bugzilla.kernel.org/show_bug.cgi?id=205033 */
		if (enable)
			tio.c_cflag |= CREAD;
		else
			tio.c_cflag &= ~CREAD;
		rc = tcsetattr(cuart->u.tty.ofd.fd, TCSANOW, &tio);
		if (rc < 0) {
			perror("tcsetattr()");
			return -EIO;
		}
		break;
	case CUART_CTL_RST:
		_set_rts(cuart->u.tty.ofd.fd, enable);
		if (enable)
			_flush(cuart->u.tty.ofd.fd);
		break;
	case CUART_CTL_POWER:
	case CUART_CTL_CLOCK:
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct card_uart_ops tty_uart_ops = {
	.open = tty_uart_open,
	.close = tty_uart_close,
	.async_tx = tty_uart_async_tx,
	.async_rx = tty_uart_async_rx,
	.ctrl = tty_uart_ctrl,
};

static struct card_uart_driver tty_uart_driver = {
	.name = "tty",
	.ops = &tty_uart_ops,
};

static __attribute__((constructor)) void on_dso_load_cuart_tty(void)
{
	card_uart_driver_register(&tty_uart_driver);
}
