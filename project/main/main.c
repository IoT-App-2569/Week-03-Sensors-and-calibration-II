#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC_CHANNEL     ADC_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_12 // รองรับ ~3.3V
#define ADC_BITWIDTH    ADC_BITWIDTH_12 // 0–4095

void app_main(void)
{
    // 1) ตั้งค่า ADC Oneshot Unit
    adc_oneshot_unit_handle_t adc1_handle;
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    // 2) ตั้งค่า Channel และ Attenuation
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &config));

    // 3) ตั้งค่า Calibration 
    adc_cali_handle_t cali_handle = NULL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_config, &cali_handle) == ESP_OK) {
        calibrated = true;
    }
#endif

    while (1) {
        int raw = 0;
        int voltage = 0;

        // 4) อ่านค่า ADC raw
        ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL, &raw));
        
        // 5) แปลงเป็นแรงดัน (mV) และคำนวณค่า Lux
        if (calibrated) {
            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &voltage));
            
            // --- ส่วนที่เพิ่มเข้ามา: คำนวณ Lux จากสมการเชิงเส้น ---
            float lux = (0.219f * raw) - 116.0f;
            
            // ป้องกันไม่ให้ค่า Lux ติดลบในกรณีที่มืดมากจนคำนวณแล้วได้ค่าน้อยกว่าศูนย์
            if (lux < 0) {
                lux = 0.0f;
            }
            
            // พิมพ์ผลลัพธ์ทั้ง ADC Raw, Voltage และ Lux ออกมาทาง Serial Monitor
            printf("ADC Raw = %d, Voltage = %d mV, Lux = %.1f\n", raw, voltage, lux);
            // --------------------------------------------------
            
        } else {
            printf("ADC Raw = %d (Calibration not available)\n", raw);
        }
        
        // 6) หน่วงเวลา 500 ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // ทำลายออบเจกต์เมื่อเลิกใช้งานเพื่อคืน Memory
    ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (calibrated) {
        adc_cali_delete_scheme_line_fitting(cali_handle);
    }
#endif
}