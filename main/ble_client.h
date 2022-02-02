/**
 * @file ble_client.h
 * 
 * 
 * @author Fernando Zaragoza
 * @brief FIle that includes wrappers around the basic BLE functionality.
 *          This is intented to run as a single client to multiple servers to 
 *          retrieve data from them, by reading the characteristic of each of 
 *          the primary server service.
 * @version 0.1
 * @date 2022-01-28
 * 
 * @copyright Copyright (c) 2022
 * 
 */


#pragma once

/* * * * * * * * * * * * * * * *
 * * * * * * INCLUDES * * * * *
 * * * * * * * * * * * * * * * */

/* STD */
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
/* Non-Volatile Mem Includes */
#include "nvs.h"
#include "nvs_flash.h"

/* ESP32 API */
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
/* Vanilla FreeRTOS */
#include "freertos/FreeRTOS.h"

/* * * * * * * * * * * * * * * *
 * * * * * * DEFINES * * * * * *
 * * * * * * * * * * * * * * * */

#define REMOTE_SERVICE_UUID     0x00FF              /* Single Remote Filter Service UUID for all servers */
#define REMOTE_NOTIFY_CHAR_UUID 0xFF01              /* Single Remote Filter Characteristc UUID for all servers */


#define PROFILE_NUM     3U   /* Register three profiles, each profile corresponds to one connection which makes it easy to handle each connection event */
#define PROFILE_NUM_MAX 8U
#define INVALID_HANDLE  0U

#define BLE_SCAN_TIME   1U   // Seconds

/* * * * * * * * * * * * * * * *
 * * * * * FN TYPEDEFS * * * *
 * * * * * * * * * * * * * * * */

/* Callback function when event is triggered; substituting  "esp_gattc_cb_t" typedef*/
typedef void (* esp_gattc_cbk_t)(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t idx);

/* * * * * * * * * * * * * * * *
 * * * * * * ENUMS * * * * * * *
 * * * * * * * * * * * * * * * */

typedef enum {
    PROFILE_A_APP_ID = 0,
    PROFILE_B_APP_ID,
    PROFILE_C_APP_ID,
    PROFILE_D_APP_ID,
    PROFILE_E_APP_ID,
    PROFILE_F_APP_ID,
    PROFILE_G_APP_ID,
    PROFILE_H_APP_ID,
    PROFILE_I_APP_ID,
    PROFILE_MAX_APP_ID
} profiles_app_id_t;

/* * * * * * * * * * * * * * * *
 * * * * * * STRUCTS * * * * * *
 * * * * * * * * * * * * * * * */

typedef struct gattc_profile_inst {
    esp_gattc_cbk_t gattc_cb;
    uint16_t        gattc_if;
    uint16_t        app_id;
    uint16_t        conn_id;
    uint16_t        service_start_handle;
    uint16_t        service_end_handle;
    uint16_t        char_handle;
    esp_bd_addr_t   remote_bda;
} gattc_profile_inst_t;

typedef struct ble_gatt_client
{
    gattc_profile_inst_t    app_profiles[PROFILE_NUM];
    const char              *remote_dev_name[PROFILE_NUM];
    esp_bt_uuid_t           service_uuid;                   /* Single Remote Filter Service UUID for all servers */
    esp_bt_uuid_t           charact_uuid;                   /* Same characteristic UUID for all services */
    esp_bt_uuid_t           notify_descr_uuid;              /* Same description notify UUID for all services */
    esp_gattc_char_elem_t  *char_elem_result[PROFILE_NUM];
    esp_gattc_descr_elem_t *descr_elem_result[PROFILE_NUM];
    bool                    connections[PROFILE_NUM];
    bool                    service_found[PROFILE_NUM];
    bool                    stop_scan_done;
    bool                    is_connecting;
} ble_gatt_client_t;

extern ble_gatt_client_t ble_client;

/***/
void bt_setup(void);

void ble_register_cbs(void);

void ble_setup(void);

void start_scan(void);

void ble_register_app(void);

void ble_start_scan(ble_gatt_client_t *client);
