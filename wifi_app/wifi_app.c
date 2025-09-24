//! SL Wi-Fi SDK includes
#include "sl_net.h"
#include "sl_net_si91x.h"
#include "sl_constants.h"
#include "sl_wifi.h"
#include "sl_wifi_callback_framework.h"
#include "sl_utility.h"

#include "cmsis_os2.h"
#include <string.h>

#include "wifi_config.h"

#include "app_log.h"
#include "blinky.h"
#include "common.h"
#include "wifi_app.h"

static osSemaphoreId_t wlan_thread_sem;

#define DHCP_HOST_NAME    "PetCat"
#define TIMEOUT_MS        5000

static uint32_t wlan_app_event_map;
static volatile bool join_success_flag = false;
static uint8_t wifi_connect_rssi_value = 0;

/*==============================================*/
/**
 * @fn         wifi_app_set_event
 * @brief      sets the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to set/raise the specific event.
 */
void wifi_app_set_event(wifi_event_t event_num)
{
  wlan_app_event_map |= BIT(event_num);

  osSemaphoreRelease(wlan_thread_sem);

  return;
}

/*==============================================*/
/**
 * @fn         wifi_app_clear_event
 * @brief      clears the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to clear the specific event.
 */
static void wifi_app_clear_event(uint32_t event_num)
{
  wlan_app_event_map &= ~BIT(event_num);
  return;
}

/*==============================================*/
/**
 * @fn         wifi_app_get_event
 * @brief      returns the first set event based on priority
 * @param[in]  none.
 * @return     int32_t
 *             > 0  = event number
 *             -1   = not received any event
 * @section description
 * This function returns the highest priority event among all the set events
 */
static int32_t wifi_app_get_event(void)
{
  uint32_t ix;

  for (ix = 0; ix < 32; ix++) {
    if (wlan_app_event_map & (1 << ix)) {
      return ix;
    }
  }

  return (-1);
}

sl_status_t wifi_stats_response_callback(sl_wifi_event_t event, void *data, uint32_t data_length, void *arg)
{
    UNUSED_PARAMETER(event);
    UNUSED_PARAMETER(arg);
    UNUSED_PARAMETER(data_length);
    sl_si91x_module_state_stats_response_t *notif = (sl_si91x_module_state_stats_response_t *)data;

    // app_log_info("\r\nSL_WIFI_STATS_RESPONSE_EVENTS Module status handler event with length : %lu\r\n", data_length);
    // app_log_info("event: 0x%lX, data_length: %lu\r\n", event, data_length);
    // app_log_info("Timestamp : %lu, state_code : 0x%x, reason_code : 0x%x, channel : %u, rssi : %u.\r\n",
    //      notif->timestamp,
    //      notif->state_code,
    //      notif->reason_code,
    //      notif->channel,
    //      notif->rssi);

    app_log_info("BSSID : %x:%x:%x:%x:%x:%x.\r\n, rssi : %u.\r\n",
         notif->bssid[0],
         notif->bssid[1],
         notif->bssid[2],
         notif->bssid[3],
         notif->bssid[4],
         notif->bssid[5], 
         notif->rssi);

    if(notif->state_code == 0x90){
        app_log_info("station code is 0x90, going to Disconnect state \r\n");
        wifi_app_set_event(WIFI_EVENT_STA_DISCONNECT);
        wifi_connect_rssi_value = 0;
    }
    if (notif->state_code == 0x80){
        app_log_info("station code is 0x80, going to Connected state \r\n");
        wifi_connect_rssi_value = notif->rssi;
        // wifi_app_set_event(WIFI_EVENT_STA_CONNECTED);
    }
    
    return SL_STATUS_OK;
}

// rejoin failure callback handler in station mode
sl_status_t join_callback_handler(sl_wifi_event_t event, void *result, uint32_t result_length, void *arg)
{
    UNUSED_PARAMETER(result);
    UNUSED_PARAMETER(arg);
    if (SL_WIFI_CHECK_IF_EVENT_FAILED(event))
    {
      app_log_error("F: Join Event received with %lu bytes payload\n", result_length);
      // callback_status = *(sl_status_t *)result;
      return SL_STATUS_FAIL;
    }

    // app_log_info("0x%lX: Join Event received with %lu bytes payload\n", *(sl_status_t *)result, result_length);
    app_log_info("Status join is OK\r\n");
    join_success_flag = true;
    // callback_status_join = SL_STATUS_OK;
    return SL_STATUS_OK;
}

