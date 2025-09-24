// BLE include file to refer BLE APIs
#include <rsi_ble_apis.h>
#include <rsi_bt_common_apis.h>
#include <rsi_common_apis.h>
#include <string.h>

#include "ble_config.h"

#include "app_log.h"
#include "ble_ota.h"
#include "ble_profile.h"

#include "sl_si91x_driver.h"

#ifdef SLI_SI91X_MCU_INTERFACE
#include "sl_si91x_hal_soc_soft_reset.h"
#endif

// // WDT constants
// #define SL_WDT_INTERRUPT_TIME 15    // WDT Interrupt Time
// #define SL_WDT_SYSTEM_RESET_TIME 17 // WDT System Reset Time
// #define SL_WDT_WINDOW_TIME 0        // WDT Window Time

QueueHandle_t ble_ota_data_queue = NULL;
static void ble_ota_work_handler(void *params)
{
    (void)params;

    rsi_ble_event_write_t ble_ota_buffer;

    uint32_t start_timer = 0, stop_timer = 0;
    uint8_t data_tf_start = 0;
    sl_status_t status = SL_STATUS_OK;
    static uint16_t chunk_number = 1;
    uint8_t firmware_header_data[64] = {0};
    uint8_t firmware_chunk_fw_payload[230] = {0};
    uint16_t received_chunk_num = 0;
    while(1)
    {
        if(xQueueReceive(ble_ota_data_queue, (void *)&ble_ota_buffer, 10 / portTICK_RATE_MS) == pdPASS)
		    {
          if (ble_ota_buffer.handle[0] == ota_fw_control_handle) {
            if (ble_ota_buffer.att_value[0] == 0) {
              data_tf_start = 1;
              chunk_number = 1;
              received_chunk_num = 0;
            } else if (ble_ota_buffer.att_value[0] == 3) {
              data_tf_start = 1;
            }

          } else if (ble_ota_buffer.handle[0] == ota_fw_tf_handle) {
            app_log_info("Received chunk number: %d\r\n", received_chunk_num++);
            if (data_tf_start == 1) {
              if (chunk_number == 1) {
                memcpy(&firmware_header_data[0], &ble_ota_buffer.att_value[0], 64);
                memcpy(&firmware_chunk_fw_payload[0], &ble_ota_buffer.att_value[0], ble_ota_buffer.length);
                status = sl_si91x_fwup_start(firmware_header_data);
                app_log_info("Firmware transfer in progress. Please wait...  \r\n");
                start_timer = osKernelGetTickCount();
                chunk_number++;
                status = sl_si91x_fwup_load(firmware_chunk_fw_payload, ble_ota_buffer.length);
                // app_log_info("Received chunk number: %d\r\n", chunk_number - 1);
              } else {
                status = sl_si91x_fwup_load(ble_ota_buffer.att_value, ble_ota_buffer.length);
                //! If "status" for loading the last chunk is 3, then all chunks
                //! received and loaded successfully
                if (status == SL_STATUS_SI91X_FW_UPDATE_DONE) {
                  stop_timer = osKernelGetTickCount();
#if (FW_UPGRADE_TYPE == TA_FW_UP)
                  LOG_PRINT("\r\n Time in sec:%ld\r\n",
                            (stop_timer - start_timer) / 1000);
                  LOG_PRINT("\r\n TA Firmware transfer complete!\r\n");
                  LOG_PRINT("\r\n Safe upgrade in Progress. Please wait....\r\n");

                  status = sl_net_deinit(SL_NET_WIFI_CLIENT_INTERFACE);
                  printf("\r\nWi-Fi Deinit status : %lx\r\n", status);
                  VERIFY_STATUS_AND_RETURN(status);

                  osDelay(29000);
                  status = sl_wifi_init(&config, NULL, sl_wifi_default_event_handler);
                  if (status != SL_STATUS_OK) {
                    LOG_PRINT(
                        "\r\nWi-Fi Initialization Failed, Error Code : 0x%lX\r\n",
                        status);
                    return status;
                  } else {
                    printf("\r\n Wi-Fi Initialization Successful\n");
                  }

                  status = sl_wifi_get_firmware_version(&version);
                  VERIFY_STATUS_AND_RETURN(status);

                  printf("\r\nFirmware version after update:\r\n");
                  print_firmware_version(&version);
                  return SL_STATUS_OK;

#endif
#if (FW_UPGRADE_TYPE == M4_FW_UP)
                  app_log_info("\t Time in sec:%ld\r\n", (stop_timer - start_timer) / 1000);
                  app_log_info("\t M4 Firmware transfer complete!\r\n");
                  sl_si91x_soc_nvic_reset();
#endif
                return;
                }
              }
            }
          }
        }
    }
}

static bool enter_ble_ota_task_flag = false;
#define TASK_CMD_OTA_STACK_SIZE   2048
static StackType_t ble_ota_TaskStack[TASK_CMD_OTA_STACK_SIZE];
static StaticTask_t ble_ota_TaskHandle;

void ble_ota_task_init(void)
{
    if (enter_ble_ota_task_flag == false){
        enter_ble_ota_task_flag = true;
        ble_ota_data_queue = xQueueCreate(5, sizeof(rsi_ble_event_write_t));
        if(ble_ota_data_queue == NULL) {
            app_log_error("Failed to create ble_ota_data_queue\r\n");
            return;
        }
        app_log_info("ble_ota_data_queue created successfully\r\n");

         if (xTaskCreateStatic(
            ble_ota_work_handler,           // 任务函数
            "ble_ota_work_handler",         // 任务名称
            TASK_CMD_OTA_STACK_SIZE,       // 堆栈大小
            NULL,                                 // 任务参数
            osPriorityNormal4,                    // 任务优先级 configMAX_PRIORITIES
            ble_ota_TaskStack,      // 静态任务堆栈
            &ble_ota_TaskHandle     // 静态任务控制块
            ) == NULL)
        {
            app_log_error("Failed to create ble_ota_work_handler\r\n");
            vQueueDelete(ble_ota_data_queue);
            return;
        }
        app_log_info("ble_ota_work_handler created successfully\r\n");
    }
}

void ble_ota_task_delete(void)
{
	app_log_debug("ble_ota_task_delete\r\n");
	if (enter_ble_ota_task_flag == true){
		enter_ble_ota_task_flag = false;
		if (ble_ota_data_queue != NULL){
			vQueueDelete(ble_ota_data_queue);		 //删除蓝牙数据接收队列
		}
	}
}