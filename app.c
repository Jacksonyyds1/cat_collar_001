/*******************************************************************************
* @file  app.c
* @brief
*******************************************************************************
* # License
* <b>Copyright 2024 Silicon Laboratories Inc. www.silabs.com</b>
*******************************************************************************
*
* The licensor of this software is Silicon Laboratories Inc. Your use of this
* software is governed by the terms of Silicon Labs Master Software License
* Agreement (MSLA) available at
* www.silabs.com/about-us/legal/master-software-license-agreement. This
* software is distributed to you in Source Code format and is governed by the
* sections of the MSLA applicable to Source Code.
*
******************************************************************************/
/*************************************************************************
 *
 */

/*================================================================================
 * @brief : This file contains example application for Wlan Station BLE
 * Provisioning
 * @section Description :
 * This application explains how to get the WLAN connection functionality using
 * BLE provisioning.
 * Silicon Labs Module starts advertising and with BLE Provisioning the Access Point
 * details are fetched.
 * Silicon Labs device is configured as a WiFi station and connects to an Access Point.
 =================================================================================*/

/**
 * Include files
 **/
#include "cmsis_os2.h"
#include "sl_utility.h"
#include "app.h"

#include "app_log.h"
#include "common_i2c.h"
#include "common.h"
#include "blinky.h"
#include "lsm6dsv.h"
#include "spa06_003.h"
#include "bmm350.h" 
#include "sy6105_bsp.h"
#include "mlx90632.h"
#include "spi_flash.h"
#include "storage_api.h"
#include "shell_port.h"

#include "chekr_record.h"
#include "ble_data_parse.h"
#include "ble_app_cat.h"
#include "wifi_http_event.h"
#include "wifi_app.h"
#include "ble_ota.h"

// #define CATCOLLAR_DEBUG

static int32_t prev_heap = -1;

#ifdef CATCOLLAR_DEBUG
#define LOG_HEAP_DELTA(step) do { \
    int32_t current_heap = xPortGetFreeHeapSize(); \
    int32_t heap_delta = (prev_heap == -1) ? 0 : (prev_heap - current_heap); \
    app_log_info(#step " Free heap size: %d bytes, Delta: %d bytes\r\n", current_heap, heap_delta); \
    prev_heap = current_heap; \
} while (0)
#else
#define LOG_HEAP_DELTA(step) do {} while (0)
#endif

// Function prototypes
extern void wifi_app_task(void);
sl_wifi_firmware_version_t catcollar_firmware_version = { 0 };

const osThreadAttr_t thread_attributes = {
  .name       = "application_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 3072,
  // .stack_size = 6144,
  .priority   = osPriorityNormal,
  .tz_module  = 0,
  .reserved   = 0,
};

void application(void *argument)
{
  int32_t status = SL_STATUS_OK;
  UNUSED_PARAMETER(argument);

  prev_heap = xPortGetFreeHeapSize();
  app_log_info("Init Free heap size: %d bytes\r\n", prev_heap);

  //! Wi-Fi initialization
  status = rsi_wifi_task_init();
  if (status != SL_STATUS_OK) {
    app_log_error("WiFi Task Init Failed!! Error Code : 0x%lX\r\n", status);
  }

  //! Firmware version Prints
  status = sl_wifi_get_firmware_version(&catcollar_firmware_version);
  if (status != SL_STATUS_OK) {
    printf("\r\nFirmware version Failed, Error Code : 0x%lX\r\n", status);
  } else {
    print_firmware_version(&catcollar_firmware_version);
  }
  app_log_info("\r\n");
  app_log_info(" \\ | /\r\n");
  app_log_info("- SL -     Thread Operating System\r\n");
  app_log_info(" / | \\     %s build %s %s\r\n",
              get_app_version(), __DATE__, __TIME__);
  app_log_info("==================================\r\n");

LOG_HEAP_DELTA(1);

  //! BLE initialization
  status = rsi_ble_task_init();
  if (status != SL_STATUS_OK) {
    app_log_error("BLE Task Init Failed!! Error Code : 0x%lX\r\n", status);
  }

LOG_HEAP_DELTA(3);
  leds_init();

  spi_flash_init();
  // spi_flash_format();    //格式化SPI Flash
LOG_HEAP_DELTA(4);
  // 挂载文件系统
  storage_init();
  //需要挂载文件后初始化时间戳
  unix_timestamp_init();
LOG_HEAP_DELTA(5);
  restore_syscfg_config();

  platform_i2c_init();
  lsm6dsv_init();
  // spa06_init();
  // sy6105_init();
  // bmm350_init();
  // mlx90632_init();
  // spi_flash_test();
  
  // shellPortInit();

  // record_to_file("active", RECORD_START);

  // test_file_close();
// LOG_HEAP_DELTA(6);
  sensor_data_thread_create();

  ble_data_parse_task_init();

  chekr_service_init();

  ble_ota_task_init();

  leds_play(BLUE_LED, LEDS_FAST_BLINK);
  leds_play(GREEN_LED, LEDS_SLOW_BLINK);

  // https_upload_test();

#ifdef CATCOLLAR_DEBUG   // 调试时打开，监测栈空间剩余量
  static uint32_t last_heap_print_tick = 0;
  const uint32_t heap_print_interval = 500; // 500ms
#endif

  for (;;){
#ifdef CATCOLLAR_DEBUG   // 调试时打开，监测栈空间剩余量
    // uint32_t hightWaterMark =  uxTaskGetStackHighWaterMark(NULL);
    // app_log_debug("AppInit->hightWaterMark=%d\r\n", hightWaterMark);
    // 每500ms打印一次堆内存信息
    uint32_t current_tick = osKernelGetTickCount();
    if (current_tick - last_heap_print_tick >= heap_print_interval) {
      last_heap_print_tick = current_tick;
      
      // 获取并打印堆内存信息
      size_t free_heap = xPortGetFreeHeapSize();
      size_t min_heap = xPortGetMinimumEverFreeHeapSize();
      
      app_log_info("Heap Status: Free=%d bytes, MinEver=%d bytes\r\n", 
        free_heap, min_heap);
      }
      
      // 添加短暂延迟以降低CPU占用
      osDelay(50);
#endif

    // blinky_process_action();
  }
  

  return;
}

void app_init(void)
{
  osThreadNew((osThreadFunc_t)application, NULL, &thread_attributes);
}
