/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_orp_sensor Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include "esp_zb_orp_sensor.h"
#include "orp_sensor_driver.h"
#include "switch_driver.h"

#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include <stdlib.h>  /* For abs() function */
#ifdef CONFIG_PM_ENABLE
#include "esp_pm.h"
#include "esp_private/esp_clk.h"
#include "esp_sleep.h"
#endif
#include "driver/rtc_io.h"
#include "driver/gpio.h"

#if !defined ZB_ED_ROLE
#error Define ZB_ED_ROLE in idf.py menuconfig to compile sensor (End Device) source code.
#endif

static const char *TAG = "ESP_ZB_ORP_SENSOR";

static switch_func_pair_t button_func_pair[] = {
    {GPIO_INPUT_IO_TOGGLE_SWITCH, SWITCH_ONOFF_TOGGLE_CONTROL}
};

/* Helper function to convert ZCL status code to string */
static const char* esp_zb_zcl_status_to_string(uint8_t status_code)
{
    switch (status_code) {
        case ESP_ZB_ZCL_STATUS_SUCCESS: return "SUCCESS";
        case ESP_ZB_ZCL_STATUS_FAIL: return "FAIL";
        case ESP_ZB_ZCL_STATUS_NOT_AUTHORIZED: return "NOT_AUTHORIZED";
        case ESP_ZB_ZCL_STATUS_MALFORMED_CMD: return "MALFORMED_CMD";
        case ESP_ZB_ZCL_STATUS_UNSUP_CLUST_CMD: return "UNSUP_CLUST_CMD";
        case ESP_ZB_ZCL_STATUS_UNSUP_GEN_CMD: return "UNSUP_GEN_CMD";
        case ESP_ZB_ZCL_STATUS_UNSUP_MANUF_CLUST_CMD: return "UNSUP_MANUF_CLUST_CMD";
        case ESP_ZB_ZCL_STATUS_UNSUP_MANUF_GEN_CMD: return "UNSUP_MANUF_GEN_CMD";
        case ESP_ZB_ZCL_STATUS_INVALID_FIELD: return "INVALID_FIELD";
        case ESP_ZB_ZCL_STATUS_UNSUP_ATTRIB: return "UNSUP_ATTRIB";
        case ESP_ZB_ZCL_STATUS_INVALID_VALUE: return "INVALID_VALUE";
        case ESP_ZB_ZCL_STATUS_READ_ONLY: return "READ_ONLY";
        case ESP_ZB_ZCL_STATUS_INSUFF_SPACE: return "INSUFF_SPACE";
        case ESP_ZB_ZCL_STATUS_DUPE_EXISTS: return "DUPE_EXISTS";
        case ESP_ZB_ZCL_STATUS_NOT_FOUND: return "NOT_FOUND";
        case ESP_ZB_ZCL_STATUS_UNREPORTABLE_ATTRIB: return "UNREPORTABLE_ATTRIB";
        case ESP_ZB_ZCL_STATUS_INVALID_TYPE: return "INVALID_TYPE";
        case ESP_ZB_ZCL_STATUS_WRITE_ONLY: return "WRITE_ONLY";
        case ESP_ZB_ZCL_STATUS_INCONSISTENT: return "INCONSISTENT";
        case ESP_ZB_ZCL_STATUS_ACTION_DENIED: return "ACTION_DENIED";
        case ESP_ZB_ZCL_STATUS_TIMEOUT: return "TIMEOUT";
        case ESP_ZB_ZCL_STATUS_ABORT: return "ABORT";
        case ESP_ZB_ZCL_STATUS_INVALID_IMAGE: return "INVALID_IMAGE";
        case ESP_ZB_ZCL_STATUS_WAIT_FOR_DATA: return "WAIT_FOR_DATA";
        case ESP_ZB_ZCL_STATUS_NO_IMAGE_AVAILABLE: return "NO_IMAGE_AVAILABLE";
        case ESP_ZB_ZCL_STATUS_REQUIRE_MORE_IMAGE: return "REQUIRE_MORE_IMAGE";
        case ESP_ZB_ZCL_STATUS_NOTIFICATION_PENDING: return "NOTIFICATION_PENDING";
        case ESP_ZB_ZCL_STATUS_HW_FAIL: return "HW_FAIL";
        case ESP_ZB_ZCL_STATUS_SW_FAIL: return "SW_FAIL";
        case ESP_ZB_ZCL_STATUS_CALIB_ERR: return "CALIB_ERR";
        case ESP_ZB_ZCL_STATUS_UNSUP_CLUST: return "UNSUP_CLUST";
        case ESP_ZB_ZCL_STATUS_LIMIT_REACHED: return "LIMIT_REACHED";
        default: return "UNKNOWN_STATUS";
    }
}

