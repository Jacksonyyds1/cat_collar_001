/*
 * common.c
 *
 *  Created on: 2025年7月18日
 *      Author: YQ05165
 */
#include "cmsis_os2.h"
#include "sl_utility.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "time.h"

#include "syscfg_id.h"
#include "wifi_app.h"
#include "stdio.h"
#include "common.h"
#include "sl_sleeptimer.h"
#include "sl_si91x_calendar.h"
#include "storage_api.h"
#include "app_log.h"

// #define DISABLE_COMMON_LOG

#ifdef DISABLE_COMMON_LOG
#undef app_log_info
#undef app_log_debug
// #undef app_log_error
#define app_log_info(...)
#define app_log_debug(...)
// #define app_log_error(...)
#endif

//static SysInfo sSystemInfo = { 0 };
static SysStatus sSystemStatus = {
    .imu_status = SYSSTATUS_IMU_ERROR,
    .airPress_status = SYSSTATUS_AIRPRESS_ERROR,
    .spiFlash_status = SYSSTATUS_SPIFLASH_ERROR,
    .wifi_status = SYSSTATUS_WIFI_ERROR,
    };
    
SysStatus* getSystemStatus(void)
{
  return &sSystemStatus;
}

void setSystemStatus(SysStatusType type, SysStatusValue status)
{
  switch(type)
  {
    case SST_IMU:
      if(sSystemStatus.imu_status != status)
        sSystemStatus.imu_status = status;
      break;

    case SST_AIRPRESS:
      if(sSystemStatus.airPress_status != status)
        sSystemStatus.airPress_status = status;
      break;

    case SST_SPIFLASH:
      if(sSystemStatus.spiFlash_status != status)
        sSystemStatus.spiFlash_status = status;
      break;

    case SST_WIFI:
      if(sSystemStatus.wifi_status != status)
        sSystemStatus.wifi_status = status;
      break;
    default:
      break;
  }
}

const char* get_app_version(void)
{
  static char szVersion[10] = { 0 };
  uint8_t majorVersion = CATCOLLAR_APPLICATION_MAJOR_VERSION;
  uint8_t minorVersion = CATCOLLAR_APPLICATION_MINOR_VERSION;
  uint8_t patchVersion = CATCOLLAR_APPLICATION_PATCH_VERSION;

  snprintf(szVersion, sizeof(szVersion), "V%d.%01d.%01d",majorVersion,minorVersion,patchVersion);

  return szVersion;
}

static SemaphoreHandle_t xMutex_timestamp;
static int8_t IsSetUnixTimeStamp_Flag = 0;

#define TEST_CENTURY      2u
#define TEST_YEAR         25u
#define TEST_MONTH        September
#define TEST_DAY_OF_WEEK  Friday
#define TEST_DAY          12u
#define TEST_HOUR         21u
#define TEST_MINUTE       34u
#define TEST_SECONDS      10u
#define TEST_MILLISECONDS 100u

