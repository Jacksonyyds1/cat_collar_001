/*******************************************************************************
* @file  ble_app.c
 * @brief : This file contains example application for WiFi Station BLE
 * Provisioning
 * @section Description :
 * This application explains how to get the WLAN connection functionality using
 * BLE provisioning.
 * Silicon Labs Module starts advertising and with BLE Provisioning the Access Point
 * details are fetched.
 * Silicon Labs device is configured as a WiFi station and connects to an Access Point.
 =================================================================================*/

//! SL Wi-Fi SDK includes
#include "sl_board_configuration.h"
#include "sl_constants.h"
#include "sl_wifi.h"
#include "sl_net_ip_types.h"
#include "cmsis_os2.h"
#include "sl_utility.h"

// BLE include file to refer BLE APIs
#include <rsi_ble_apis.h>
#include <rsi_bt_common_apis.h>
#include <rsi_common_apis.h>
#include <string.h>

#include "ble_config.h"
#include "wifi_config.h"
#include "ble_prov.h"

#include "app_log.h"
#include "ble_app_cat.h"
#include "ble_ota.h"
#include "ble_data_parse.h"
#include "blinky.h"
#include "stdio.h"
#include "ble_profile.h"

// #define RSI_BLE_PROVISIONING_ENABLED

#define RSI_BLE_CONN_EVENT 					        0x01
#define RSI_BLE_MTU_EX_EVENT 				        0x02
#define RSI_BLE_PHY_UPDATE_EVENT 			      0x03
#define RSI_BLE_CONN_UPDATE_EVENT 			    0x04
#define RSI_BLE_DISCONN_EVENT 				      0x05
#define RSI_BLE_GATT_WRITE_EVENT        	  0x06
#define RSI_BLE_RECEIVE_REMOTE_FEATURES 	  0x07
#define RSI_APP_EVENT_DATA_LENGTH_CHANGE	  0x08
#define RSI_BLE_MORE_DATA_REQ_EVENT 		    0x09

// global parameters list
static osSemaphoreId_t ble_thread_sem;

static uint32_t ble_app_event_mask;
static uint32_t ble_app_event_map;
static uint32_t ble_app_event_map1;
rsi_ble_event_conn_status_t conn_event_to_app;


uint8_t remote_dev_addr[18] = { 0 };

static rsi_ble_event_remote_features_t remote_dev_feature;




/******************************************************
 *               Function Declarations
 ******************************************************/
void rsi_ble_on_enhance_conn_status_event(rsi_ble_event_enhance_conn_status_t *resp_enh_conn);
void rsi_ble_configurator_init(void);
void rsi_ble_configurator_task(void *argument);

/*==============================================*/
/**
 * @fn         rsi_ble_app_init_events
 * @brief      initializes the event parameter.
 * @param[in]  none.
 * @return     none.
 * @section description
 * This function is used during BLE initialization.
 */
static void rsi_ble_app_init_events() {
  ble_app_event_map = 0;
  ble_app_event_mask = 0xFFFFFFFF;
  ble_app_event_mask = ble_app_event_mask; // To suppress warning while
                                           // compiling
  return;
}
/*==============================================*/
/**
 * @fn         rsi_ble_app_set_event
 * @brief      sets the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to set/raise the specific event.
 */
static void rsi_ble_app_set_event(uint32_t event_num)
{
  if (event_num < 32) {
    ble_app_event_map |= BIT(event_num);
  } else {
    ble_app_event_map1 |= BIT((event_num - 32));
  }

  if (ble_thread_sem != NULL) {
    osSemaphoreRelease(ble_thread_sem);
  }

  return;
}

/*==============================================*/
/**
 * @fn         rsi_ble_app_clear_event
 * @brief      clears the specific event.
 * @param[in]  event_num, specific event number.
 * @return     none.
 * @section description
 * This function is used to clear the specific event.
 */
static void rsi_ble_app_clear_event(uint32_t event_num)
{
  if (event_num < 32) {
    ble_app_event_map &= ~BIT(event_num);
  } else {
    ble_app_event_map1 &= ~BIT((event_num - 32));
  }

  return;
}

/*==============================================*/
/**
 * @fn         rsi_ble_app_get_event
 * @brief      returns the first set event based on priority
 * @param[in]  none.
 * @return     int32_t
 *             > 0  = event number
 *             -1   = not received any event
 * @section description
 * This function returns the highest priority event among all the set events
 */
