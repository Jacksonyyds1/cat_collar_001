// BLE include file to refer BLE APIs
#include <rsi_ble_apis.h>
#include <rsi_bt_common_apis.h>
#include <rsi_common_apis.h>
#include <string.h>
// #include <rsi_ble_common_config.h>

#include "ble_config.h"
#include "ble_profile.h"
#include "app_log.h"

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////
/*============================================================================*/
/**
 * @fn         rsi_ble_prepare_128bit_uuid
 * @brief      this function is used to prepare the 128bit UUID
 * @param[in]  temp_service,received 128-bit service.
 * @param[out] temp_uuid,formed 128-bit service structure.
 * @return     none.
 * @section description
 * This function prepares the 128bit UUID
 */
static void rsi_ble_prepare_128bit_uuid(uint8_t temp_service[UUID_16BYTES_SIZE], uuid_t *temp_uuid) 
{
  temp_uuid->val.val128.data1 =
      ((temp_service[0] << 24) | (temp_service[1] << 16) |
       (temp_service[2] << 8) | (temp_service[3]));
  temp_uuid->val.val128.data2 = ((temp_service[5]) | (temp_service[4] << 8));
  temp_uuid->val.val128.data3 = ((temp_service[7]) | (temp_service[6] << 8));
  temp_uuid->val.val128.data4[0] = temp_service[9];
  temp_uuid->val.val128.data4[1] = temp_service[8];
  temp_uuid->val.val128.data4[2] = temp_service[11];
  temp_uuid->val.val128.data4[3] = temp_service[10];
  temp_uuid->val.val128.data4[4] = temp_service[15];
  temp_uuid->val.val128.data4[5] = temp_service[14];
  temp_uuid->val.val128.data4[6] = temp_service[13];
  temp_uuid->val.val128.data4[7] = temp_service[12];
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_char_serv_att
 * @brief      this function is used to add characteristic service attribute.
 * @param[in]  serv_handler, service handler.
 * @param[in]  handle, characteristic service attribute handle.
 * @param[in]  val_prop, characteristic value property.
 * @param[in]  att_val_handle, characteristic value handle
 * @param[in]  att_val_uuid, characteristic value UUID
 * @return     none.
 * @section description
 * This function is used at application to add characteristic attribute
 */
static void rsi_ble_add_char_serv_att(void *serv_handler, 
                                uint16_t handle,
                                uint8_t val_prop, 
                                uint16_t att_val_handle,
                                uuid_t att_val_uuid) 
{
  rsi_ble_req_add_att_t new_att = {0};

  //! preparing the attribute service structure
  new_att.serv_handler       = serv_handler;
  new_att.handle             = handle;
  new_att.att_uuid.size      = 2;
  new_att.att_uuid.val.val16 = RSI_BLE_CHAR_SERV_UUID;
  new_att.property           = RSI_BLE_ATT_PROPERTY_READ;

  //! preparing the characteristic attribute value
  if (att_val_uuid.size == UUID_16BYTES_SIZE) {
    new_att.data_len = 4 + att_val_uuid.size;
    new_att.data[0] = val_prop;
    rsi_uint16_to_2bytes(&new_att.data[2], att_val_handle);
    memcpy(&new_att.data[4], &att_val_uuid.val.val128, sizeof(att_val_uuid.val.val128));
  } else {
    new_att.data_len = 4;
    new_att.data[0] = val_prop;
    rsi_uint16_to_2bytes(&new_att.data[2], att_val_handle);
    rsi_uint16_to_2bytes(&new_att.data[4], att_val_uuid.val.val16);
  }

  //! Add attribute to the service
  rsi_ble_add_attribute(&new_att);

  return;
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_char_val_att
 * @brief      this function is used to add characteristic value attribute.
 * @param[in]  serv_handler, new service handler.
 * @param[in]  handle, characteristic value attribute handle.
 * @param[in]  att_type_uuid, attribute UUID value.
 * @param[in]  val_prop, characteristic value property.
 * @param[in]  data, characteristic value data pointer.
 * @param[in]  data_len, characteristic value length.
 * @return     none.
 * @section description
 * This function is used to add characteristic value attribute.
 */

static void rsi_ble_add_char_val_att(void *serv_handler, 
                                     uint16_t handle,
                                     uuid_t att_type_uuid, 
                                     uint8_t val_prop,
                                     uint8_t *data, 
                                     uint8_t data_len,
                                     uint8_t auth_read)
{
  rsi_ble_req_add_att_t new_att = {0};

  //! preparing the attributes
  new_att.serv_handler = serv_handler;
  new_att.handle       = handle;
  new_att.config_bitmap = auth_read;
  memcpy(&new_att.att_uuid, &att_type_uuid, sizeof(uuid_t));
  new_att.property = val_prop;

  // preparing the attribute value
  new_att.data_len = RSI_MIN(sizeof(new_att.data), data_len);
  memcpy(new_att.data, data, new_att.data_len);

  // add attribute to the service
  rsi_ble_add_attribute(&new_att);

  // check the attribute property with notification
  if (val_prop & RSI_BLE_ATT_PROPERTY_NOTIFY) {
    // if notification property supports then we need to add client characteristic service.

    // preparing the client characteristic attribute & values
    memset(&new_att, 0, sizeof(rsi_ble_req_add_att_t));
    new_att.serv_handler       = serv_handler;
    new_att.handle             = handle + 1;
    new_att.att_uuid.size      = 2;
    new_att.att_uuid.val.val16 = RSI_BLE_CLIENT_CHAR_UUID;
    new_att.property           = RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE;
    new_att.data_len           = 2;

    // add attribute to the service
    rsi_ble_add_attribute(&new_att);
  }

  return;
}


/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

rsi_ble_resp_add_serv_t ota_serv_response;
uint16_t ota_fw_tf_handle, ota_fw_control_handle;
uint16_t ota_fw_version_handle, ota_bd_add_handle;

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_serv
 * @brief      this function is used to create the OTA Service
 * This function is used to create the OTA service
 * UUID: 1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0
 */
static void rsi_ble_add_ota_serv(void) {
  uuid_t ota_service = {0};
  rsi_ble_resp_add_serv_t new_serv_resp = {0};
  uint8_t ota_service_uuid[UUID_16BYTES_SIZE] = {0x1d, 0x14, 0xd6, 0xee, 0xfd, 0x63,
                                          0x4f, 0xa1, 0xbf, 0xa4, 0x8f, 0x47,
                                          0xb4, 0x21, 0x19, 0xf0};
  rsi_ble_prepare_128bit_uuid(ota_service_uuid, &ota_service);
  ota_service.size = UUID_16BYTES_SIZE;
  rsi_ble_add_service(ota_service, &new_serv_resp);
  ota_serv_response.serv_handler = new_serv_resp.serv_handler;
  ota_serv_response.start_handle = new_serv_resp.start_handle;
  app_log_info("ota_serv_response.start_handle = %d\r\n", ota_serv_response.start_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_info_char_serv
 * @brief      this function is used to create the OTA Information characteristic service
 * Information about the "Total number of chunks" and "Size of Last chunk" is
 * written on this characteristic UUID:F7BF3564-FB6D-4E53-88A4-5E37E0326063
 */
static void rsi_ble_add_ota_info_char_serv(void) {
  uuid_t ota_info_char_service = {0};
  uint8_t ota_info_char_serv[UUID_16BYTES_SIZE] = {0xf7, 0xbf, 0x35, 0x64, 0xfb, 0x6d,
                                            0x4e, 0x53, 0x88, 0xa4, 0x5e, 0x37,
                                            0xe0, 0x32, 0x60, 0x63};
  ota_info_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(ota_info_char_serv, &ota_info_char_service);

  rsi_ble_add_char_serv_att(ota_serv_response.serv_handler,
                            ota_serv_response.start_handle + 1,
                            RSI_BLE_ATT_PROPERTY_WRITE,
                            ota_serv_response.start_handle + 2,
                            ota_info_char_service);

  rsi_ble_add_char_val_att(ota_serv_response.serv_handler,
                           ota_serv_response.start_handle + 2,
                           ota_info_char_service,
                           RSI_BLE_ATT_PROPERTY_WRITE,
                           NULL, 
                           5, 
                           0);
  ota_fw_control_handle = ota_serv_response.start_handle + 2;
  app_log_info("ota_fw_control_handle = %d\r\n", ota_fw_control_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_data_char_serv
 * @brief      this function is used to create the "OTA Data" charactersitic
 * service
 * @param[in]  none.
 * @return     none.
 * @section description
 * Firmware chunks are written on this characteristic
 * UUID: 984227F3-34FC-4045-A5D0-2C581F81A153
 */
static void rsi_ble_add_ota_data_char_serv(void) {
  uuid_t ota_data_char_service = {0};
  uint8_t chunk_data = 0;
  uint8_t ota_data_char_serv[UUID_16BYTES_SIZE] = {0x98, 0x42, 0x27, 0xf3, 0x34, 0xfc,
                                            0x40, 0x45, 0xa5, 0xd0, 0x2c, 0x58,
                                            0x1f, 0x81, 0xa1, 0x53};

  ota_data_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(ota_data_char_serv, &ota_data_char_service);

  rsi_ble_add_char_serv_att(ota_serv_response.serv_handler,
                            ota_serv_response.start_handle + 3,
                            RSI_BLE_ATT_PROPERTY_WRITE,
                            ota_serv_response.start_handle + 4,
                            ota_data_char_service);

  rsi_ble_add_char_val_att(ota_serv_response.serv_handler,
                           ota_serv_response.start_handle + 4,
                           ota_data_char_service,
                           RSI_BLE_ATT_PROPERTY_WRITE,
                           &chunk_data,
                           RSI_BLE_MAX_DATA_LEN, 
                           0);
  ota_fw_tf_handle = ota_serv_response.start_handle + 4;
  app_log_info("ota_fw_tf_handle = %d\r\n", ota_fw_tf_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_fw_version_char_serv
 * @brief      This function is used to create Firmware version characteristic
 * @param[in]  none.
 * @return     none.
 * @section Description
 * This function is used to create Firmware version characteristic
 * UUID:4F4A2368-8CCA-451E-BFFF-CF0E2EE23E9F
 */
static void rsi_ble_add_ota_fw_version_char_serv(void) {
  uuid_t ota_fw_version_char_service = {0};
  extern sl_wifi_firmware_version_t catcollar_firmware_version;
  uint8_t fw_version_char_serv[UUID_16BYTES_SIZE] = {0x4f, 0x4a, 0x23, 0x68, 0x8c, 0xca,
                                              0x45, 0x1e, 0xbf, 0xff, 0xcf, 0x0e,
                                              0x2e, 0xe2, 0x3e, 0x9f};
  uint8_t firmware_version_conversion[18]={catcollar_firmware_version.chip_id,
                                           catcollar_firmware_version.rom_id,
                                           catcollar_firmware_version.major,
                                           catcollar_firmware_version.minor,
                                           catcollar_firmware_version.security_version,
                                           catcollar_firmware_version.patch_num,
                                           catcollar_firmware_version.customer_id};
  memcpy(&firmware_version_conversion[7],&catcollar_firmware_version.build_num,2);

  ota_fw_version_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(fw_version_char_serv,
                              &ota_fw_version_char_service);

  rsi_ble_add_char_serv_att(ota_serv_response.serv_handler,
                            ota_serv_response.start_handle + 5,
                            RSI_BLE_ATT_PROPERTY_READ,
                            ota_serv_response.start_handle + 6,
                            ota_fw_version_char_service);

  rsi_ble_add_char_val_att(ota_serv_response.serv_handler,
                           ota_serv_response.start_handle + 6,
                           ota_fw_version_char_service,
                           RSI_BLE_ATT_PROPERTY_READ,
                           firmware_version_conversion, 
                           20, 
                           0);
  ota_fw_version_handle = ota_serv_response.start_handle + 6;
  app_log_info("ota_fw_version_handle = %d\r\n", ota_fw_version_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_bd_add_char_serv
 * @brief      This function is used to create BD address characteristic
 * @param[in]  none.
 * @return     none.
 * @section Description
 * This function is used to create BD address characteristic
 * UUID:4M4A2368-8CCA-451E-BFFF-CF0E2EE23E9F
 */
static void rsi_ble_add_ota_bd_add_char_serv(void) {
  uuid_t ota_bd_add_char_service = {0};
  uint8_t str_local_dev_address[BD_ADDR_STRING_SIZE] = {0};
  uint8_t bd_add_char_serv[UUID_16BYTES_SIZE] = {0x4b, 0x4a, 0x23, 0x68, 0x8c, 0xca,
                                          0x45, 0x1e, 0xbf, 0xff, 0xcf, 0x0e,
                                          0x2e, 0xe2, 0x3e, 0x9f};

  ota_bd_add_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(bd_add_char_serv, &ota_bd_add_char_service);
  rsi_ble_add_char_serv_att(ota_serv_response.serv_handler,
                            ota_serv_response.start_handle + 7,
                            RSI_BLE_ATT_PROPERTY_READ,
                            ota_serv_response.start_handle + 8,
                            ota_bd_add_char_service);

  rsi_ble_add_char_val_att(ota_serv_response.serv_handler,
                           ota_serv_response.start_handle + 8,
                           ota_bd_add_char_service,
                           RSI_BLE_ATT_PROPERTY_READ,
                           str_local_dev_address, 
                           20, 
                           0);
  ota_bd_add_handle = ota_serv_response.start_handle + 8;
  app_log_info("ota_bd_add_handle = %d\r\n", ota_bd_add_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_ota_fwup_serv
 * @brief      This function is used to add the OTA Service and its
 * characteristics in the module
 * @param[in]  none.
 * @return     none.
 * @section description
 * The entire firmware update process is done on this service and
 */
void rsi_ble_add_ota_fwup_serv(void) 
{
  //! Adding the "OTA Service" Primary service
  //! UUID: 1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0
  rsi_ble_add_ota_serv();

  //! Adding the "OTA Information" characteristic service attribute
  //! UUID: F7BF3564-FB6D-4E53-88A4-5E37E0326063
  rsi_ble_add_ota_info_char_serv();

  //! Adding the "OTA Data" characteristic service attribute
  //! UUID: 984227F3-34FC-4045-A5D0-2C581F81A153
  rsi_ble_add_ota_data_char_serv();

  //! Adding the "OTA Firmware version" characteristic service attribute
  //! UUID: 4F4A2368-8CCA-451E-BFFF-CF0E2EE23E9F
  rsi_ble_add_ota_fw_version_char_serv();

  //! Adding the "OTA BD Address" characteristic service attribute
  //! UUID: 4B4A2368-8CCA-451E-BFFF-CF0E2EE23E9F
  rsi_ble_add_ota_bd_add_char_serv();
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

// BLE characteristic service uuid
#define RSI_BLE_NEW_SERVICE_UUID 0xAABB
#define RSI_BLE_ATTRIBUTE_1_UUID 0x1AA1
#define RSI_BLE_ATTRIBUTE_2_UUID 0x1BB1
#define RSI_BLE_ATTRIBUTE_3_UUID 0x1CC1

uint16_t mobile_to_device_handle, device_to_mobile_handle, notify_val_handle;

/*==============================================*/
/**
 * @fn         rsi_ble_simple_chat_add_new_serv
 * @brief      this function is used to add new service i.e., simple chat service.
 * @param[in]  none.
 * @return     int32_t
 *             0  =  success
 *             !0 = failure
 * @section description
 * This function is used at application to create new service.
 */
uint32_t rsi_ble_add_prov_serv(void)
{
  uuid_t new_uuid                       = { 0 };
  rsi_ble_resp_add_serv_t new_serv_resp = { 0 };
  uint8_t data[RSI_BLE_MAX_DATA_LEN]    = { 0 };

  new_uuid.size      = 2; // adding new service
  new_uuid.val.val16 = RSI_BLE_NEW_SERVICE_UUID;

  rsi_ble_add_service(new_uuid, &new_serv_resp);

  new_uuid.size      = 2; // adding characteristic service attribute to the service
  new_uuid.val.val16 = RSI_BLE_ATTRIBUTE_1_UUID;
  rsi_ble_add_char_serv_att(new_serv_resp.serv_handler,
                            new_serv_resp.start_handle + 1,
                            RSI_BLE_ATT_PROPERTY_WRITE,
                            new_serv_resp.start_handle + 2,
                            new_uuid);

  mobile_to_device_handle = new_serv_resp.start_handle + 2; // adding characteristic value attribute to the service
  new_uuid.size         = 2;
  new_uuid.val.val16    = RSI_BLE_ATTRIBUTE_1_UUID;
  rsi_ble_add_char_val_att(new_serv_resp.serv_handler,
                           new_serv_resp.start_handle + 2,
                           new_uuid,
                           RSI_BLE_ATT_PROPERTY_WRITE,
                           data,
                           RSI_BLE_MAX_DATA_LEN,
                           0);

  new_uuid.size      = 2; // adding characteristic service attribute to the service
  new_uuid.val.val16 = RSI_BLE_ATTRIBUTE_2_UUID;
  rsi_ble_add_char_serv_att(new_serv_resp.serv_handler,
                            new_serv_resp.start_handle + 3,
                            RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
                            new_serv_resp.start_handle + 4,
                            new_uuid);
  device_to_mobile_handle = new_serv_resp.start_handle + 4; // adding characteristic value attribute to the service
  new_uuid.size         = 2;
  new_uuid.val.val16    = RSI_BLE_ATTRIBUTE_2_UUID;
  rsi_ble_add_char_val_att(new_serv_resp.serv_handler,
                           new_serv_resp.start_handle + 4,
                           new_uuid,
                           RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_WRITE,
                           data,
                           RSI_BLE_MAX_DATA_LEN,
                           0);

  new_uuid.size      = 2; // adding characteristic service attribute to the service
  new_uuid.val.val16 = RSI_BLE_ATTRIBUTE_3_UUID;
  rsi_ble_add_char_serv_att(new_serv_resp.serv_handler,
                            new_serv_resp.start_handle + 5,
                            RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
                            new_serv_resp.start_handle + 6,
                            new_uuid);
  notify_val_handle = new_serv_resp.start_handle + 6; // adding characteristic value attribute to the service
  new_uuid.size         = 2;
  new_uuid.val.val16    = RSI_BLE_ATTRIBUTE_3_UUID;
  rsi_ble_add_char_val_att(new_serv_resp.serv_handler,
                           new_serv_resp.start_handle + 6,
                           new_uuid,
                           RSI_BLE_ATT_PROPERTY_READ | RSI_BLE_ATT_PROPERTY_NOTIFY,
                           data,
                           RSI_BLE_MAX_DATA_LEN,
                           0);

  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

uint16_t cat_tx_handle, cat_rx_handle, cat_notify_handle;
rsi_ble_resp_add_serv_t cat_serv_response;

/*============================================================================*/
/**
 * @fn         rsi_ble_add_cat_service
 * @brief      this function is used to create the OTA Service
 * @param[in]  none.
 * @return     none.
 * @section description
 * This function is used to create the OTA service
 * UUID: 00c74f02-d1bc-11ed-afa1-0242ac120002
 */

static void rsi_ble_add_cat_service(void) {
  uuid_t cat_service = {0};
  rsi_ble_resp_add_serv_t new_serv_resp = {0};
  uint8_t cat_service_uuid[UUID_16BYTES_SIZE] = {0x00, 0xc7, 0x4f, 0x02, 0xd1, 0xbc,
                                  0x11, 0xed, 0xaf, 0xa1, 0x02, 0x42,
                                  0xac, 0x12, 0x00, 0x02};
  rsi_ble_prepare_128bit_uuid(cat_service_uuid, &cat_service);
  cat_service.size = UUID_16BYTES_SIZE;
  rsi_ble_add_service(cat_service, &new_serv_resp);
  cat_serv_response.serv_handler = new_serv_resp.serv_handler;
  cat_serv_response.start_handle = new_serv_resp.start_handle;
  app_log_info("cat_serv_response.start_handle = %d\r\n", cat_serv_response.start_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_cat_tx_char
 * @brief      this function is used to create the "Cat TX" charactersitic service
 * UUID: 6765a69d-cd79-4df6-aad5-043df9425556
 */
static void rsi_ble_add_cat_tx_char(void) {
  uuid_t cat_tx_char_service = {0};
  uint8_t send_data[RSI_BLE_MAX_DATA_LEN] = {0};
  uint8_t cat_tx_char_serv[UUID_16BYTES_SIZE] = {0x67, 0x65, 0xa6, 0x9d, 0xcd, 0x79,
                                          0x4d, 0xf6, 0xaa, 0xd5, 0x04, 0x3d,
                                          0xf9, 0x42, 0x55, 0x56};

  cat_tx_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(cat_tx_char_serv, &cat_tx_char_service);

  rsi_ble_add_char_serv_att(cat_serv_response.serv_handler,
                            cat_serv_response.start_handle + 1,
                             RSI_BLE_ATT_PROPERTY_WRITE,
                            cat_serv_response.start_handle + 2,
                            cat_tx_char_service);

  rsi_ble_add_char_val_att(cat_serv_response.serv_handler,
                           cat_serv_response.start_handle + 2,
                           cat_tx_char_service,
                           RSI_BLE_ATT_PROPERTY_WRITE,
                           send_data,
                           sizeof(send_data),
                           0);
  cat_tx_handle = cat_serv_response.start_handle + 2;
  app_log_info("cat_tx_handle = %d\r\n", cat_tx_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_cat_rx_char
 * @brief      this function is used to create the "Cat RX" characteristic service
 * UUID: b6ab2ce3-a5aa-436a-817a-cc13a45aab76
 */
static void rsi_ble_add_cat_rx_char(void) {
  uuid_t cat_rx_char_service = {0};
  uint8_t read_data[RSI_BLE_MAX_DATA_LEN] = {0};
  uint8_t cat_rx_char_serv[UUID_16BYTES_SIZE] = {0xb6, 0xab, 0x2c, 0xe3, 0xa5, 0xaa,
                                          0x43, 0x6a, 0x81, 0x7a, 0xcc, 0x13,
                                          0xa4, 0x5a, 0xab, 0x76};
  cat_rx_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(cat_rx_char_serv, &cat_rx_char_service);

  rsi_ble_add_char_serv_att(cat_serv_response.serv_handler,
                            cat_serv_response.start_handle + 3,
                            RSI_BLE_ATT_PROPERTY_READ,
                            cat_serv_response.start_handle + 4,
                            cat_rx_char_service);

  rsi_ble_add_char_val_att(cat_serv_response.serv_handler,
                            cat_serv_response.start_handle + 4,
                            cat_rx_char_service,
                            RSI_BLE_ATT_PROPERTY_READ,
                            read_data,
                            sizeof(read_data),
                            BIT(7));
  cat_rx_handle = cat_serv_response.start_handle + 4;
  app_log_info("cat_rx_handle = %d\r\n", cat_rx_handle);
}

/*============================================================================*/
/**
 * @fn         rsi_ble_add_cat_notify_char
 * @brief      This function is used to create "Cat Notify" characteristic service
 * UUID:  207bdc30-c3cc-4a14-8b66-56ba8a826640
 */
static void rsi_ble_add_cat_notify_char(void) {
  uuid_t cat_notify_char_service = {0};
  uint8_t notify_data[RSI_BLE_MAX_DATA_LEN] = {0};
  uint8_t cat_notify_char_serv[UUID_16BYTES_SIZE] = {0x20, 0x7b, 0xdc, 0x30, 0xc3, 0xcc,
                                              0x4a, 0x14, 0x8b, 0x66, 0x56, 0xba,
                                              0x8a, 0x82, 0x66, 0x40};
  cat_notify_char_service.size = UUID_16BYTES_SIZE;
  rsi_ble_prepare_128bit_uuid(cat_notify_char_serv, &cat_notify_char_service);

  rsi_ble_add_char_serv_att(cat_serv_response.serv_handler,
                            cat_serv_response.start_handle + 5,
                            RSI_BLE_ATT_PROPERTY_NOTIFY,
                            cat_serv_response.start_handle + 6,
                            cat_notify_char_service);

  rsi_ble_add_char_val_att(cat_serv_response.serv_handler,
                           cat_serv_response.start_handle + 6,
                           cat_notify_char_service,
                           RSI_BLE_ATT_PROPERTY_NOTIFY,
                           notify_data,
                           sizeof(notify_data),
                           0);
  cat_notify_handle = cat_serv_response.start_handle + 6;
  app_log_info("cat_notify_handle = %d\r\n", cat_notify_handle);
}

void rsi_ble_add_catcollar_serv(void)
{
  //! Adding the "CAT Service" Primary service
  //! UUID: 00c74f02-d1bc-11ed-afa1-0242ac120002
  rsi_ble_add_cat_service();

  //! Adding the "Cat TX" characteristic service attribute
  //! UUID: 6765a69d-cd79-4df6-aad5-043df9425556
  // mobile app will use this handle to send data to the device
  // mobile -> device
  rsi_ble_add_cat_tx_char();

  //! Adding the "Cat RX" characteristic service attribute
  //! UUID: b6ab2ce3-a5aa-436a-817a-cc13a45aab76
  // device will use this handle to send data to the mobile app
  // device -> mobile
  rsi_ble_add_cat_rx_char();

  //! Adding the "Cat Notify" characteristic service attribute
  //! UUID: 207bdc30-c3cc-4a14-8b66-56ba8a826640
  // mobile app will use this handle to receive notifications from the device
  // mobile -> device
  rsi_ble_add_cat_notify_char();
}

/////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////

