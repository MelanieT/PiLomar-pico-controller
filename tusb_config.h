// SPDX-FileCopyrightText: 2024 Melanie Thielker & Leonie Gaertner
// SPDX-License-Identifier: BSD-3-Clause

#if !defined(_TUSB_CONFIG_H_)
#define _TUSB_CONFIG_H_

#include <tusb_option.h>

#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

#define CFG_TUD_ENDPOINT0_SIZE    64

#define CFG_TUD_CDC               2
#define CFG_TUD_NET               1
#define CFG_TUD_CDC_RX_BUFSIZE 1024
#define CFG_TUD_CDC_TX_BUFSIZE 1024

#define CFG_TUD_ECM_RNDIS     1

void usbd_serial_init(void);

#endif /* _TUSB_CONFIG_H_ */

