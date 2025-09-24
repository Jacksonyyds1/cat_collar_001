#ifndef BLE_APP_CAT_H
#define BLE_APP_CAT_H

#include <stdint.h>
#include <stdbool.h>
#include "ble_data_parse.h"

typedef enum {
  CATCOLLAR_BT_DISCONNECTED = 0,
  CATCOLLAR_BT_CONNECTED
} catcollar_bt_connection_state_t;

typedef struct {
	uint8_t response[MAX_FRAME_LEN];
	uint8_t response_len;
} read_response_t;

int catcollar_bt_connection_get_state(void);
void catcollar_bt_connection_set_state(catcollar_bt_connection_state_t state);
char *ble_get_local_name(void);
void write_to_central(uint8_t *data, int len);
int chekr_service_init(void);

#endif // BLE_APP_CAT_H