#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// กำหนดแชนเนลและระดับการทนแรงดันให้เหมาะสม
#define ADC_CHANNEL     ADC_CHANNEL_6
#define ADC_ATTEN       ADC_ATTEN_DB_12
#define ADC_UNIT        ADC_UNIT_1

static const char *TAG = "LDR_CALIBRATION";

void app_main(void)
{
    // 1) ตั้งค่า ADC Oneshot Unit
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2) ตั้งค่า Channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // 3) ระบบทำ Calibration
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    
    // เรียกใช้ฟังก์ชัน Calibration (ชื่อถูกต้องตาม v5.5.1)
    adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);

    // 4) Loop อ่านค่า LDR และแปลงเป็น Lux ด้วยสมการ Polynomial
    while (1) {
        int raw = 0;
        if (adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw) == ESP_OK) {
            
            // คำนวณค่า Lux ตามสมการจากกราฟ: y = 9E-05*x^2 + 0.0962*x - 0.1693
            // 9E-05 = 0.00009
            float lux = (0.00009f * (float)raw * (float)raw) + (0.0962f * (float)raw) - 0.1693f;
            
            // ป้องกันค่าติดลบ
            if (lux < 0) lux = 0;
            
            ESP_LOGI(TAG, "Raw ADC: %d, Lux: %.2f", raw, lux);
        }
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}