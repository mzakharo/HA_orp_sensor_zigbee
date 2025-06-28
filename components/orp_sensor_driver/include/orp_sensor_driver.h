/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee ORP sensor driver example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#pragma once

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** ORP sensor configuration */
typedef struct {
    adc_unit_t adc_unit;        /*!< ADC unit */
    adc_channel_t adc_channel;  /*!< ADC channel */
    adc_atten_t adc_atten;      /*!< ADC attenuation */
    int min_value_mv;           /*!< Minimum ORP value in mV */
    int max_value_mv;           /*!< Maximum ORP value in mV */
} orp_sensor_config_t;

/** ORP sensor callback
 *
 * @param[in] orp_mv ORP value in millivolts from sensor
 *
 */
typedef void (*esp_orp_sensor_callback_t)(int orp_mv);

/**
 * @brief Default ORP sensor configuration
 */
#define ORP_SENSOR_CONFIG_DEFAULT() {                    \
    .adc_unit = ADC_UNIT_1,                             \
    .adc_channel = ADC_CHANNEL_3,                       \
    .adc_atten = ADC_ATTEN_DB_12,                       \
    .min_value_mv = 100,                                \
    .max_value_mv = 1000,                               \
}

/**
 * @brief init function for ORP sensor and callback setup
 *
 * @param config                pointer of ORP sensor config.
 * @param update_interval       sensor value update interval in seconds.
 * @param cb                    callback pointer.
 *
 * @return ESP_OK if the driver initialization succeed, otherwise ESP_FAIL.
 */
esp_err_t orp_sensor_driver_init(orp_sensor_config_t *config, uint16_t update_interval, esp_orp_sensor_callback_t cb);

/**
 * @brief Set calibration offset for ORP sensor
 *
 * @param offset_mv             calibration offset in millivolts
 *
 * @return ESP_OK if calibration set successfully, otherwise ESP_FAIL.
 */
esp_err_t orp_sensor_set_calibration(int offset_mv);

/**
 * @brief Get current calibration offset
 *
 * @param offset_mv             pointer to store current offset in millivolts
 *
 * @return ESP_OK if calibration retrieved successfully, otherwise ESP_FAIL.
 */
esp_err_t orp_sensor_get_calibration(int *offset_mv);

/**
 * @brief Get current ORP reading
 *
 * @param orp_mv                pointer to store current ORP value in millivolts
 *
 * @return ESP_OK if reading successful, otherwise ESP_FAIL.
 */
esp_err_t orp_sensor_get_reading(int *orp_mv);

#ifdef __cplusplus
} // extern "C"
#endif