static esp_err_t esp_zb_power_save_init(void)
{
    esp_err_t rc = ESP_OK;
#ifdef CONFIG_PM_ENABLE
    int cur_cpu_freq_mhz = CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    esp_pm_config_t pm_config = {
        .max_freq_mhz = cur_cpu_freq_mhz,
        .min_freq_mhz = cur_cpu_freq_mhz,
#if CONFIG_FREERTOS_USE_TICKLESS_IDLE
        .light_sleep_enable = true
#endif
    };
    rc = esp_pm_configure(&pm_config);
#endif
    return rc;
}

/* ZCL attribute write callback for handling calibration updates */
static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        {
            const esp_zb_zcl_set_attr_value_message_t *set_attr_message = (esp_zb_zcl_set_attr_value_message_t*)message;
            ESP_RETURN_ON_FALSE(set_attr_message, ESP_FAIL, TAG, "Empty message");
            ESP_RETURN_ON_FALSE(set_attr_message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                                set_attr_message->info.status);

            ESP_LOGI(TAG, "Received ZCL attribute write: endpoint(0x%x), cluster(0x%x), attribute(0x%x), data size(%d)",
                     set_attr_message->info.dst_endpoint, set_attr_message->info.cluster, set_attr_message->attribute.id, set_attr_message->attribute.data.size);

            /* Handle ORP calibration attribute in Analog Input cluster */
            if (set_attr_message->info.dst_endpoint == HA_ESP_SENSOR_ENDPOINT &&
                set_attr_message->info.cluster == ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT &&
                set_attr_message->attribute.id == ESP_ZB_ZCL_ATTR_ORP_CALIBRATION_ID) {
                
                if (set_attr_message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_SINGLE) {
                    float calibration_offset_f = *(float*)set_attr_message->attribute.data.value;
                    int calibration_offset = (int)calibration_offset_f;
                    
                    /* Validate calibration range */
                    if (calibration_offset >= ESP_ORP_CALIBRATION_MIN_VALUE && 
                        calibration_offset <= ESP_ORP_CALIBRATION_MAX_VALUE) {
                        
                        /* Apply calibration to ORP sensor */
                        ret = orp_sensor_set_calibration(calibration_offset);
                        if (ret == ESP_OK) {
                            ESP_LOGI(TAG, "ORP calibration set to: %d mV", calibration_offset);
                        } else {
                            ESP_LOGE(TAG, "Failed to set ORP calibration");
                        }
                    } else {
                        ESP_LOGW(TAG, "Calibration value %d out of range [%d, %d]", 
                                 calibration_offset, ESP_ORP_CALIBRATION_MIN_VALUE, ESP_ORP_CALIBRATION_MAX_VALUE);
                        ret = ESP_ERR_INVALID_ARG;
                    }
                } else {
                    ESP_LOGW(TAG, "Invalid data type for calibration attribute: %d", set_attr_message->attribute.data.type);
                    ret = ESP_ERR_INVALID_ARG;
                }
            }
        }
        break;
    case ESP_ZB_CORE_CMD_DEFAULT_RESP_CB_ID:
        {
            const esp_zb_zcl_cmd_default_resp_message_t *default_resp = (esp_zb_zcl_cmd_default_resp_message_t*)message;
            ESP_RETURN_ON_FALSE(default_resp, ESP_FAIL, TAG, "Empty default response message");
            
            const char *status_str = esp_zb_zcl_status_to_string(default_resp->status_code);
            ESP_LOGI(TAG, "Default response received: endpoint(0x%x), cluster(0x%x), status_code(0x%x): %s",
                     default_resp->info.dst_endpoint, default_resp->info.cluster, 
                      default_resp->status_code, status_str);
        }
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

static void esp_app_buttons_handler(switch_func_pair_t *button_func_pair)
{
    if (button_func_pair->func == SWITCH_ONOFF_TOGGLE_CONTROL) {
        /* Send report attributes command */
        esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {0};
        report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
        report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
        report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
        report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
        report_attr_cmd.zcl_basic_cmd.src_endpoint = HA_ESP_SENSOR_ENDPOINT;

        esp_zb_lock_acquire(portMAX_DELAY);
        esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
        esp_zb_lock_release();
        ESP_EARLY_LOGI(TAG, "Send 'report attributes' command");
    }
}

