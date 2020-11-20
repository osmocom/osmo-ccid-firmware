/* Card (ICC) UART driver for the Atmel ASF4 asynchronous USART */

#include <errno.h>

#include <osmocom/core/linuxlist.h>
#include <osmocom/core/utils.h>

#include <hal_usart_async.h>
#include <utils_ringbuffer.h>
#include "driver_init.h"

#include "ncn8025.h"

#include "cuart.h"

#ifndef ENABLE_DBG_UART7
static struct usart_async_descriptor* SIM_peripheral_descriptors[] = {&SIM0, &SIM1, &SIM2, &SIM3, &SIM4, &SIM5, &SIM6, &SIM7};
#else
static struct usart_async_descriptor* SIM_peripheral_descriptors[] = {&SIM0, &SIM1, &SIM2, &SIM3, &SIM4, &SIM5, &SIM6, NULL};
#endif

extern struct card_uart *cuart4slot_nr(uint8_t slot_nr);

/***********************************************************************
 * low-level helper routines
 ***********************************************************************/

static void _SIM_rx_cb(const struct usart_async_descriptor *const io_descr, uint8_t slot_nr)
{
	struct card_uart *cuart = cuart4slot_nr(slot_nr);
	int rc;
	OSMO_ASSERT(cuart);

	if (cuart->rx_threshold == 1) {
		/* bypass ringbuffer and report byte directly */
		uint8_t rx[1];
		rc = io_read((struct io_descriptor * const)&io_descr->io, rx, sizeof(rx));
		OSMO_ASSERT(rc == sizeof(rx));
		card_uart_notification(cuart, CUART_E_RX_SINGLE, rx);
	} else {
		/* go via ringbuffer and notify only after threshold */
		if (ringbuffer_num(&io_descr->rx) >= cuart->rx_threshold)
			card_uart_notification(cuart, CUART_E_RX_COMPLETE, NULL);
	}
}

static void _SIM_tx_cb(const struct usart_async_descriptor *const io_descr, uint8_t slot_nr)
{
	struct card_uart *cuart = cuart4slot_nr(slot_nr);
	OSMO_ASSERT(cuart);
	card_uart_notification(cuart, CUART_E_TX_COMPLETE, io_descr->tx_buffer);
}

#include <hpl_usart_async.h>
#include <hpl_usart_sync.h>


static void _SIM_error_cb(const struct usart_async_descriptor *const descr){
	volatile uint32_t status = hri_sercomusart_read_STATUS_reg(descr->device.hw);
	volatile uint32_t data = hri_sercomusart_read_DATA_reg(descr->device.hw);
	volatile uint32_t errrs = hri_sercomusart_read_RXERRCNT_reg(descr->device.hw);
	OSMO_ASSERT(0 == 1)
}

/* the below ugli-ness is required as the usart_async_descriptor doesn't have
 * some kind of 'private' member that could provide the call-back anty kind of
 * context */
static void SIM0_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 0);
}
static void SIM1_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 1);
}
static void SIM2_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 2);
}
static void SIM3_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 3);
}
static void SIM4_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 4);
}
static void SIM5_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 5);
}
static void SIM6_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 6);
}
static void SIM7_rx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_rx_cb(io_descr, 7);
}
static usart_cb_t SIM_rx_cb[8] = {
	SIM0_rx_cb, SIM1_rx_cb, SIM2_rx_cb, SIM3_rx_cb,
	SIM4_rx_cb, SIM5_rx_cb, SIM6_rx_cb, SIM7_rx_cb,
};
static void SIM0_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 0);
}
static void SIM1_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 1);
}
static void SIM2_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 2);
}
static void SIM3_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 3);
}
static void SIM4_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 4);
}
static void SIM5_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 5);
}
static void SIM6_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 6);
}
static void SIM7_tx_cb(const struct usart_async_descriptor *const io_descr)
{
	_SIM_tx_cb(io_descr, 7);
}
static usart_cb_t SIM_tx_cb[8] = {
	SIM0_tx_cb, SIM1_tx_cb, SIM2_tx_cb, SIM3_tx_cb,
	SIM4_tx_cb, SIM5_tx_cb, SIM6_tx_cb, SIM7_tx_cb,
};

#include <math.h>
#include "atmel_start.h"
#include "atmel_start_pins.h"
#include "config/hpl_gclk_config.h"
#include "iso7816_3.h"

/** possible clock sources for the SERCOM peripheral
 *  warning: the definition must match the GCLK configuration
 */
static const uint8_t sercom_glck_sources[] = {GCLK_PCHCTRL_GEN_GCLK2_Val, GCLK_PCHCTRL_GEN_GCLK4_Val, GCLK_PCHCTRL_GEN_GCLK6_Val};

 /** possible clock frequencies in MHz for the SERCOM peripheral
  *  warning: the definition must match the GCLK configuration
  */
