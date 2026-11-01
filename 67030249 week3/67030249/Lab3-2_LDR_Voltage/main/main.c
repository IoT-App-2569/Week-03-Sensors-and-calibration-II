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

    // 2) สอบเทียบ ADC เพื่อแปลงค่าดิบเป็นแรงดันไฟฟ้าจริง (mV)
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        DEFAULT_VREF,
        &adc_chars
    );

    printf("\n===================================================\n");
    printf(" 📊 Lab 3.2: การแปลงค่า ADC เป็นแรงดันไฟฟ้า (Voltage mV)\n");
    printf(" ใช้บันทึกข้อมูล 5 สภาวะแสงเพื่อนำไปเทียบกับ Lux มือถือ\n");
    printf("===================================================\n\n");

    while (1) {
        int raw = adc1_get_raw(ADC_CHANNEL);
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

        printf("[Lab 3.2] ADC Raw = %4d | Voltage = %4u mV (%.3f V)\n", 
               raw, (unsigned int)voltage, voltage / 1000.0f);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