static void esp_app_orp_sensor_handler(int orp_mv)
{
    float orp_value = (float)orp_mv;
    
    /* Update ORP sensor measured value */
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(HA_ESP_SENSOR_ENDPOINT,
        ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID, &orp_value, false);
    
    /* Always send report every 15 seconds */
    esp_zb_zcl_report_attr_cmd_t report_attr_cmd = {0};
    report_attr_cmd.address_mode = ESP_ZB_APS_ADDR_MODE_DST_ADDR_ENDP_NOT_PRESENT;
    report_attr_cmd.attributeID = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID;
    report_attr_cmd.direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI;
    report_attr_cmd.clusterID = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT;
    report_attr_cmd.zcl_basic_cmd.src_endpoint = HA_ESP_SENSOR_ENDPOINT;
    
    esp_err_t ret = esp_zb_zcl_report_attr_cmd_req(&report_attr_cmd);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send attribute report: %s", esp_err_to_name(ret));
    }
    esp_zb_lock_release();
    
    ESP_LOGI(TAG, "ORP sensor value: %d mV [REPORTED]", orp_mv);
}

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_RETURN_ON_FALSE(esp_zb_bdb_start_top_level_commissioning(mode_mask) == ESP_OK, ,
                        TAG, "Failed to start Zigbee bdb commissioning");
}

static esp_err_t deferred_driver_init(void)
{
    static bool is_inited = false;
    orp_sensor_config_t orp_sensor_config = ORP_SENSOR_CONFIG_DEFAULT();
    if (!is_inited) {
        ESP_RETURN_ON_ERROR(
            orp_sensor_driver_init(&orp_sensor_config, ESP_ORP_SENSOR_UPDATE_INTERVAL, esp_app_orp_sensor_handler),
            TAG, "Failed to initialize ORP sensor");
        ESP_RETURN_ON_FALSE(switch_driver_init(button_func_pair, PAIR_SIZE(button_func_pair), esp_app_buttons_handler),
                            ESP_FAIL, TAG, "Failed to initialize switch driver");
        is_inited = true;
    }
    return is_inited ? ESP_OK : ESP_FAIL;
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p     = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Initialize Zigbee stack");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Deferred driver initialization %s", deferred_driver_init() ? "failed" : "successful");
            ESP_LOGI(TAG, "Device started up in%s factory-reset mode", esp_zb_bdb_is_factory_new() ? "" : " non");
            if (esp_zb_bdb_is_factory_new()) {
                ESP_LOGI(TAG, "Start network steering");
                esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
            } else {
                ESP_LOGI(TAG, "Device rebooted");
            }
        } else {
            ESP_LOGW(TAG, "%s failed with status: %s, retrying", esp_zb_zdo_signal_to_string(sig_type),
                     esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb,
                                   ESP_ZB_BDB_MODE_INITIALIZATION, 1000);
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
      case ESP_ZB_COMMON_SIGNAL_CAN_SLEEP:
        {
            esp_zb_zdo_signal_can_sleep_params_t *sleep_params = (esp_zb_zdo_signal_can_sleep_params_t *)esp_zb_app_signal_get_params(p_sg_p);
            if (sleep_params) {
                ESP_LOGI(TAG, "Zigbee can sleep for %lu ms", sleep_params->sleep_duration);
            } else {
                ESP_LOGI(TAG, "Zigbee can sleep");
            }
            esp_zb_sleep_now();
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type,
                 esp_err_to_name(err_status));
        break;
    }
}

