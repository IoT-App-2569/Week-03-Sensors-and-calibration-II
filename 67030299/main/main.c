#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับ ~3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // 0–4095

static esp_adc_cal_characteristics_t adc_chars;

void app_main(void)
{
    // 1) ตั้งค่า ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // 2) ทำ Calibration (ใช้ค่า Vref = 1100mV)
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        1100,              // ค่า Vref ของ ESP32 โดยประมาณ
        &adc_chars
    );

    while (1) {
        // 3) อ่านค่า ADC raw
        int raw = adc1_get_raw(ADC_CHANNEL);

        // 4) แปลงเป็นแรงดัน (mV)
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

        // 5) คำนวณค่า Lux จากสมการ Linear Calibration: float lux = 0.0984 * raw - 82.621;
        float raw_f = (float)raw;
        float lux = 0.0984f * raw_f - 82.621f;
        if (lux < 0.0f) {
            lux = 0.0f; // ป้องกันไม่ให้ค่า Lux ติดลบในสภาพแสงมืดจัด
        }

        printf("ADC Raw = %d, Voltage = %d mV, Calibrated Lux = %.2f\n", raw, (int)voltage, lux);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
