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

#include "orp_sensor_driver.h"

#include "esp_err.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "nvs.h"

/**
 * @brief:
 * This example code shows how to configure ORP sensor with ADC.
 *
 * @note:
 * The callback will be called with updated ORP sensor value every $interval seconds.
 *
 */

/* ADC handle */
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t adc_cali_handle = NULL;
static adc_channel_t adc_channel;

/* ORP sensor configuration */
static orp_sensor_config_t sensor_config;

/* call back function pointer */
static esp_orp_sensor_callback_t func_ptr;

/* update interval in seconds */
static uint16_t interval = 1;

/* calibration offset in mV */
static int calibration_offset_mv = 0;

static const char *TAG = "ESP_ORP_SENSOR_DRIVER";
static const char *NVS_NAMESPACE = "orp_sensor";
static const char *NVS_CALIBRATION_KEY = "cal_offset";

/**
 * @brief Initialize ADC calibration
 */
static bool orp_sensor_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

/**
 * @brief Load calibration offset from NVS
 */
static esp_err_t orp_sensor_load_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No calibration data found, using default offset: 0 mV");
        calibration_offset_mv = 0;
        return ESP_OK;
    }

    size_t required_size = sizeof(calibration_offset_mv);
    err = nvs_get_blob(nvs_handle, NVS_CALIBRATION_KEY, &calibration_offset_mv, &required_size);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No calibration data found, using default offset: 0 mV");
        calibration_offset_mv = 0;
    } else {
        ESP_LOGI(TAG, "Loaded calibration offset: %d mV", calibration_offset_mv);
    }

    nvs_close(nvs_handle);
    return ESP_OK;
}

/**
 * @brief Save calibration offset to NVS
 */
static esp_err_t orp_sensor_save_calibration(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle for writing");
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_CALIBRATION_KEY, &calibration_offset_mv, sizeof(calibration_offset_mv));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save calibration to NVS");
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes");
    } else {
        ESP_LOGI(TAG, "Calibration offset saved: %d mV", calibration_offset_mv);
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Read ORP value from ADC
 */
static esp_err_t orp_sensor_read_raw(int *orp_mv)
{
    int adc_raw[10];
    int voltage_sum = 0;
    
    // Take multiple readings for averaging
    for (int i = 0; i < 10; i++) {
        ESP_RETURN_ON_ERROR(adc_oneshot_read(adc_handle, adc_channel, &adc_raw[i]), TAG, "ADC read failed");
        
        if (adc_cali_handle) {
            int voltage;
            ESP_RETURN_ON_ERROR(adc_cali_raw_to_voltage(adc_cali_handle, adc_raw[i], &voltage), TAG, "ADC calibration failed");
            voltage_sum += voltage;
        } else {
            // Fallback calculation without calibration
            voltage_sum += (adc_raw[i] * 3300) / 4095;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10)); // Small delay between readings
    }
    
    // Average the readings and apply calibration offset
    int avg_voltage = voltage_sum / 10;
    *orp_mv = avg_voltage + calibration_offset_mv;
    
    // Clamp to configured range
    if (*orp_mv < sensor_config.min_value_mv) {
        *orp_mv = sensor_config.min_value_mv;
    } else if (*orp_mv > sensor_config.max_value_mv) {
        *orp_mv = sensor_config.max_value_mv;
    }
    
    return ESP_OK;
}

/**
 * @brief Tasks for updating the sensor value
 *
 * @param arg      Unused value.
 */
static void orp_sensor_driver_value_update(void *arg)
{
    for (;;) {
        int orp_value;
        if (orp_sensor_read_raw(&orp_value) == ESP_OK) {
            if (func_ptr) {
                func_ptr(orp_value);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read ORP sensor");
        }
        vTaskDelay(pdMS_TO_TICKS(interval * 1000));
    }
}

/**
 * @brief init ORP sensor
 *
 * @param config      pointer of ORP sensor config.
 */
static esp_err_t orp_sensor_driver_sensor_init(orp_sensor_config_t *config)
{
    // Store configuration
    sensor_config = *config;
    adc_channel = config->adc_channel;
    
    // Configure ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = config->adc_unit,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_new_unit(&init_config, &adc_handle), TAG, "Failed to initialize ADC unit");

    adc_oneshot_chan_cfg_t chan_config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = config->adc_atten,
    };
    ESP_RETURN_ON_ERROR(adc_oneshot_config_channel(adc_handle, config->adc_channel, &chan_config), TAG, "Failed to configure ADC channel");

    // Initialize ADC calibration
    bool cali_enable = orp_sensor_adc_calibration_init(config->adc_unit, config->adc_channel, config->adc_atten, &adc_cali_handle);
    if (!cali_enable) {
        ESP_LOGW(TAG, "ADC calibration not available, using raw values");
    }

    // Load calibration offset from NVS
    ESP_RETURN_ON_ERROR(orp_sensor_load_calibration(), TAG, "Failed to load calibration");

    ESP_LOGI(TAG, "ORP sensor initialized - Range: %d-%d mV, Calibration offset: %d mV", 
             config->min_value_mv, config->max_value_mv, calibration_offset_mv);

    return (xTaskCreate(orp_sensor_driver_value_update, "orp_sensor_update", 4096, NULL, 10, NULL) == pdTRUE) ? ESP_OK : ESP_FAIL;
}

esp_err_t orp_sensor_driver_init(orp_sensor_config_t *config, uint16_t update_interval, esp_orp_sensor_callback_t cb)
{
    if (ESP_OK != orp_sensor_driver_sensor_init(config)) {
        return ESP_FAIL;
    }
    func_ptr = cb;
    interval = update_interval;
    return ESP_OK;
}

esp_err_t orp_sensor_set_calibration(int offset_mv)
{
    // Validate calibration range (±500mV)
    if (offset_mv < -500 || offset_mv > 500) {
        ESP_LOGE(TAG, "Calibration offset out of range: %d mV (allowed: ±500mV)", offset_mv);
        return ESP_ERR_INVALID_ARG;
    }
    
    calibration_offset_mv = offset_mv;
    return orp_sensor_save_calibration();
}

esp_err_t orp_sensor_get_calibration(int *offset_mv)
{
    if (offset_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *offset_mv = calibration_offset_mv;
    return ESP_OK;
}

esp_err_t orp_sensor_get_reading(int *orp_mv)
{
    if (orp_mv == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return orp_sensor_read_raw(orp_mv);
}
