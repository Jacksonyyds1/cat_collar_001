#ifndef __BLE_PROFILE_H__
#define __BLE_PROFILE_H__

#include <stdint.h>

//!UUID size
#define  UUID_16BYTES_SIZE  16

//! attribute properties
#define RSI_BLE_ATT_PROPERTY_READ         0x02
#define RSI_BLE_ATT_PROPERTY_WRITE        0x08
#define RSI_BLE_ATT_PROPERTY_NOTIFY       0x10

// max data length
#define RSI_BLE_MAX_DATA_LEN  230
// #define RSI_BLE_CAT_MAX_DATA_LEN  230
// #define RSI_BLE_PROV_MAX_DATA_LEN   200

#define BD_ADDR_STRING_SIZE   18


/////////////////////////////////////////////////////////////
extern uint16_t mobile_to_device_handle, device_to_mobile_handle, notify_val_handle;
extern uint32_t rsi_ble_add_prov_serv(void);
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
#define TA_FW_UP 0
#define M4_FW_UP 1

#define FW_UPGRADE_TYPE  M4_FW_UP

extern uint16_t ota_fw_tf_handle, ota_fw_control_handle;
extern void rsi_ble_add_ota_fwup_serv(void);
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
extern uint16_t cat_tx_handle, cat_rx_handle, cat_notify_handle;
void rsi_ble_add_catcollar_serv(void);
//////////////////////////////////////////////////////////////

#endif //__BLE_PROFILE_H__

