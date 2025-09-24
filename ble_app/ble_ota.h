#ifndef BLE_OTA_H__
#define BLE_OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "time.h"

// #include <stdint.h>
extern QueueHandle_t ble_ota_data_queue;

void rsi_ble_add_ota_fwup_serv(void);
void ble_ota_task_init(void);

#endif /* BLE_OTA_H__ */