static esp_zb_cluster_list_t *custom_orp_sensor_clusters_create(esp_zb_analog_input_cluster_cfg_t *analog_input_cfg)
{
    esp_zb_cluster_list_t *cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_attribute_list_t *basic_cluster = esp_zb_basic_cluster_create(NULL);
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME));
    ESP_ERROR_CHECK(esp_zb_basic_cluster_add_attr(basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER));
    
    /* Create analog input cluster and add calibration attribute */
    esp_zb_attribute_list_t *analog_input_cluster = esp_zb_analog_input_cluster_create(analog_input_cfg);
    
    /* Add calibration attribute to analog input cluster using maxPresentValue */
    float calibration_default = 0.0f;  /* Default calibration offset */
    ESP_ERROR_CHECK(esp_zb_cluster_add_attr(analog_input_cluster, ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, 
        ESP_ZB_ZCL_ATTR_ORP_CALIBRATION_ID, ESP_ZB_ZCL_ATTR_TYPE_SINGLE, 
        ESP_ZB_ZCL_ATTR_ACCESS_READ_WRITE, &calibration_default));
    
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_basic_cluster(cluster_list, basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_identify_cluster_create(NULL), ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_identify_cluster(cluster_list, esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY), ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE));
    ESP_ERROR_CHECK(esp_zb_cluster_list_add_analog_input_cluster(cluster_list, analog_input_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE));
    return cluster_list;
}

static esp_zb_ep_list_t *custom_orp_sensor_ep_create(uint8_t endpoint_id, esp_zb_analog_input_cluster_cfg_t *analog_input_cfg)
{
    esp_zb_ep_list_t *ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = endpoint_id,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(ep_list, custom_orp_sensor_clusters_create(analog_input_cfg), endpoint_config);
    return ep_list;
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack with Zigbee end-device config */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZED_CONFIG();
    /* Enable zigbee light sleep */
    esp_zb_sleep_enable(true);
    esp_zb_init(&zb_nwk_cfg);

    /* Create customized ORP sensor endpoint */
    esp_zb_analog_input_cluster_cfg_t analog_input_cfg = {
        .out_of_service = false,
        .status_flags = 0
    };
    esp_zb_ep_list_t *esp_zb_sensor_ep = custom_orp_sensor_ep_create(HA_ESP_SENSOR_ENDPOINT, &analog_input_cfg);

    /* Register the device */
    esp_zb_device_register(esp_zb_sensor_ep);

    /* Register ZCL attribute write handler */
    esp_zb_core_action_handler_register(zb_action_handler);

    /* Initialize calibration attribute with current value from NVS */
    int current_calibration = 0;
    if (orp_sensor_get_calibration(&current_calibration) == ESP_OK) {
        float calibration_value = (float)current_calibration;
        esp_zb_zcl_set_attribute_val(HA_ESP_SENSOR_ENDPOINT,
            ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
            ESP_ZB_ZCL_ATTR_ORP_CALIBRATION_ID, &calibration_value, false);
        ESP_LOGI(TAG, "Initialized calibration attribute with current value: %d mV", current_calibration);
    }

    /* Config the reporting info for automatic reporting */
    esp_zb_zcl_reporting_info_t reporting_info = {
        .direction = ESP_ZB_ZCL_CMD_DIRECTION_TO_CLI,
        .ep = HA_ESP_SENSOR_ENDPOINT,
        .cluster_id = ESP_ZB_ZCL_CLUSTER_ID_ANALOG_INPUT,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
        .dst.profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .u.send_info.min_interval = ESP_ORP_SENSOR_UPDATE_INTERVAL,     /* Minimum interval matches sensor update */
        .u.send_info.max_interval = ESP_ORP_SENSOR_UPDATE_INTERVAL * 2, /* Maximum interval is 2x sensor update */
        .u.send_info.def_min_interval = ESP_ORP_SENSOR_UPDATE_INTERVAL,
        .u.send_info.def_max_interval = ESP_ORP_SENSOR_UPDATE_INTERVAL * 2,
        .u.send_info.delta.u16 = 0,        /* Always report (no delta threshold) */
        .attr_id = ESP_ZB_ZCL_ATTR_ANALOG_INPUT_PRESENT_VALUE_ID,
        .manuf_code = ESP_ZB_ZCL_ATTR_NON_MANUFACTURER_SPECIFIC,
    };

    esp_err_t ret = esp_zb_zcl_update_reporting_info(&reporting_info);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to configure reporting info: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Configured automatic reporting: min=%ds, max=%ds, delta=%dmV", 
                 reporting_info.u.send_info.min_interval, 
                 reporting_info.u.send_info.max_interval,
                 reporting_info.u.send_info.delta.u16);
    }

    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_stack_main_loop();
}

void app_main(void)
{
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    /* esp zigbee light sleep initialization*/
    ESP_ERROR_CHECK(esp_zb_power_save_init());
    /* load Zigbee platform config to initialization */
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 5, NULL);
}