void unix_timestamp_init(void)
{
  sl_calendar_datetime_config_t datetime_config;
  sl_status_t status;

  // Initialization of calendar
  sl_si91x_calendar_init();

  //Setting datetime for Calendar
  status = sl_si91x_calendar_build_datetime_struct(&datetime_config,
                                                    TEST_CENTURY,
                                                    TEST_YEAR,
                                                    TEST_MONTH,
                                                    TEST_DAY_OF_WEEK,
                                                    TEST_DAY,
                                                    TEST_HOUR,
                                                    TEST_MINUTE,
                                                    TEST_SECONDS,
                                                    TEST_MILLISECONDS);
  if (status != SL_STATUS_OK) {
    app_log_error("sl_si91x_calendar_build_datetime_struct: Invalid Parameters, Error Code : %lu\r\n", status);
  }

  status = sl_si91x_calendar_set_date_time(&datetime_config);
  if (status != SL_STATUS_OK) {
    app_log_error("sl_si91x_calendar_set_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
  }

  // // Printing datetime for Calendar
  // status = sl_si91x_calendar_get_date_time(&get_datetime);
  // if (status != SL_STATUS_OK) {
  //   app_log_debug("sl_si91x_calendar_get_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
  // }
  // app_log_debug("Successfully fetched the calendar datetime \r\n");
  // calendar_print_datetime(get_datetime);

  // /** Demo APIs related to Unix timestamp conversions */
  // datetime_for_unix_demo = get_datetime;
  // sl_si91x_calendar_convert_calendar_datetime_to_unix_time(&datetime_for_unix_demo, &unix_timestamp);
  // app_log_debug("\r\nIts equivalent Unix timestamp: %lu\r\n", unix_timestamp);

  // unix_timestamp += 300; // increment by 5min (300 in sec), to demo Unix-to-calendar conversion
  // app_log_debug("\r\nUnix Timestamp incremented by 5min");
  // app_log_debug("\r\nIncremented Unix timestamp: %lu\r\n", unix_timestamp);
  // sl_si91x_calendar_convert_unix_time_to_calendar_datetime(unix_timestamp, &datetime_for_unix_demo);
  // app_log_debug("\r\nUnix to Calendar datetime conversion:\r\n");
  // app_log_debug("Time Format: hour:%d, min:%d, sec:%d, msec:%d\r\n",
  //           datetime_for_unix_demo.Hour,
  //           datetime_for_unix_demo.Minute,
  //           datetime_for_unix_demo.Second,
  //           datetime_for_unix_demo.MilliSeconds);
  // app_log_debug("Date Format: DayOfWeek DD/MM/YY: %.2d %.2d/%.2d/%.2d ",
  //           datetime_for_unix_demo.DayOfWeek,
  //           datetime_for_unix_demo.Day,
  //           datetime_for_unix_demo.Month,
  //           datetime_for_unix_demo.Year);
  // app_log_debug(" Century: %d in GMT Time Zone\r\n", datetime_for_unix_demo.Century);

  xMutex_timestamp = xSemaphoreCreateMutex();
  if (xMutex_timestamp == NULL){
      app_log_error("initUnitTimeStamp: create xMutex error\r\r\n");
      return;
  }
}

// /*******************************************************************************
//  * Function to print date and time from given structure
//  * 
//  * @param[in] data pointer to the datetime structure
//  * @return none
//  ******************************************************************************/
// static void calendar_print_datetime(sl_calendar_datetime_config_t data)
// {
//   app_log_info("***Calendar time****\r\n");
//   app_log_info("Time Format: hour:%d, min:%d, sec:%d, msec:%d\r\n", data.Hour, data.Minute, data.Second, data.MilliSeconds);
//   app_log_info("Date Format: DayOfWeek DD/MM/YY: %.2d %.2d/%.2d/%.2d ", data.DayOfWeek, data.Day, data.Month, data.Year);
//   app_log_info(" Century: %d\r\n", data.Century);
// }


int set_unix_timestamp(uint64_t timestamp)
{
  if (!xMutex_timestamp) return -1;

  sl_calendar_datetime_config_t datetime_config;
  sl_status_t status;

  xSemaphoreTake(xMutex_timestamp, portMAX_DELAY);
  sl_si91x_calendar_convert_unix_time_to_calendar_datetime(timestamp, &datetime_config);
  status = sl_si91x_calendar_set_date_time(&datetime_config);
  IsSetUnixTimeStamp_Flag = 1;
  xSemaphoreGive(xMutex_timestamp);

  app_log_debug("\r\nUnix to Calendar datetime conversion:\r\n");
  app_log_debug(" Year: %d\r\n", datetime_config.Year);
  app_log_debug(" Month: %d\r\n", datetime_config.Month);
  app_log_debug(" Day: %d\r\n", datetime_config.Day);
  app_log_debug(" Hour: %d\r\n", datetime_config.Hour);
  app_log_debug(" Minute: %d\r\n", datetime_config.Minute);
  app_log_debug(" Second: %d\r\n", datetime_config.Second);
  app_log_debug(" Millisecond: %d\r\n", datetime_config.MilliSeconds);
  app_log_debug(" Century: %d\r\n", datetime_config.Century);
  app_log_debug("\r\n");

  if (status != SL_STATUS_OK) {
    app_log_error("sl_si91x_calendar_set_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
    return status;
  }
  return 0;
}

uint64_t get_unix_timestamp(void)
{
  sl_calendar_datetime_config_t get_datetime;
  uint64_t unix_timestamp;
  sl_status_t status;

  status = sl_si91x_calendar_get_date_time(&get_datetime);
  if (status != SL_STATUS_OK) {
    app_log_error("sl_si91x_calendar_get_date_time: Invalid Parameters, Error Code : %lu \r\n", status);
  }
  sl_si91x_calendar_convert_calendar_datetime_to_unix_time(&get_datetime, &unix_timestamp);
  return unix_timestamp;
}

// uint64_t get_unix_timestamp_ms(void)
// {
//   return ((uint64_t)get_unix_timestamp() * 1000);
// }

int IsSetUnixTimeStamp(void)
{
  return IsSetUnixTimeStamp_Flag;
}

// void update_unix_timestamp(void)
// {
//   static uint32_t lastTick_ms = 0;

//   if (!xMutex_timestamp) return;
//   xSemaphoreTake(xMutex_timestamp, portMAX_DELAY);

//   xSemaphoreGive(xMutex_timestamp);
// }

const char* get_unix_timestamp_string(void)
{
    static char strTimeString[30] = { 0 };
    struct tm *tm_info;
    time_t timestamp = (time_t)get_unix_timestamp();
    tm_info = gmtime(&timestamp);
    strftime(strTimeString, 20, "%Y-%m-%d %H:%M:%S", tm_info);
    return (const char*)strTimeString;
}

/**
 * @brief Convert 16-bit integer from host byte order to big-endian
 * 
 * @param val 16-bit integer in host byte order
 * @return uint16_t Value in big-endian byte order
 */
uint16_t sys_cpu_to_be16(uint16_t val)
{
    return ((val & 0x00FF) << 8) |  // 低位字节移到高位
           ((val & 0xFF00) >> 8);   // 高位字节移到低位
}
uint16_t sys_be16_to_cpu(uint16_t val)
{
    return sys_cpu_to_be16(val); // 16位转换是对称的
}

// 32位转换函数
uint32_t sys_cpu_to_be32(uint32_t val)
{
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
}
uint32_t sys_be32_to_cpu(uint32_t val)
{
    return sys_cpu_to_be32(val); // 32位转换是对称的
}

// 64位转换函数
uint64_t sys_cpu_to_be64(uint64_t val)
{
    return ((val & 0x00000000000000FFULL) << 56) |
           ((val & 0x000000000000FF00ULL) << 40) |
           ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x00000000FF000000ULL) << 8)  |
           ((val & 0x000000FF00000000ULL) >> 8)  |
           ((val & 0x0000FF0000000000ULL) >> 24) |
           ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0xFF00000000000000ULL) >> 56);
}
uint64_t sys_be64_to_cpu(uint64_t val)
{
    return sys_cpu_to_be64(val); // 64位转换是对称的
}