static sl_status_t wifi_client_configure_ip_address()
{
    sl_status_t status;
    sl_net_ip_configuration_t ip_address  = { 0 };
    ip_address.type      = SL_IPV4;
    ip_address.mode      = SL_IP_MANAGEMENT_DHCP;
    ip_address.host_name = DHCP_HOST_NAME;
    // Configure IP
    status = sl_si91x_configure_ip_address(&ip_address, SL_SI91X_WIFI_CLIENT_VAP_ID);
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to configure IP address: 0x%lx\r\n", status);
        return status;
    }

#if defined(SL_SI91X_PRINT_DBG_LOG)
    sl_ip_address_t ip = { 0 };
    ip.type            = ip_address.type;
    ip.ip.v4.value     = ip_address.ip.v4.ip_address.value;
    print_sl_ip_address(&ip);
#endif

    return SL_STATUS_OK;
}

static sl_status_t wifi_client_connect(char *ssid, char *password, sl_wifi_security_t security_type)
{
  sl_status_t status = RSI_SUCCESS;
  sl_wifi_client_configuration_t client_access_point = { 0 };
  sl_wifi_credential_id_t id = SL_NET_DEFAULT_WIFI_CLIENT_CREDENTIAL_ID;

  sl_wifi_scan_configuration_t wifi_scan_configuration = { 0 };
  wifi_scan_configuration                              = default_wifi_scan_configuration;
  status = sl_wifi_start_scan(SL_WIFI_CLIENT_2_4GHZ_INTERFACE, NULL, &wifi_scan_configuration);

  if (security_type != SL_WIFI_OPEN) {
    status = sl_net_set_credential(id, SL_NET_WIFI_PSK, password, strlen(password));
    if (status != SL_STATUS_OK) {
        app_log_error("Failed to set client credentials: 0x%lx\r\n", status);
        return status;
    }
  }
  client_access_point.ssid.length = strlen(ssid);
  memcpy(client_access_point.ssid.value, ssid, client_access_point.ssid.length);
  client_access_point.security      = security_type;
  client_access_point.encryption    = SL_WIFI_DEFAULT_ENCRYPTION;
  client_access_point.credential_id = id;

  app_log_info("SSID=%s\r\n", client_access_point.ssid.value);
  app_log_info("PWD=%s\r\n", password);

  status = sl_wifi_connect(SL_WIFI_CLIENT_2_4GHZ_INTERFACE, &client_access_point, 0);

  if (SL_STATUS_IN_PROGRESS == status) {
    const uint32_t start = osKernelGetTickCount();

    while (!join_success_flag && (osKernelGetTickCount() - start) <= TIMEOUT_MS) {
        osThreadYield();
    }
    status = join_success_flag ? SL_STATUS_OK : SL_STATUS_TIMEOUT;
  }
  join_success_flag = false;

  if (status != RSI_SUCCESS) {
    app_log_error("WLAN Connect Failed, Error Code : 0x%lX\r\n", status);
    return status;
  } else {
    status = wifi_client_configure_ip_address();
    if (status != RSI_SUCCESS) {
      app_log_error("IP Config Failed, Error Code : 0x%lX\r\n", status);
      return status;
    } else {
      app_log_info("\n WLAN IP Config is successful!\r\n");
    }
  }
  return status;
}

