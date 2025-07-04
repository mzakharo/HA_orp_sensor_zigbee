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

#include "esp_zigbee_core.h"

#define ESP_ORP_SENSOR_UPDATE_INTERVAL  (15)    /* Local sensor update interval (15 seconds) */
#define ESP_ORP_SENSOR_MIN_VALUE        (100)   /* Local sensor min measured value (millivolts) */
#define ESP_ORP_SENSOR_MAX_VALUE        (4000)  /* Local sensor max measured value (millivolts) */

/* Zigbee configuration */
#define INSTALLCODE_POLICY_ENABLE       false   /* enable the install code policy for security */
#define ED_AGING_TIMEOUT                ESP_ZB_ED_AGING_TIMEOUT_64MIN
#define ED_KEEP_ALIVE                   (ESP_ORP_SENSOR_UPDATE_INTERVAL * 1000)
#define HA_ESP_SENSOR_ENDPOINT          10      /* esp ORP sensor device endpoint, used for ORP measurement */
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK    /* Zigbee primary channel mask use in the example */


/* ORP calibration using maxPresentValue attribute in Analog Input cluster */
#define ESP_ZB_ZCL_ATTR_ORP_CALIBRATION_ID    ESP_ZB_ZCL_ATTR_ANALOG_INPUT_MAX_PRESENT_VALUE_ID  /* Use maxPresentValue for calibration */
#define ESP_ORP_CALIBRATION_MIN_VALUE   (-500)  /* Minimum calibration offset (millivolts) */
#define ESP_ORP_CALIBRATION_MAX_VALUE   (500)   /* Maximum calibration offset (millivolts) */

/* Attribute values in ZCL string format
 * The string should be started with the length of its own.
 */
#define MANUFACTURER_NAME               "\x09""ESPRESSIF"
#define MODEL_IDENTIFIER                "\x07"CONFIG_IDF_TARGET

#define ESP_ZB_ZED_CONFIG()                                         \
    {                                                               \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ED,                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,           \
        .nwk_cfg.zed_cfg = {                                        \
            .ed_timeout = ED_AGING_TIMEOUT,                         \
            .keep_alive = ED_KEEP_ALIVE,                            \
        },                                                          \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                     \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,   \
    }