#define POLYNOMIAL    0xA001
#define INITIAL_VALUE 0xFFFF
uint16_t utils_crc16_modbus(const uint8_t *data, uint16_t length)
{
	uint16_t crc = INITIAL_VALUE;
	uint16_t i, j;

	for (i = 0; i < length; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			bool lsb = crc & 1;
			crc >>= 1;
			if (lsb) {
				crc ^= POLYNOMIAL;
			}
		}
	}

	// crc is always big endian
	crc = sys_cpu_to_be16(crc);
	return crc;
}

uint16_t test_utils_crc16_modbus(const uint8_t *data, uint16_t length)
{
	uint16_t crc = INITIAL_VALUE;
	uint16_t i, j;

	for (i = 0; i < length; i++) {
		crc ^= data[i];
		for (j = 0; j < 8; j++) {
			bool lsb = crc & 1;
			crc >>= 1;
			if (lsb) {
				crc ^= POLYNOMIAL;
			}
		}
	}

	// // crc is always big endian
	// crc = sys_cpu_to_be16(crc);
	// return (crc << 8) | (crc >> 8);
	return crc;
}

// Base64编码表
static const char base64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char* base64_encode(const uint8_t *data, size_t input_length, size_t *output_length) 
{
    *output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = malloc(*output_length + 1);
    if (!encoded_data) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? data[i++] : 0;
        uint32_t octet_b = i < input_length ? data[i++] : 0;
        uint32_t octet_c = i < input_length ? data[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        encoded_data[j++] = base64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = base64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = base64_table[triple & 0x3F];
    }

    // 添加填充
    for (size_t i = 0; i < (3 - input_length % 3) % 3; i++)
        encoded_data[*output_length - 1 - i] = '=';

    encoded_data[*output_length] = '\0';
    return encoded_data;
}

