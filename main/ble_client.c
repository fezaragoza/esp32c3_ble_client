/**
 * @file ble_client.c
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


/* * * * * * * * * * * * * * * *
 * * * * * * INCLUDES * * * * *
 * * * * * * * * * * * * * * * */

/* API */
#include "ble_client.h"

/* * * * * * * * * * * * * * * *
 * * * * * * DEFINES * * * * * *
 * * * * * * * * * * * * * * * */

#define TAG     "BLE_CLIENT_DEV"    /* TAG */
#define DEBUG   1

/* * * * * * * * * * * * * * * *
 * * * * FN DECLARATIONS * * * *
 * * * * * * * * * * * * * * * */

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_evt_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t idx);

/* * * * * * * * * * * * * * * *
 * * * * * * VARIABLES * * * * *
 * * * * * * * * * * * * * * * */
static bool set_up_complete = false;

/* * * * * * * * * * * * * * * *
 * * * * * * STRUCTS * * * * * *
 * * * * * * * * * * * * * * * */

/* API Global */
ble_gatt_client_t ble_client = {
    .app_profiles = {
        [PROFILE_A_APP_ID] = {
            .gattc_cb = gattc_profile_evt_handler,
            .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
        [PROFILE_B_APP_ID] = {
            .gattc_cb = gattc_profile_evt_handler,
            .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
        [PROFILE_C_APP_ID] = {
            .gattc_cb = gattc_profile_evt_handler,
            .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
        },
    },
    .remote_dev_name = {
        // "ESP_GATTS_DEMO"
        "ESP_GATTS_DEMO_a", "ESP_GATTS_DEMO_b", "ESP_GATTS_DEMO_c"
    },
    .service_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = REMOTE_SERVICE_UUID,},
    },
    .charact_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
    },
    .notify_descr_uuid = {
        .len = ESP_UUID_LEN_16,
        .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
    },
    .char_elem_result  = { NULL, NULL, NULL },
    .descr_elem_result = { NULL, NULL, NULL},
    .connections       = { false, false, false },
    .service_found     = { false, false, false },
    .charact_count     = { 0U, 0U, 0U },
    .stop_scan_done    = false,
    .is_connecting     = false
};

/* API Locals */
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

/* * * * * * * * * * * * * * * *
 * * * * FN DEFINITIONS * * * * 
 * * * * * * * * * * * * * * * */