static int32_t rsi_ble_app_get_event(void)
{
  uint32_t ix;

  for (ix = 0; ix < 64; ix++) {
    if (ix < 32) {
      if (ble_app_event_map & (1 << ix)) {
        return ix;
      }
    } else {
      if (ble_app_event_map1 & (1 << (ix - 32))) {
        return ix;
      }
    }
  }

  return (-1);
}

/*==============================================*/
/**
 * @fn         rsi_ble_on_enhance_conn_status_event
 * @brief      invoked when enhanced connection complete event is received
 * @param[out] resp_enh_conn, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the status of the connection
 */
void rsi_ble_on_enhance_conn_status_event(rsi_ble_event_enhance_conn_status_t *resp_enh_conn)
{
  conn_event_to_app.dev_addr_type = resp_enh_conn->dev_addr_type;
  memcpy(conn_event_to_app.dev_addr, resp_enh_conn->dev_addr, RSI_DEV_ADDR_LEN);

  conn_event_to_app.status = resp_enh_conn->status;
  rsi_ble_app_set_event(RSI_BLE_CONN_EVENT);
}

/*==============================================*/
/**
 * @fn         rsi_ble_on_connect_event
 * @brief      invoked when connection complete event is received
 * @param[out] resp_conn, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the status of the connection
 */
static void rsi_ble_on_connect_event(rsi_ble_event_conn_status_t *resp_conn)
{
  memcpy(&conn_event_to_app, resp_conn, sizeof(rsi_ble_event_conn_status_t));

  rsi_ble_app_set_event(RSI_BLE_CONN_EVENT);
}

/*==============================================*/
/**
 * @fn         rsi_ble_on_disconnect_event
 * @brief      invoked when disconnection event is received
 * @param[out]  resp_disconnect, disconnected remote device information
 * @param[out]  reason, reason for disconnection.
 * @return     none.
 * @section description
 * This Callback function indicates disconnected device information and status
 */
static void rsi_ble_on_disconnect_event(rsi_ble_event_disconnect_t *resp_disconnect, uint16_t reason)
{
  UNUSED_PARAMETER(reason);
  rsi_ble_event_disconnect_t disconn_event_to_app = { 0 };
  memcpy(&disconn_event_to_app, resp_disconnect, sizeof(rsi_ble_event_disconnect_t));
  rsi_ble_app_set_event(RSI_BLE_DISCONN_EVENT);
}

/*==============================================*/
/**
 * @fn         rsi_ble_on_conn_update_complete_event
 * @brief      invoked when conn update complete event is received
 * @param[out] rsi_ble_event_conn_update_complete contains the controller
 * support conn information.
 * @param[out] resp_status contains the response status (Success or Error code)
 * @return     none.
 * @section description
 * This Callback function indicates the conn update complete event is received
 */
