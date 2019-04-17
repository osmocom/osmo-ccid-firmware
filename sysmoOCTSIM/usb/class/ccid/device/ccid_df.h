#pragma once

#include "usbdc.h"

enum ccid_df_cb_type {
	CCID_DF_CB_READ_OUT,
	CCID_DF_CB_WRITE_IN,
	CCID_DF_CB_WRITE_IRQ,
};

int32_t ccid_df_init(void);
void ccid_df_deinit(void);
int32_t ccid_df_read_out(uint8_t *buf, uint32_t size);
int32_t ccid_df_write_in(uint8_t *buf, uint32_t size);
int32_t ccid_df_write_irq(uint8_t *buf, uint32_t size);
int32_t ccid_df_register_callback(enum ccid_df_cb_type cb_type, FUNC_PTR ptr);
bool ccid_df_is_enabled(void);