void update_wifi_cfg_from_syscfg()
{
    bsp_wifi_info_t wifi_cfg = {0};

    int ssid_ret = syscfg_read(VM_WIFI_SSID_ADDR, wifi_cfg.ssid, sizeof(wifi_cfg.ssid));
    int pwd_ret = syscfg_read(VM_WIFI_PWD_ADDR, wifi_cfg.pwd, sizeof(wifi_cfg.pwd));
  
    if (ssid_ret > 0 && pwd_ret > 0) {
        wifi_cfg.ssid[sizeof(wifi_cfg.ssid) - 1] = '\0';
        wifi_cfg.pwd[sizeof(wifi_cfg.pwd) - 1] = '\0';
        
        // 更新全局配置
        memcpy(&device_access_point_info, &wifi_cfg, sizeof(wifi_cfg));
        app_log_info("Restored WiFi config from syscfg: SSID=%s\r\n", device_access_point_info.ssid);
    } else {
        app_log_info("Using default WiFi config: SSID=%s\r\n", device_access_point_info.ssid);
        
        // 保存默认配置到syscfg
        syscfg_write(VM_WIFI_SSID_ADDR, device_access_point_info.ssid, 
                    strlen(device_access_point_info.ssid) + 1);
        syscfg_write(VM_WIFI_PWD_ADDR, device_access_point_info.pwd, 
                    strlen(device_access_point_info.pwd) + 1);
    }
}

void restore_syscfg_config()
{
    sys_config_info_t sys_cfg;
    memset(&sys_cfg, 0x00, sizeof(sys_cfg));
    syscfg_read(VM_SYS_CONFIG_ADDR, &sys_cfg, sizeof(sys_cfg));
    if(sys_cfg.magic != SYS_CONFIG_MAGIC){
          sys_cfg.magic = SYS_CONFIG_MAGIC;
          sys_cfg.device_upgrade_flag = 0;
          sys_cfg.fw_version = CATCOLLAR_FW_VERSION;
          sys_cfg.auto_shutdown_timer = AUTO_SHUTDOWN_TIMER_DEFAULT;
          syscfg_write(VM_SYS_CONFIG_ADDR, &sys_cfg, sizeof(sys_cfg));
    }else {
        app_log_info("\r\n magic: %d\r\n fw_version: %d\r\n auto_shutdown_timer: %dmin\r\n",
        sys_cfg.magic, sys_cfg.fw_version, sys_cfg.auto_shutdown_timer);
    }

    update_wifi_cfg_from_syscfg();
}

