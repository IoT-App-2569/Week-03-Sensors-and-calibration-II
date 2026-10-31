#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับช่วงแรงดัน ~0 - 3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // ความละเอียด 12 บิต (0 - 4095)
#define DEFAULT_VREF    1100             // ค่าแรงดันอ้างอิงภายใน (mV)

static esp_adc_cal_characteristics_t adc_chars;

void app_main(void)
{
    // 1) กำหนดค่าคอนฟิก ADC1
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // 2) สอบเทียบ ADC Vref
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        DEFAULT_VREF,
        &adc_chars
    );

    printf("\n===================================================\n");
    printf(" 🌟 Lab 3.3: มิเตอร์วัดความสว่างเรียลไทม์ (Calibrated Lux)\n");
    printf(" คำนวณค่า Lux ด้วยสมการ Calibration พร้อมตัวกรอง\n");
    printf("===================================================\n\n");

    while (1) {
        int raw = adc1_get_raw(ADC_CHANNEL);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

        // 3) คำนวณค่าความสว่าง Lux ด้วยสมการสอบเทียบ Linear:
        //    lux = 0.0984 * raw - 82.621
        float raw_f = (float)raw;
        float lux = 0.0984f * raw_f - 82.621f;
        if (lux < 0.0f) {
            lux = 0.0f; // ป้องกันค่าความสว่างติดลบในสภาวะมืดสนิท
        }

        printf("[Lab 3.3] ADC Raw = %4d | Voltage = %4u mV | Calibrated Lux = %6.2f Lux\n", 
               raw, (unsigned int)voltage, lux);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
