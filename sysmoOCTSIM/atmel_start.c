#include <atmel_start.h>

/** Memory to memory DMA callback */
static void M2M_DMA_complete_cb(void)
{
	dma_m2m_complete_flag = true;
}

/**
 * Initializes MCU, drivers and middleware in the project
 **/
void atmel_start_init(void)
{
	system_init();
	dma_memory_init();
	dma_memory_register_callback(DMA_MEMORY_COMPLETE_CB, M2M_DMA_complete_cb);
	stdio_redirect_init();
	usb_init();
}
