#ifndef ATMEL_START_H_INCLUDED
#define ATMEL_START_H_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

#include <dma_memory.h>

#include "driver_init.h"
#include "usb_start.h"
#include "stdio_start.h"

/** flag set when the memory to memory DMA is complete */
extern volatile bool dma_m2m_complete_flag;

/**
 * Initializes MCU, drivers and middleware in the project
 **/
void atmel_start_init(void);

#ifdef __cplusplus
}
#endif
#endif
