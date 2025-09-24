// //! SL Wi-Fi SDK includes
// #include "sl_board_configuration.h"
// #include "sl_constants.h"
// #include "sl_wifi.h"
// #include "sl_net_ip_types.h"
// #include "cmsis_os2.h"
// #include "sl_utility.h"

// BLE include file to refer BLE APIs
#include <rsi_ble_apis.h>
#include <rsi_bt_common_apis.h>
#include <rsi_common_apis.h>
#include <string.h>

#include "ble_config.h"

#include "app_log.h"
#include "ble_profile.h"
#include "ble_app_cat.h"
#include "ble_data_parse.h"
#include "chekr_dash.h"
#include "blinky.h"
#include "common.h"

static catcollar_bt_connection_state_t catcollar_bt_connection_state = 0;
int catcollar_bt_connection_get_state(void)
{
  return catcollar_bt_connection_state;
}

void catcollar_bt_connection_set_state(catcollar_bt_connection_state_t state)
{
  catcollar_bt_connection_state = state;
  if (state == CATCOLLAR_BT_DISCONNECTED) {
    leds_play(BLUE_LED, LEDS_SLOW_BLINK);
  } else if (state == CATCOLLAR_BT_CONNECTED) {
    leds_play(BLUE_LED, LEDS_ON);
  } else {
    app_log_error("Unknown connection state: %d\r\n", state);
    leds_play(BLUE_LED, LEDS_OFF);
  }
}

static char local_name_str[16];
rsi_bt_resp_get_local_name_t local_bt_name;
char *ble_get_local_name(void)
{
  rsi_bt_get_local_name(&local_bt_name);
  if (local_bt_name.name_len > 0) {
    memcpy(local_name_str, local_bt_name.name, local_bt_name.name_len);
    local_name_str[local_bt_name.name_len] = '\0'; // Null-terminate the string
    return local_name_str;
  } else {
    return "Unknown";
  }
}

static read_response_t read_response;
// send notification
void write_to_central(uint8_t *data, int len)
{
	if (catcollar_bt_connection_get_state() == CATCOLLAR_BT_CONNECTED) {
    app_log_info("----SEND---->>");
    // app_log_info("HEX Data:\n");
    app_log_hexdump_info(data, len);
    memcpy(read_response.response, data, len);
    read_response.response_len = len;
    rsi_ble_set_local_att_value(cat_rx_handle, read_response.response_len, read_response.response);
	} else {
		app_log_error("BLE disconnected\r\n");
	}
}

int chekr_service_init(void)
{
	// create ChekrAppLink service
	dashboard_init();

  // ///test dashboard
  // dashboard_ctrl(DASH_START, 3);
	return 0;
}

