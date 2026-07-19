#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

// ขา GPIO สำหรับบอร์ด ESP32 เก่า (สามารถเปลี่ยนได้ตามขาที่พี่ต่อจริง เช่น ADC1_CHANNEL_6 คือ GPIO34)
#define ADC_CHANNEL     ADC_CHANNEL_6    
#define ADC_ATTEN       ADC_ATTEN_DB_12  

void app_main(void)
{
    // 1) ตั้งค่าระบบ ADC1 สำหรับ ESP32 รุ่นแรก
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2) ตั้งค่า Channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // 3) สำหรับ ESP32 รุ่นเก่า ต้องใช้ Line Fitting เท่านั้น (ตัวเก่าไม่มีโครงสร้าง Curve Fitting)
    adc_cali_handle_t cali_handle = NULL;
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t cali_ret = adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle);

    while (1) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw));
        
        int voltage = 0;
        if (cali_ret == ESP_OK) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &voltage));
        } else {
            voltage = (raw * 3300) / 4095;
        }

        printf("ADC Raw = %d, Voltage = %d mV\n", raw, voltage);
        
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}