void wifi_app_task()
{
  int32_t status   = RSI_SUCCESS;
  int32_t event_id = 0;

  uint8_t retry_count = 0;

  wlan_thread_sem = osSemaphoreNew(1, 0, NULL);
  if (wlan_thread_sem == NULL) {
    app_log_error("Failed to create wlan_thread_sem\n");
    return;
  }

  while (1) {
    // checking for events list
    event_id = wifi_app_get_event();
    if (event_id == -1) {
      osSemaphoreAcquire(wlan_thread_sem, osWaitForever);

      // if events are not received loop will be continued.
      continue;
    }

    switch (event_id) {
      case WIFI_EVENT_INITIAL: {
        wifi_app_clear_event(WIFI_EVENT_INITIAL);
        app_log_info("|WIFI App Initial State\r\n");

        retry_count = 0;

        sl_wifi_set_callback(SL_WIFI_JOIN_EVENTS, join_callback_handler, NULL);
        sl_wifi_set_callback(SL_WIFI_STATS_RESPONSE_EVENTS, wifi_stats_response_callback, NULL);

        wifi_app_set_event(WIFI_EVENT_STA_JOIN);
      } break;

      case WIFI_EVENT_STA_JOIN: {
        wifi_app_clear_event(WIFI_EVENT_STA_JOIN);
        app_log_info("|WIFI App Join State\r\n");

        status = wifi_client_connect(device_access_point_info.ssid, device_access_point_info.pwd, SL_WIFI_WPA2);
        if (status != RSI_SUCCESS) {
          app_log_error("\r\n|WLAN Connect Failed, Error Code : 0x%lX\r\n", status);
          retry_count++;
          if (retry_count < 3) {
            app_log_info("|Retrying connection (%d/3)...\r\n", retry_count);
            osDelay(2000); // 等待2秒后重试
            wifi_app_set_event(WIFI_EVENT_STA_JOIN);
          } else {
            app_log_error("|Retrying connection (%d/3)...\r\n", retry_count);
            app_log_error("|Max retry attempts reached. Connection failed.\r\n");
            retry_count = 0;
            status = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
            if (status == RSI_SUCCESS) {
              wifi_app_set_event(WIFI_EVENT_ERROR);
            }
          }
        } else {
            // update wlan application state
            retry_count = 0;
            wifi_app_set_event(WIFI_EVENT_NETWORK_STACK_DHCP_SUCC);
        }
        
        osSemaphoreRelease(wlan_thread_sem);
      } break;

      case WIFI_EVENT_STA_RECONNECT: {
        wifi_app_clear_event(WIFI_EVENT_STA_RECONNECT);
        app_log_info("|WIFI App Reconnect State\r\n");

        update_wifi_cfg_from_syscfg();

        wifi_connection_state_t state = wifi_app_get_connection_state();
        if (state == WIFI_CONNECTED) {
          wifi_internal_state = WIFI_STATE_RECONNECT_PENDING;
          status = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
          if (status != RSI_SUCCESS) {
            app_log_error("|WIFI Disconnect Failed, Error Code : 0x%lX\r\n", status);
            wifi_app_set_event(WIFI_EVENT_ERROR);
          } else {
            app_log_info("|WIFI Disconnect Successful\r\n");
            wifi_app_set_event(WIFI_EVENT_STA_DISCONNECT);
          }
        }else {
          // 没有连接，直接走 join
          wifi_app_set_event(WIFI_EVENT_STA_JOIN);
        }
      } break;

      case WIFI_EVENT_NETWORK_STACK_DHCP_SUCC: {
        wifi_app_clear_event(WIFI_EVENT_NETWORK_STACK_DHCP_SUCC);
        app_log_info("\r\n |WLAN connection is successful\r\n");

        wifi_app_set_connection_state(WIFI_CONNECTED);
        wifi_internal_state = WIFI_STATE_CONNECTED;

        osSemaphoreRelease(wlan_thread_sem);
      } break;

      case WIFI_EVENT_ERROR: {
        wifi_app_clear_event(WIFI_EVENT_ERROR);
        app_log_info("|WIFI App Error State\r\n");

        // Any additional code if required
        wifi_app_set_connection_state(WIFI_DISCONNECTED);

        osSemaphoreRelease(wlan_thread_sem);
      } break;

      case WIFI_EVENT_STA_DISCONNECT: {
        wifi_app_clear_event(WIFI_EVENT_STA_DISCONNECT);
        app_log_info("|WIFI App Disconnected State\r\n");

        wifi_app_set_connection_state(WIFI_DISCONNECTED);

        // wifi_app_set_event(WIFI_APP_FLASH_STATE);
        if (wifi_internal_state == WIFI_STATE_RECONNECT_PENDING) {
          app_log_info("Proceeding with reconnect...\r\n");
          wifi_internal_state = WIFI_STATE_CONNECTING;
          wifi_app_set_event(WIFI_EVENT_STA_JOIN);
        } else {
          wifi_internal_state = WIFI_STATE_IDLE;
        }

        osSemaphoreRelease(wlan_thread_sem);
      } break;

//       case WIFI_EVENT_STA_DISCONNECT_NOTIFY: {
//         wifi_app_clear_event(WIFI_EVENT_STA_DISCONNECT_NOTIFY);
//         app_log_info("|WIFI App Disconnect Notify State\r\n");

//         status = sl_wifi_disconnect(SL_WIFI_CLIENT_INTERFACE);
//         if (status == RSI_SUCCESS) {
// #if RSI_WISE_MCU_ENABLE
//           rsi_flash_erase((uint32_t)FLASH_ADDR_TO_STORE_AP_DETAILS);
// #endif
//           app_log_info("\r\nWLAN Disconnected\r\n");
//           // wifi_app_send_to_ble(WIFI_APP_DISCONNECTION_NOTIFY, (uint8_t *)&disassosiated, 1);
//           wifi_app_set_event(WIFI_EVENT_STA_DISCONNECT);
//         } else {
//           app_log_error("\r\nWIFI Disconnect Failed, Error Code : 0x%lX\r\n", status);
//         }

//         osSemaphoreRelease(wlan_thread_sem);
//       } break;
      default:
        break;
    }
  }
}