void rsi_ble_on_conn_update_complete_event(rsi_ble_event_conn_update_t *rsi_ble_event_conn_update_complete,
                                           uint16_t resp_status)
{
  UNUSED_PARAMETER(resp_status);
  rsi_ble_event_conn_update_t event_conn_update_complete = { 0 };
  rsi_6byte_dev_address_to_ascii(remote_dev_addr, (uint8_t *)rsi_ble_event_conn_update_complete->dev_addr);
  memcpy(&event_conn_update_complete, rsi_ble_event_conn_update_complete, sizeof(rsi_ble_event_conn_update_t));
  rsi_ble_app_set_event(RSI_BLE_CONN_UPDATE_EVENT);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_on_remote_features_event
 * @brief      invoked when LE remote features event is received.
 * @param[out] rsi_ble_event_remote_features, connected remote device information
 * @return     none.
 * @section description
 * This callback function indicates the remote device features
 */
void rsi_ble_on_remote_features_event(rsi_ble_event_remote_features_t *rsi_ble_event_remote_features)
{
  memcpy(&remote_dev_feature, rsi_ble_event_remote_features, sizeof(rsi_ble_event_remote_features_t));
  rsi_ble_app_set_event(RSI_BLE_RECEIVE_REMOTE_FEATURES);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_more_data_req_event
 * @brief      its invoked when le more data request is received
 * @param[out] rsi_ble_more_data_evt contains the LE Device Buffer Indication
 * information.
 * @return     none.
 * @section description
 * This callback function is invoked when indication confirmation response is
 * received from the module.
 */
static void rsi_ble_more_data_req_event(rsi_ble_event_le_dev_buf_ind_t *rsi_ble_more_data_evt)
{
  UNUSED_PARAMETER(rsi_ble_more_data_evt);
  rsi_ble_app_set_event(RSI_BLE_MORE_DATA_REQ_EVENT);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_data_length_change_event
 * @brief      invoked when data length is set
 * @param[out] rsi_ble_data_length_update, data length information
 * @section description
 * This Callback function indicates data length is set
 */
void rsi_ble_data_length_change_event(rsi_ble_event_data_length_update_t *rsi_ble_data_length_update)
{
  UNUSED_PARAMETER(rsi_ble_data_length_update); //This statement is added only to resolve compilation warning, value is unchanged
  // memcpy(&updated_data_len_params, rsi_ble_data_length_update, sizeof(rsi_ble_event_data_length_update_t));
  rsi_ble_app_set_event(RSI_APP_EVENT_DATA_LENGTH_CHANGE);
}
/*==============================================*/
/**
 * @fn         rsi_ble_on_mtu_event
 * @brief      invoked  when an MTU size event is received
 * @param[out]  rsi_ble_mtu, it indicates MTU size.
 * @return     none.
 * @section description
 * This callback function is invoked  when an MTU size event is received
 */
static void rsi_ble_on_mtu_event(rsi_ble_event_mtu_t *rsi_ble_mtu)
{
  // uint16_t mtu_size = rsi_ble_mtu->mtu_size;
  rsi_ble_event_mtu_t app_ble_mtu_event = { 0 };
  memcpy(&app_ble_mtu_event, rsi_ble_mtu, sizeof(rsi_ble_event_mtu_t));
  app_log_info("\r\nMTU size : %dBytes\r\n", rsi_ble_mtu->mtu_size);
  rsi_6byte_dev_address_to_ascii(remote_dev_addr, app_ble_mtu_event.dev_addr);
  rsi_ble_app_set_event(RSI_BLE_MTU_EX_EVENT);
}

/*==============================================*/
/**
 * @fn         rsi_ble_on_gatt_write_event
 * @brief      this is call back function, it invokes when write/notify events received.
 * @param[out]  event_id, it indicates write/notification event id.
 * @param[out]  rsi_ble_write, write event parameters.
 * @return     none.
 * @section description   手机APP-> BLE设备
 * This is a callback function
 */
static void rsi_ble_on_gatt_write_event(uint16_t event_id, rsi_ble_event_write_t *rsi_ble_write)
{
  UNUSED_PARAMETER(event_id);

  uint16_t gatt_write_handle = *((uint16_t *)rsi_ble_write->handle);
  // app_log_info("handle: 0x%04X, len: %d\r\n", gatt_write_handle, rsi_ble_write->length);

  // ota handles
  rsi_ble_event_write_t app_ble_write_event;
  if ((ota_fw_control_handle == gatt_write_handle) || (ota_fw_tf_handle == gatt_write_handle))
  {
    memcpy(&app_ble_write_event, rsi_ble_write, sizeof(rsi_ble_event_write_t));
    if (xQueueSend(ble_ota_data_queue, &app_ble_write_event, 10 / portTICK_PERIOD_MS) == pdFALSE){
        app_log_error("Failed to send data because the queue was full!\r\n");
    }
  }

  // Requests will come from Mobile app
  CommandBufferT command_buffer_param;
  if ((cat_tx_handle) == gatt_write_handle) 
  {
    app_log_info("<<----RECV----");
    app_log_hexdump_info(rsi_ble_write->att_value, rsi_ble_write->length);

    memcpy(command_buffer_param.data, rsi_ble_write->att_value, rsi_ble_write->length);
    command_buffer_param.len = rsi_ble_write->length;
    if(xQueueSend(bluetooth_data_queue, &command_buffer_param, 10 / portTICK_PERIOD_MS) == pdFALSE){
        app_log_error("Failed to send data because the queue was full!\r\n");
    }
  }
}

/*==============================================*/
/**
 * @fn         rsi_ble_app_init
 * @brief      initialize the BLE module.
 * @param[in]  none
 * @return     none.
 * @section description
 * This function is used to initialize the BLE module
 */
void rsi_ble_configurator_init(void)
{
  uint8_t adv[31] = { 2, 1, 6 };

  // registering the GAP callback functions
  rsi_ble_gap_register_callbacks(NULL,
                                 rsi_ble_on_connect_event,
                                 rsi_ble_on_disconnect_event,
                                 NULL,
                                 NULL,
                                 rsi_ble_data_length_change_event,
                                 rsi_ble_on_enhance_conn_status_event,
                                 NULL,
                                 rsi_ble_on_conn_update_complete_event,
                                 NULL);
  //! registering the GAP extended call back functions
  rsi_ble_gap_extended_register_callbacks(rsi_ble_on_remote_features_event, rsi_ble_more_data_req_event);

  // registering the GATT callback functions
  rsi_ble_gatt_register_callbacks(NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  rsi_ble_on_gatt_write_event,
                                  NULL,
                                  NULL,
                                  NULL,
                                  rsi_ble_on_mtu_event,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL,
                                  NULL);


  // rsi_ble_add_prov_serv(); // adding simple BLE chat service
  rsi_ble_add_catcollar_serv(); // adding catcollar service
  rsi_ble_add_ota_fwup_serv();     // adding ota firmware update service

  // initializing the application events map
  rsi_ble_app_init_events();

#ifdef RSI_BLE_PROVISIONING_ENABLED
  // Set local name
  rsi_bt_set_local_name((uint8_t *)RSI_BLE_APP_DEVICE_NAME);

  // prepare advertise data //local/device name
  adv[3] = strlen(RSI_BLE_APP_DEVICE_NAME) + 1;
  adv[4] = 9;
  strcpy((char *)&adv[5], RSI_BLE_APP_DEVICE_NAME);

  // set advertise data
  rsi_ble_set_advertise_data(adv, strlen(RSI_BLE_APP_DEVICE_NAME) + 5);
#else
  uint8_t catclooar_ble_name[16] = {0};
  uint8_t device_bt_addr[6] = {0};

  // Set local name
  const char *addr_str = RSI_BLE_ADV_DIR_ADDR;
  char addr_str_copy[20];
  strncpy(addr_str_copy, addr_str, sizeof(addr_str_copy));
  addr_str_copy[sizeof(addr_str_copy)-1] = '\0';

  char *token = strtok(addr_str_copy, ":");
  int i = 0;
  while (token != NULL && i < 6) {
      device_bt_addr[i++] = (uint8_t)strtol(token, NULL, 16);
      token = strtok(NULL, ":");
  }

  if (i != 6) {
      app_log_error("Failed to parse device bt addr, using default\r\n");
      // 使用默认地址
      uint8_t default_addr[6] = {0x00, 0x23, 0xA7, 0x12, 0x34, 0x56};
      memcpy(device_bt_addr, default_addr, sizeof(default_addr));
  }

  app_log_info("device bt addr hex = %02X:%02X:%02X:%02X:%02X:%02X\r\n",
               device_bt_addr[0], device_bt_addr[1], device_bt_addr[2],
               device_bt_addr[3], device_bt_addr[4], device_bt_addr[5]);

  snprintf((char *)catclooar_ble_name, sizeof(catclooar_ble_name), 
            "NDC%02X%02X%02X%02X%02X%02X", 
            device_bt_addr[0], device_bt_addr[1], device_bt_addr[2], 
            device_bt_addr[3], device_bt_addr[4], device_bt_addr[5]);
  app_log_info("ble local name = %s\r\n", catclooar_ble_name);
  rsi_bt_set_local_name((uint8_t *)catclooar_ble_name);
  rsi_bt_set_bd_addr((uint8_t *)device_bt_addr);

  // prepare advertise data //local/device name
  adv[3] = sizeof(catclooar_ble_name) + 1;
  adv[4] = 9;
  // strcpy((char *)&adv[5], catclooar_ble_name);
  memcpy(&adv[5], catclooar_ble_name, sizeof(catclooar_ble_name));

  // set advertise data
  rsi_ble_set_advertise_data(adv, sizeof(catclooar_ble_name) + 5);
#endif

  // set device in advertising mode.
  rsi_ble_start_advertising();
  LOG_PRINT("\r\nBLE Advertising Started...\r\n");
}

/*==============================================*/
/**
 * @fn         rsi_ble_app_task
 * @brief      this function will execute when BLE events are raised.
 * @param[in]  none.
 * @return     none.
 * @section description
 */

void rsi_ble_configurator_task(void *argument)
{
  UNUSED_PARAMETER(argument);

  int32_t status = 0;
  int32_t event_id;
  uint8_t buf_config_var = 0;
  static uint16_t mtu_size = 0;
  uint16_t conn_int = 0;
  uint8_t str_remote_address[BD_ADDR_STRING_SIZE] = {0};

  ble_thread_sem = osSemaphoreNew(1, 0, NULL);
  if (ble_thread_sem == NULL) {
    app_log_error("Failed to create ble_thread_sem\n");
    return;
  }

  while (1) {
    // checking for events list
    event_id = rsi_ble_app_get_event();

    if (event_id == -1) {
      osSemaphoreAcquire(ble_thread_sem, osWaitForever);
      // if events are not received loop will be continued.
      continue;
    }

    switch (event_id) {
      case RSI_BLE_CONN_EVENT: {
      //! Converting the 6 byte address to ASCII.
      rsi_6byte_dev_address_to_ascii(str_remote_address, conn_event_to_app.dev_addr);
      LOG_PRINT("\r\n Module connected to address : %s \r\n", str_remote_address);

      //! MTU update request only if it was not initially set to 232.
      //  if (mtu_size < RSI_BLE_MAX_DATA_LEN) {
      status = rsi_ble_mtu_exchange_event(conn_event_to_app.dev_addr, RSI_BLE_MAX_DATA_LEN);
      if (status != RSI_SUCCESS) {
        LOG_PRINT("\r\n MTU request failed with status = %lx \r\n", status);
      }
      //  }
      catcollar_bt_connection_set_state(CATCOLLAR_BT_CONNECTED);
      //! clear the served event
      rsi_ble_app_clear_event(RSI_BLE_CONN_EVENT);

    } break; //! end of RSI_BLE_CONN_EVENT case

    case RSI_BLE_MTU_EX_EVENT: {

      //! MTU update request only if it was not initially set to 232.
      if (mtu_size > RSI_BLE_MAX_DATA_LEN) {
        status = rsi_ble_mtu_exchange_event(conn_event_to_app.dev_addr, RSI_BLE_MAX_DATA_LEN);
        if (status != RSI_SUCCESS) {
          LOG_PRINT("\r\n MTU request failed with status = %lx \r\n", status);
        } else {
          LOG_PRINT("\r\n MTU Requested\r\n");
        }
      }

      //! Else if MTU is already equal to 232, then proceed to PHY update event
      else {
        status = rsi_ble_setphy((int8_t *)conn_event_to_app.dev_addr, TX_PHY_RATE, RX_PHY_RATE, CODDED_PHY_RATE);
      }

      //! clear the served event
      rsi_ble_app_clear_event(RSI_BLE_MTU_EX_EVENT);

    } break; //! end of RSI_BLE_MTU_EX_EVENT case

    case RSI_BLE_PHY_UPDATE_EVENT: {

      if (conn_int != CONNECTION_INTERVAL_MIN) {
        status = rsi_ble_conn_params_update(conn_event_to_app.dev_addr,
                                            CONNECTION_INTERVAL_MIN,
                                            CONNECTION_INTERVAL_MAX,
                                            CONNECTION_LATENCY,
                                            SUPERVISION_TIMEOUT);
        if (status != RSI_SUCCESS) {
          LOG_PRINT("\r\n Connection Parameters update request failed with ""status = %lx \r\n", status);
        }
      }

      rsi_ble_app_clear_event(RSI_BLE_PHY_UPDATE_EVENT);

    } break; //! End of RSI_BLE_PHY_UPDATE_EVENT

    case RSI_BLE_CONN_UPDATE_EVENT: {
      //! clear the served event
      rsi_ble_app_clear_event(RSI_BLE_CONN_UPDATE_EVENT);
      if (buf_config_var == 1) {
        //! Configure the buf mode for Notify and WO response commands for the
        //! remote device
        status = rsi_ble_set_wo_resp_notify_buf_info(conn_event_to_app.dev_addr, DLE_BUFFER_MODE, DLE_BUFFER_COUNT);
        if (status != RSI_SUCCESS) {
          break;
        } else {
          LOG_PRINT("\r\nBuf configuration done for notify and set_att cmds buf ""mode = %d , max buff count =%d \n",
              DLE_BUFFER_MODE, DLE_BUFFER_COUNT);
        }
      }

    } break;
    // -------------------------------------------------------------------------
    // This event invokes when a device received the remote device features
    case RSI_BLE_RECEIVE_REMOTE_FEATURES: {
      //! clear the served event
      rsi_ble_app_clear_event(RSI_BLE_RECEIVE_REMOTE_FEATURES);

      if (remote_dev_feature.remote_features[0] & 0x20) {
        status = rsi_ble_set_data_len(conn_event_to_app.dev_addr, TX_LEN, TX_TIME);
        if (status != RSI_SUCCESS) {
          LOG_PRINT("\n set data length cmd failed with error code = ""%lx \n", status);
          rsi_ble_app_set_event(RSI_BLE_RECEIVE_REMOTE_FEATURES);
        }
      }

    } break;
    // -------------------------------------------------------------------------
    // This event invokes when a device received the more data request event
    case RSI_BLE_MORE_DATA_REQ_EVENT: {
      //! clear the served event
      rsi_ble_app_clear_event(RSI_BLE_MORE_DATA_REQ_EVENT);

    } break;
    case RSI_APP_EVENT_DATA_LENGTH_CHANGE: {
      rsi_ble_app_clear_event(RSI_APP_EVENT_DATA_LENGTH_CHANGE);
    } break;

    //! This event is invoked when the module gets diconnected before completion
    //! of firmware Update.
    case RSI_BLE_DISCONN_EVENT: {

      rsi_ble_app_clear_event(RSI_BLE_DISCONN_EVENT);
      LOG_PRINT("\r\n Module got Disconnected\r\n");
      catcollar_bt_connection_set_state(CATCOLLAR_BT_DISCONNECTED);
      //! set device in advertising mode.
    adv:
      status = rsi_ble_start_advertising();
      if (status != RSI_SUCCESS) {
        goto adv;
      } else {
        LOG_PRINT("\r\n module advertising \r\n");
      }

    } break;

    default: {
      break;
    }
    }
  }
}


const osThreadAttr_t ble_thread_attributes = {
  .name       = "ble_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 2048,
  .priority   = osPriorityNormal1,
  .tz_module  = 0,
  .reserved   = 0,
};

int rsi_ble_task_init(void)
{

  if (osThreadNew((osThreadFunc_t)rsi_ble_configurator_task, NULL, &ble_thread_attributes) == NULL) {
    app_log_error("Failed to create BLE thread\r\n");
    return SL_STATUS_FAIL;
  }

  // BLE initialization
  rsi_ble_configurator_init();

  return SL_STATUS_OK;
}


// /*==============================================*/
// /**
//  * @fn         wifi_app_send_to_ble
//  * @brief      this function is used to send data to ble app.
//  * @param[in]   msg_type, it indicates write/notification event id.
//  * @param[in]  data, raw data pointer.
//  * @param[in]  data_len, raw data length.
//  * @return     none.
//  * @section description
//  */
// void wifi_app_send_to_ble(uint16_t msg_type, uint8_t *data, uint16_t data_len)
// {
//   switch (msg_type) {
//     case WIFI_APP_SCAN_RESP:
//       memset(scanresult, 0, data_len);
//       memcpy(scanresult, (sl_wifi_scan_result_t *)data, data_len);

//       rsi_ble_app_set_event(RSI_BLE_WLAN_SCAN_RESP);
//       break;
//     case WIFI_APP_CONNECTION_STATUS:
//       rsi_ble_app_set_event(RSI_BLE_WLAN_JOIN_STATUS);
//       break;
//     case WIFI_APP_DISCONNECTION_STATUS:
//       rsi_ble_app_set_event(RSI_BLE_WLAN_DISCONNECT_STATUS);
//       break;
//     case WIFI_APP_DISCONNECTION_NOTIFY:
//       rsi_ble_app_set_event(RSI_BLE_WLAN_DISCONN_NOTIFY);
//       break;
//     case WIFI_APP_TIMEOUT_NOTIFY:
//       rsi_ble_app_set_event(RSI_BLE_WLAN_TIMEOUT_NOTIFY);
//       break;
//     default:
//       break;
//   }
// }