static const double sercom_glck_freqs[] = {100E6 / CONF_GCLK_GEN_2_DIV, 100E6 / CONF_GCLK_GEN_4_DIV, 120E6 / CONF_GCLK_GEN_6_DIV};

/** the GCLK ID for the SERCOM SIM peripherals
 *  @note: used as index for PCHCTRL
 */
static const uint8_t SIM_peripheral_GCLK_ID[] = {SERCOM0_GCLK_ID_CORE, SERCOM1_GCLK_ID_CORE, SERCOM2_GCLK_ID_CORE, SERCOM3_GCLK_ID_CORE, SERCOM4_GCLK_ID_CORE, SERCOM5_GCLK_ID_CORE, SERCOM6_GCLK_ID_CORE, SERCOM7_GCLK_ID_CORE};

/** inverted signalling as per 7816-3 : inverted bit, inverted bit order
 */
static void set_inverted_signalling(void* hw, bool on) {

	hri_sercomusart_clear_CTRLA_ENABLE_bit(hw);

	hri_sercomusart_write_CTRLA_DORD_bit(hw, !on); // inverted == msb first
	hri_sercomusart_write_CTRLA_TXINV_bit(hw, on);
	hri_sercomusart_write_CTRLA_RXINV_bit(hw, on);
	hri_sercomusart_write_CTRLB_PMODE_bit(hw, on); // inverted == even parity

	hri_sercomusart_set_CTRLA_ENABLE_bit(hw);
}

/** change baud rate of card slot
 *  @param[in] slotnr slot number for which the baud rate should be set
 *  @param[in] baudrate baud rate in bps to set
 *  @return if the baud rate has been set, else a parameter is out of range
 */
static bool slot_set_baudrate(struct card_uart *cuart, uint32_t baudrate)
{
	uint8_t slotnr = cuart->u.asf4.slot_nr;

	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));

	// calculate the error corresponding to the clock sources
	uint16_t bauds[ARRAY_SIZE(sercom_glck_freqs)];
	double errors[ARRAY_SIZE(sercom_glck_freqs)];
	for (uint8_t i = 0; i < ARRAY_SIZE(sercom_glck_freqs); i++) {
		double freq = sercom_glck_freqs[i]; // remember possible SERCOM frequency
		uint32_t min = freq/16. *  (1. - 65535. / 65536.); // calculate the minimum baud rate for this frequency
		uint32_t max = freq/16. *  (1. - 1. / 65536.); // calculate the maximum baud rate for this frequency
		if (baudrate < min || baudrate > max) { // baud rate it out of supported range
			errors[i] = NAN;
		} else {
			uint16_t baud = round(65536. * (1. - 16. * (baudrate/freq)));
			bauds[i] = baud;
			double actual = freq/16. *  (1. - baud / 65536.);
			errors[i] = fabs(1.0 - (actual / baudrate));
		}
	}

	// find the smallest error
	uint8_t best = ARRAY_SIZE(sercom_glck_freqs);
	for (uint8_t i = 0; i < ARRAY_SIZE(sercom_glck_freqs); i++) {
		if (isnan(errors[i])) {
			continue;
		}
		if (best >= ARRAY_SIZE(sercom_glck_freqs)) {
			best = i;
		} else if (errors[i] < errors[best]) {
			best = i;
		}
	}
	if (best >= ARRAY_SIZE(sercom_glck_freqs)) { // found no clock supporting this baud rate
		return false;
	}

	// set clock and baud rate
	struct usart_async_descriptor* slot = SIM_peripheral_descriptors[slotnr]; // get slot
	if (NULL == slot) {
		return false;
	}

	// update cached values
	cuart->u.asf4.current_baudrate = baudrate;
	cuart->u.asf4.extrawait_after_rx = 1./baudrate * 1000 * 1000;

	printf("(%u) switching SERCOM clock to GCLK%u (freq = %lu kHz) and baud rate to %lu bps (baud = %u)\r\n", slotnr, (best + 1) * 2, (uint32_t)(round(sercom_glck_freqs[best] / 1000)), baudrate, bauds[best]);
	while (!usart_async_is_tx_empty(slot)); // wait for transmission to complete (WARNING no timeout)
	usart_async_disable(slot); // disable SERCOM peripheral
	hri_gclk_clear_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], (1 << GCLK_PCHCTRL_CHEN_Pos)); // disable clock for this peripheral
	while (hri_gclk_get_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], (1 << GCLK_PCHCTRL_CHEN_Pos))); // wait until clock is really disabled
	// it does not seem we need to completely disable the peripheral using hri_mclk_clear_APBDMASK_SERCOMn_bit
	hri_gclk_write_PCHCTRL_reg(GCLK, SIM_peripheral_GCLK_ID[slotnr], sercom_glck_sources[best] | (1 << GCLK_PCHCTRL_CHEN_Pos)); // set peripheral core clock and re-enable it
	usart_async_set_baud_rate(slot, bauds[best]); // set the new baud rate
	usart_async_enable(slot); // re-enable SERCOM peripheral

	return true;
}