int32_t wifi_app_get_rssi()
{
  int32_t wifi_connect_rssi;
  if (wifi_app_get_connection_state() == WIFI_CONNECTED){
    // sl_wifi_get_signal_strength(SL_WIFI_CLIENT_INTERFACE, &wifi_connect_rssi);
    wifi_connect_rssi = (int32_t)wifi_connect_rssi_value;
  }else{
    wifi_connect_rssi = 0;
  }
  return wifi_connect_rssi;
}

bsp_wifi_info_t device_access_point_info = {
  .ssid = "cooper",
  .pwd = "12348888",
};

static wifi_connection_state_t wifi_connection_state = 0;
wifi_connection_state_t wifi_app_get_connection_state(void)
{
  return wifi_connection_state;
}

void wifi_app_set_connection_state(wifi_connection_state_t state)
{
  wifi_connection_state = state;
  if (state == WIFI_DISCONNECTED) {
    leds_play(GREEN_LED, LEDS_SLOW_BLINK);
  } else if (state == WIFI_CONNECTED) {
    leds_play(GREEN_LED, LEDS_ON);
  } else {
    app_log_error("Unknown connection state: %d\r\n", state);
    leds_play(GREEN_LED, LEDS_OFF);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////
const osThreadAttr_t wifi_thread_attributes = {
  .name       = "wifi_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 2048,
  .priority   = osPriorityNormal,
  .tz_module  = 0,
  .reserved   = 0,
};

int rsi_wifi_task_init()
{
    sl_status_t status = SL_STATUS_OK;
    sl_mac_address_t mac_addr          = { 0 };

    //! Wi-Fi initialization
    status = sl_wifi_init(&cat_wifi_client_config, NULL, sl_wifi_default_event_handler);
    // status = sl_wifi_init(&station_init_configuration, NULL, sl_wifi_default_event_handler);
    // status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, &cat_wifi_config, NULL, sl_wifi_default_event_handler);
    if (status != SL_STATUS_OK) {
        app_log_error("\r\nWi-Fi Initialization Failed, Error Code : 0x%lX\r\n", status);
        return -1;
    }

    status = sl_wifi_get_mac_address(SL_WIFI_CLIENT_INTERFACE, &mac_addr);
    if (status == SL_STATUS_OK) {
      printf("Device MAC address: %x:%x:%x:%x:%x:%x\r\n",
             mac_addr.octet[0],
             mac_addr.octet[1],
             mac_addr.octet[2],
             mac_addr.octet[3],
             mac_addr.octet[4],
             mac_addr.octet[5]);
    } else {
      app_log_error("Failed to get mac address: 0x%lx\r\n", status);
    }

    if (osThreadNew((osThreadFunc_t)wifi_app_task, NULL, &wifi_thread_attributes) == NULL) {
        app_log_error("Failed to create wifi thread\r\n");
        return -1;
    }

    printf("\r\n Wi-Fi initialization is successful\r\n");

    wifi_app_set_event(WIFI_EVENT_INITIAL);
    
    return 0;
}