/* API Locals */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    gattc_profile_inst_t *app_profiles  = ble_client.app_profiles;
    // const char           **names        = ble_client.remote_dev_name;
    bool                 *conn_devices  = ble_client.connections;
    bool                 *is_connecting = &ble_client.is_connecting;
    bool                 *stop_scan     = &ble_client.stop_scan_done;

    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;

    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
            ESP_LOGI(TAG, "EVT: BLE Scan Parameters Set Completed");
            uint32_t duration = BLE_SCAN_TIME;          // The unit of the duration is second
            esp_ble_gap_start_scanning(duration);
            break;
        }

        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            ESP_LOGI(TAG, "EVT: BLE Scan Start");
            // Scan start complete event to indicate scan start successfully or failed
            if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Scan start success");
            } else{
                ESP_LOGE(TAG, "Scan start failed");
            }
            break;

        case ESP_GAP_BLE_SCAN_RESULT_EVT: {
            ESP_LOGI(TAG, "EVT: BLE Scan Result");
            esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
            switch (scan_result->scan_rst.search_evt) {
                case ESP_GAP_SEARCH_INQ_RES_EVT:
                    esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6);
                    ESP_LOGI(TAG, "Searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
                    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                        ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
                    ESP_LOGI(TAG, "Searched Device Name Len %d", adv_name_len);
                    esp_log_buffer_char(TAG, adv_name, adv_name_len);   /* Print Device Name */
                    ESP_LOGI(TAG, "\n");

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
                    if (scan_result->scan_rst.adv_data_len > 0) {
                        ESP_LOGI(TAG, "Advertised data:");
                        esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
                    }
                    if (scan_result->scan_rst.scan_rsp_len > 0) {
                        ESP_LOGI(TAG, "Scan response:");
                        esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
                    }
#endif

                    if (*is_connecting) {
                        ESP_LOGI(TAG, "BLE is connecting");
                        break;
                    }
                    if (conn_devices[PROFILE_A_APP_ID] && conn_devices[PROFILE_B_APP_ID] && !*stop_scan) {
                        *stop_scan = true;
                        esp_ble_gap_stop_scanning();
                        ESP_LOGW(TAG, "all devices are connected");
                        break;
                    }
                    if (adv_name != NULL) {
                        if (strlen(ble_client.remote_dev_name[PROFILE_A_APP_ID]) == adv_name_len && strncmp((char *)adv_name, ble_client.remote_dev_name[PROFILE_A_APP_ID], adv_name_len) == 0) {
                            if (conn_devices[PROFILE_A_APP_ID] == false) {
                                conn_devices[PROFILE_A_APP_ID] = true;
                                ESP_LOGW(TAG, "Searched device %s", ble_client.remote_dev_name[PROFILE_A_APP_ID]);
                                ESP_LOGI(TAG, "Connect to the remote device.");
                                *is_connecting = true;
                                esp_ble_gap_stop_scanning(); /* This takes some time to stop scanning, is_connecting flag will prevent to keep trying to connect to others in the meantime. */
                                esp_ble_gattc_open(app_profiles[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                            }
                            // break;
                        }
                        else if (strlen(ble_client.remote_dev_name[PROFILE_B_APP_ID]) == adv_name_len && strncmp((char *)adv_name, ble_client.remote_dev_name[PROFILE_B_APP_ID], adv_name_len) == 0) {
                            if (conn_devices[PROFILE_B_APP_ID] == false) {
                                conn_devices[PROFILE_B_APP_ID] = true;
                                ESP_LOGW(TAG, "Searched device %s", ble_client.remote_dev_name[PROFILE_B_APP_ID]);
                                ESP_LOGI(TAG, "Connect to the remote device.");
                                *is_connecting = true;
                                esp_ble_gap_stop_scanning(); /* This takes some time to stop scanning, is_connecting flag will prevent to keep trying to connect to others in the meantime. */
                                esp_ble_gattc_open(app_profiles[PROFILE_B_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                            }
                        }
                        // else if (strlen(names[PROFILE_C_APP_ID]) == adv_name_len && strncmp((char *)adv_name, names[PROFILE_C_APP_ID], adv_name_len) == 0) {
                        //     if (conn_devices[PROFILE_C_APP_ID] == false) {
                        //         conn_devices[PROFILE_C_APP_ID] = true;
                        //         ESP_LOGI(TAG, "Searched device %s", names[PROFILE_C_APP_ID]);
                        //         esp_ble_gap_stop_scanning();
                        //         esp_ble_gattc_open(app_profiles[PROFILE_C_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        //         *is_connecting = true;
                        //     }
                        //     break;
                        // }
                    }
                    break;
                case ESP_GAP_SEARCH_INQ_CMPL_EVT:
                    break;
                default:
                    break;
                } 
            break;
        }
        
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "EVT: BLE Scan Stop");
            if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE(TAG, "Scan stop failed");
                break;
            }
            ESP_LOGI(TAG, "Stop scan successfully");
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "EVT: BLE Adv Stop");
            if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
                ESP_LOGE(TAG, "Adv stop failed");
                break;
            }
            ESP_LOGI(TAG, "Stop adv successfully");
            break;
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            ESP_LOGI(TAG, "EVT: Update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                    param->update_conn_params.status,
                    param->update_conn_params.min_int,
                    param->update_conn_params.max_int,
                    param->update_conn_params.conn_int,
                    param->update_conn_params.latency,
                    param->update_conn_params.timeout);
            break;
        default:
            break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    ESP_LOGI(TAG, "EVT %d, gattc if %d, app_id %d", event, gattc_if, param->reg.app_id);

    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            ble_client.app_profiles[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == ble_client.app_profiles[idx].gattc_if) {
                if (ble_client.app_profiles[idx].gattc_cb) {
                    ble_client.app_profiles[idx].gattc_cb(event, gattc_if, param, idx);
                }
            }
        }
    } while (0);
}

