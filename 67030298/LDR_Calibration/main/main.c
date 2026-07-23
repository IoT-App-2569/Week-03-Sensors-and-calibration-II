#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC_CHANNEL     ADC_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_12 // รองรับ ~3.3V (ใน IDF รุ่นใหม่จะเป็น 12dB แทน 11dB)

void app_main(void)
{
    // 1) ตั้งค่า ADC (One-shot mode)
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // 2) ทำ Calibration
    adc_cali_handle_t adc_cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_line_fitting(&cali_config, &adc_cali_handle));

    while (1) {
        // 3) อ่านค่า ADC raw
        int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw));
        
        // 4) แปลงเป็นแรงดัน (mV)
        int voltage;
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw, &voltage));
        
        // คำนวณค่า Lux ตามสมการจาก Excel
        float lux = 0.00001 * raw * raw - 0.0299 * raw + 14.63;
        
        printf("ADC Raw = %d, Voltage = %d mV, Lux = %.2f\n", raw, voltage, lux);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