/** change ISO baud rate of card slot
 *  @param[in] slotnr slot number for which the baud rate should be set
 *  @param[in] clkdiv can clock divider
 *  @param[in] f clock rate conversion integer F
 *  @param[in] d baud rate adjustment factor D
 *  @return if the baud rate has been set, else a parameter is out of range
 */
static bool slot_set_isorate(struct card_uart *cuart, enum ncn8025_sim_clkdiv clkdiv, uint16_t f, uint8_t d)
{
	uint8_t slotnr = cuart->u.asf4.slot_nr;
	struct usart_async_descriptor* slot = SIM_peripheral_descriptors[slotnr];

	// input checks
	ASSERT(slotnr < ARRAY_SIZE(SIM_peripheral_descriptors));
	if (clkdiv != SIM_CLKDIV_1 && clkdiv != SIM_CLKDIV_2 && clkdiv != SIM_CLKDIV_4 && clkdiv != SIM_CLKDIV_8) {
		return false;
	}
	if (!iso7816_3_valid_f(f)) {
		return false;
	}
	if (!iso7816_3_valid_d(d)) {
		return false;
	}

	// set clockdiv
	struct ncn8025_settings settings;
	ncn8025_get(slotnr, &settings);
	if (settings.clkdiv != clkdiv) {
		settings.clkdiv = clkdiv;
		ncn8025_set(slotnr, &settings);
	}

	// calculate desired frequency
	uint32_t freq = 20000000UL; // maximum frequency
	switch (clkdiv) {
	case SIM_CLKDIV_1:
		freq /= 1;
		break;
	case SIM_CLKDIV_2:
		freq /= 2;
		break;
	case SIM_CLKDIV_4:
		freq /= 4;
		break;
	case SIM_CLKDIV_8:
		freq /= 8;
		break;
	}

	/* error interrupt off after reset due to possbile inverted atr and accompanying parity error
	 * this was automatically enabled during error callback registration */
	hri_sercomusart_write_INTEN_ERROR_bit(slot->device.hw, 0);

	set_inverted_signalling(slot->device.hw, false);

	// set baud rate
	uint32_t baudrate = (freq * d) / f; // calculate actual baud rate
	return slot_set_baudrate(cuart, baudrate); // set baud rate
}

/***********************************************************************
 * Interface with card_uart (cuart) core
 ***********************************************************************/

/* forward-declaration */
static struct card_uart_driver asf4_usart_driver;
static int asf4_usart_close(struct card_uart *cuart);

static int asf4_usart_open(struct card_uart *cuart, const char *device_name)
{
	struct usart_async_descriptor *usa_pd;
	int slot_nr = atoi(device_name);

	if (slot_nr >= ARRAY_SIZE(SIM_peripheral_descriptors))
		return -ENODEV;
	usa_pd = SIM_peripheral_descriptors[slot_nr];
	if (!usa_pd)
		return -ENODEV;

	cuart->u.asf4.usa_pd = usa_pd;
	cuart->u.asf4.slot_nr = slot_nr;

	usart_async_register_callback(usa_pd, USART_ASYNC_RXC_CB, SIM_rx_cb[slot_nr]);
	usart_async_register_callback(usa_pd, USART_ASYNC_TXC_CB, SIM_tx_cb[slot_nr]);
	usart_async_register_callback(usa_pd, USART_ASYNC_ERROR_CB, _SIM_error_cb);
	usart_async_enable(usa_pd);

	// set USART baud rate to match the interface (f = 2.5 MHz) and card default settings (Fd = 372, Dd = 1)
	slot_set_isorate(cuart, SIM_CLKDIV_8, ISO7816_3_DEFAULT_FD, ISO7816_3_DEFAULT_DD);

        return 0;
}

static int asf4_usart_close(struct card_uart *cuart)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);

	usart_async_disable(usa_pd);

	return 0;
}

static int asf4_usart_async_tx(struct card_uart *cuart, const uint8_t *data, size_t len)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;
	int rc;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);
	OSMO_ASSERT(usart_async_is_tx_empty(usa_pd));

	rc = io_write(&usa_pd->io, data, len);
	if (rc < 0)
		return rc;

	cuart->tx_busy = true;

	return rc;
}