static void gattc_profile_evt_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param, uint8_t idx)
{
    esp_ble_gattc_cb_param_t *p_data      = (esp_ble_gattc_cb_param_t *)param;
    profiles_app_id_t         app_id      = (profiles_app_id_t)idx;
    gattc_profile_inst_t     *app_profile = &ble_client.app_profiles[app_id];
    esp_bt_uuid_t    remfilt_service_uuid = ble_client.service_uuid;
    esp_bt_uuid_t       remfilt_char_uuid = ble_client.charact_uuid;
    esp_bt_uuid_t       notify_descr_uuid = ble_client.notify_descr_uuid;
    esp_gattc_char_elem_t      *char_elem = ble_client.char_elem_result[app_id];
    esp_gattc_descr_elem_t    *descr_elem = ble_client.descr_elem_result[app_id];
    bool                     *get_service = &ble_client.service_found[app_id];
    bool                     *conn_device = &ble_client.connections[app_id];
    uint16_t               *charact_count = &ble_client.charact_count[app_id];

    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(TAG, "REG_EVT -> app_id: %d", idx);
            /* Set scan parameters after first app register */
            if (PROFILE_C_APP_ID == app_id) {
                esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
                if (scan_ret) {
                    ESP_LOGE(TAG, "Set scan params error, error code = %x", scan_ret);
                }
            }     
            break;
        /* one device connect successfully, all profiles callback function will get the ESP_GATTC_CONNECT_EVT,
        so must compare the mac address to check which device is connected, so it is a good choice to use ESP_GATTC_OPEN_EVT. */
        case ESP_GATTC_CONNECT_EVT:
            ESP_LOGI(TAG, "EVT: Connect.");
            break;
        case ESP_GATTC_OPEN_EVT:
            ESP_LOGI(TAG, "EVT: GATTC Open.");
            if (p_data->open.status != ESP_GATT_OK) {
                // Open failed, ignore the device, connect the next device
                ESP_LOGE(TAG, "Connect device failed, status %d", p_data->open.status);
                *conn_device = false;
                //start_scan();
                break;
            }
            ESP_LOGI(TAG, "Open success");
            app_profile->conn_id = p_data->open.conn_id;
            memcpy(app_profile->remote_bda, p_data->open.remote_bda, 6);
            ESP_LOGI(TAG, "ESP_GATTC_OPEN_EVT conn_id %d, if %d, status %d, mtu %d, app_id %d", p_data->open.conn_id, gattc_if, p_data->open.status, p_data->open.mtu, app_id);
            ESP_LOGI(TAG, "REMOTE BDA:");
            esp_log_buffer_hex(TAG, p_data->open.remote_bda, sizeof(esp_bd_addr_t));
            esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->open.conn_id);
            if (mtu_ret) {
                ESP_LOGE(TAG, "Config MTU error, error code = %x", mtu_ret);
            }
            break;
        case ESP_GATTC_DIS_SRVC_CMPL_EVT:
            if (param->dis_srvc_cmpl.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
                break;
            }
            ESP_LOGI(TAG, "discover service complete conn_id %d", param->dis_srvc_cmpl.conn_id);
            // esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remfilt_service_uuid);
            break;
        case ESP_GATTC_CFG_MTU_EVT:
            if (param->cfg_mtu.status != ESP_GATT_OK) {
                ESP_LOGE(TAG,"Config mtu failed");
            }
            ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT: Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
            esp_ble_gattc_search_service(gattc_if, param->cfg_mtu.conn_id, &remfilt_service_uuid);
            break;
        case ESP_GATTC_SEARCH_RES_EVT: {
            ESP_LOGI(TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
            ESP_LOGI(TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
            if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == REMOTE_SERVICE_UUID) {
                ESP_LOGI(TAG, "service found");
                *get_service = true;
                app_profile->service_start_handle = p_data->search_res.start_handle;
                app_profile->service_end_handle   = p_data->search_res.end_handle;
                ESP_LOGI(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
            }
            break;
        }
        case ESP_GATTC_SEARCH_CMPL_EVT:
            ESP_LOGI(TAG, "EVT: Search Completed.");
            if (p_data->search_cmpl.status != ESP_GATT_OK){
                ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
                break;
            }
            if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
                ESP_LOGI(TAG, "Get service information from remote device");
            } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
                ESP_LOGI(TAG, "Get service information from flash");
            } else {
                ESP_LOGI(TAG, "unknown service source");
            }
            if (*get_service) {
                // uint16_t count = 0;
                esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                        p_data->search_cmpl.conn_id,
                                                                        ESP_GATT_DB_CHARACTERISTIC,
                                                                        app_profile->service_start_handle,
                                                                        app_profile->service_end_handle,
                                                                        INVALID_HANDLE,
                                                                        charact_count);
                if (status != ESP_GATT_OK) {
                    ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
                }
                if (*charact_count > 0) {
                    char_elem = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * (*charact_count));
                    ESP_LOGI(TAG, "Char count %d", *charact_count);
                    if (!char_elem){
                        ESP_LOGE(TAG, "gattc no mem");
                    } else {
                        status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                                p_data->search_cmpl.conn_id,
                                                                app_profile->service_start_handle,
                                                                app_profile->service_end_handle,
                                                                remfilt_char_uuid,
                                                                char_elem,
                                                                charact_count);
                        if (status != ESP_GATT_OK){
                            ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                        }

                        /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                        if (*charact_count > 0 && (char_elem[0].properties & ESP_GATT_CHAR_PROP_BIT_READ)) { // ESP_GATT_CHAR_PROP_BIT_NOTIFY
                            app_profile->char_handle = char_elem[0].char_handle;
                            // esp_ble_gattc_register_for_notify (gattc_if, app_profile->remote_bda, char_elem[0].char_handle);
                            esp_ble_gattc_read_char(app_profile->gattc_if, app_profile->conn_id, app_profile->char_handle, ESP_GATT_AUTH_REQ_NONE);
                        }
                    }
                    /* free char_elem */
                    free(char_elem);
                    set_up_complete = true;
                } else {
                    ESP_LOGE(TAG, "No char found");
                }
            }
            break;
        case ESP_GATTC_READ_CHAR_EVT:
            ESP_LOGI(TAG, "ESP_GATTC_READ_CHAR_EVT");
            if (param->read.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "read failed, status %d", p_data->read.status);
                break;
            }
            esp_log_buffer_hex(TAG, p_data->read.value, p_data->read.value_len); // esp_ble_gattc_cb_param_t
            ble_start_scan(&ble_client);
            break;
        case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
            ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
            if (p_data->reg_for_notify.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "Reg Notify failed, error status =%x", p_data->reg_for_notify.status);
                break;
            }
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                        app_profile->conn_id,
                                                                        ESP_GATT_DB_DESCRIPTOR,
                                                                        app_profile->service_start_handle,
                                                                        app_profile->service_end_handle,
                                                                        app_profile->char_handle,
                                                                        &count);
            if (ret_status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            if (count > 0) {
                descr_elem = (esp_gattc_descr_elem_t *)malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem) {
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                } else {
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                        app_profile->conn_id,
                                                                        p_data->reg_for_notify.handle,
                                                                        notify_descr_uuid,
                                                                        descr_elem,
                                                                        &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }

                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem' */
                    if (count > 0 && descr_elem[0].uuid.len == ESP_UUID_LEN_16 && descr_elem[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                    app_profile->conn_id,
                                                                    descr_elem[0].handle,
                                                                    sizeof(notify_en),
                                                                    (uint8_t *)&notify_en,
                                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                                    ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem */
                    free(descr_elem);
                }
            }
            else{
                ESP_LOGE(TAG, "decsr not found");
            }
            break;
        }
        case ESP_GATTC_NOTIFY_EVT:
            if (p_data->notify.is_notify) {
                ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, Receive notify value:");
            } else {
                ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, Receive indicate value:");
            }
            esp_log_buffer_hex(TAG, p_data->notify.value, p_data->notify.value_len);
            break;
        case ESP_GATTC_WRITE_DESCR_EVT:
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
                break;
            }
            ESP_LOGI(TAG, "write descr success");
            uint8_t write_char_data[35];
            for (int i = 0; i < sizeof(write_char_data); ++i)
            {
                write_char_data[i] = i % 256;
            }
            esp_ble_gattc_write_char( gattc_if,
                                    app_profile->conn_id,
                                    app_profile->char_handle,
                                    sizeof(write_char_data),
                                    write_char_data,
                                    ESP_GATT_WRITE_TYPE_RSP,
                                    ESP_GATT_AUTH_REQ_NONE);
            break;
        case ESP_GATTC_WRITE_CHAR_EVT:
            if (p_data->write.status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "write char failed, error status = %x", p_data->write.status);
            } else {
                ESP_LOGI(TAG, "write char success");
            }
            // ble_start_scan(&ble_client);
            break;
        case ESP_GATTC_SRVC_CHG_EVT: {
            esp_bd_addr_t bda;
            memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
            // ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:%08x%04x",(bda[0] << 24) + (bda[1] << 16) + (bda[2] << 8) + bda[3],
            //          (bda[4] << 8) + bda[5]);
            ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
            esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
            break;
        }
        case ESP_GATTC_DISCONNECT_EVT:
            //Start scanning again
            // start_scan();
            if (memcmp(p_data->disconnect.remote_bda, app_profile->remote_bda, 6) == 0){
                ESP_LOGI(TAG, "Device a disconnect");
                *conn_device = false;
                *get_service = false;
            }
            ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);
            break;
        default:
            break;
    }
}