static int asf4_usart_async_rx(struct card_uart *cuart, uint8_t *data, size_t len)
{
	struct usart_async_descriptor *usa_pd = cuart->u.asf4.usa_pd;

	OSMO_ASSERT(cuart->driver == &asf4_usart_driver);

	return io_read(&usa_pd->io, data, len);
}

#include "ccid_device.h"
#include "iso7816_3.h"
static int asf4_usart_ctrl(struct card_uart *cuart, enum card_uart_ctl ctl, int arg)
{
	struct ncn8025_settings settings;
	Sercom *sercom = cuart->u.asf4.usa_pd->device.hw;

	switch (ctl) {
	case CUART_CTL_NO_RXTX:
		/* no op */
		break;
	case CUART_CTL_RX:
		if (arg){
			/* no op */
		} else {
			delay_us(cuart->u.asf4.extrawait_after_rx);
		}
		break;
	case CUART_CTL_RST:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		settings.rstin = arg ? true : false;
		ncn8025_set(cuart->u.asf4.slot_nr, &settings);
		usart_async_flush_rx_buffer(cuart->u.asf4.usa_pd);

		/* reset everything, card reset resets pps params */
		if (arg)
			slot_set_isorate(cuart, SIM_CLKDIV_8, ISO7816_3_DEFAULT_FD, ISO7816_3_DEFAULT_DD);

		break;

	case CUART_CTL_POWER_5V0:
	case CUART_CTL_POWER_3V0:
	case CUART_CTL_POWER_1V8:
		/* reset everything */
		slot_set_isorate(cuart, SIM_CLKDIV_8, ISO7816_3_DEFAULT_FD, ISO7816_3_DEFAULT_DD);


		enum ncn8025_sim_voltage v = CUART_CTL_POWER_5V0;
		switch (ctl) {
			case CUART_CTL_POWER_5V0: v = SIM_VOLT_5V0; break;
			case CUART_CTL_POWER_3V0: v = SIM_VOLT_3V0; break;
			case CUART_CTL_POWER_1V8: v = SIM_VOLT_1V8; break;
			default: break;
		}

		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		settings.cmdvcc = arg ? true : false;
		settings.led = arg ? true : false;
		settings.vsel = v;
		ncn8025_set(cuart->u.asf4.slot_nr, &settings);

		break;
	case CUART_CTL_WTIME:
		/* no driver-specific handling of this */
		break;
	case CUART_CTL_CLOCK:
		/* no clock stop support */
		break;
	case CUART_CTL_SET_CLOCK_FREQ:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);

		/* 2,5/5/10/20 supported by dividers */
		enum ncn8025_sim_clkdiv clkdiv = SIM_CLKDIV_1;
		if(arg < 20000000)
			clkdiv = SIM_CLKDIV_2;
		if(arg < 10000000)
			clkdiv = SIM_CLKDIV_4;
		if(arg < 5000000)
			clkdiv = SIM_CLKDIV_8;
		settings.clkdiv = clkdiv;
		ncn8025_set(cuart->u.asf4.slot_nr, &settings);
		break;
	case CUART_CTL_SET_FD:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		uint8_t divider = ncn8025_div_val[settings.clkdiv];
		uint32_t baudrate = (20e6/divider)/arg;
		slot_set_baudrate(cuart, baudrate);
		break;
	case CUART_CTL_GET_BAUDRATE:
		return cuart->u.asf4.current_baudrate;
		break;
	case CUART_CTL_GET_CLOCK_FREQ:
		ncn8025_get(cuart->u.asf4.slot_nr, &settings);
		return 20e6 / ncn8025_div_val[settings.clkdiv];
		break;
	case CUART_CTL_ERROR_AND_INV:
		set_inverted_signalling(sercom, arg);

		/* clear pending errors that happened while the interrupt was off (ATR) and enable it*/
		hri_sercomusart_clear_interrupt_ERROR_bit(sercom);
		hri_sercomusart_clear_STATUS_reg(sercom, 0xff);
		volatile uint8_t dummy = hri_sercomusart_read_RXERRCNT_reg(sercom);
		hri_sercomusart_set_INTEN_ERROR_bit(sercom);
		break;
	default:
		return 0;
	}
	return 0;
}

static const struct card_uart_ops asf4_usart_ops = {
	.open = asf4_usart_open,
	.close = asf4_usart_close,
	.async_tx = asf4_usart_async_tx,
	.async_rx = asf4_usart_async_rx,
	.ctrl = asf4_usart_ctrl,
};

static struct card_uart_driver asf4_usart_driver = {
	.name = "asf4",
	.ops = &asf4_usart_ops,
};

static __attribute__((constructor)) void on_dso_load_cuart_asf4(void)
{
	card_uart_driver_register(&asf4_usart_driver);
}