/* API Globals */
void bt_setup(void) {
    esp_err_t ret;
    /* Initialize BT Controller */
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) {
        ESP_LOGE(TAG, "%s Initialize controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Bluetooth Controller Enable */
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) {
        ESP_LOGE(TAG, "%s Enable controller failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Initialize Bluedroid API Stack */
    ret = esp_bluedroid_init();
    if (ret) {
        ESP_LOGE(TAG, "%s Init bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }

    /* Enable Bluedroid API Stack */
    ret = esp_bluedroid_enable();
    if (ret) {
        ESP_LOGE(TAG, "%s Enable bluetooth failed: %s\n", __func__, esp_err_to_name(ret));
        return;
    }
}

void ble_register_cbs(void) {
    esp_err_t ret;
    /* Register the  callback function to the gap module */
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret){
        ESP_LOGE(TAG, "Gap register error, error code = %x", ret);
        return;
    }

    /* Register the callback function to the gattc module */
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if(ret){
        ESP_LOGE(TAG, "gattc register error, error code = %x", ret);
        return;
    }
}

void ble_setup(void) {
    esp_err_t ret;
    /* Initialize NVS Flash module - Non-Volatile Storage */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    /* Realease Memory for BT Controller */
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    /* Call BT Controller & Bluedroid Stack API */
    bt_setup();

    /* Register BLE GAP & BLE GATT Client Callbacks */
    ble_register_cbs();

}

void ble_register_app(void)
{
    esp_err_t ret = ESP_FAIL;
    /* Register number of profiles to be used in the app */
    for (uint8_t i = 0; i < PROFILE_NUM; i++)
    {
        profiles_app_id_t app_id = (profiles_app_id_t)i;
        /* Use index as argument to setup app register. */
        ret = esp_ble_gattc_app_register(app_id);
        if (ret) {
            ESP_LOGE(TAG, "Gattc app register error, error code = %x", ret);
            ESP_LOGE(TAG, "Failed to register APP_ID: %d", app_id);
            return;
        }
    }
}

void ble_set_local_mtu(uint16_t mtu)
{
    /* Run after starting BLE scan */
    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(mtu);
    if (local_mtu_ret){
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    } else {
        ESP_LOGE(TAG, "set local  MTU sucess, code = %x", local_mtu_ret);
    }
}

void ble_start_scan(ble_gatt_client_t *client)
{
    client->stop_scan_done = false;
    client->is_connecting  = false;
    esp_ble_gap_start_scanning(BLE_SCAN_TIME); // Duration in seconds;
}

void app_main(void)
{
    /* Run complete BLE setup */
    ble_setup();

    /* Register BLE App Profiles */
    ble_register_app();

    /* Setup MTU size */
    ble_set_local_mtu(500);

    // while (!set_up_complete) {
    //     vTaskDelay(10 / portTICK_PERIOD_MS);
    // }

    // esp_ble_gattc_read_char(ble_client.gattc_if[PROFILE_A_APP_ID], ble_client.gattc_if[PROFILE_A_APP_ID], app_profile->char_handle, ESP_GATT_AUTH_REQ_NONE);
    // vTaskDelay(2 / portTICK_PERIOD_MS);
    // vTaskDelay(2 / portTICK_PERIOD_MS);
    // vTaskDelay(2 / portTICK_PERIOD_MS);
    // vTaskDelay(2 / portTICK_PERIOD_MS);




}