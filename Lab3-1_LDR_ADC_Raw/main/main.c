#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34 (ต่อกับ LDR Voltage Divider)
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับช่วงแรงดัน ~0 - 3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // ความละเอียด 12 บิต (0 - 4095)

void app_main(void)
{
    // 1) กำหนดความกว้างบิตและระดับ Attenuation ของ ADC1
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    printf("\n===================================================\n");
    printf(" ☀️ Lab 3.1: การอ่านค่าแอนะล็อกดิบจาก LDR (ADC Raw)\n");
    printf(" ต่อ LDR + R 10k เข้าขา GPIO 34 (ADC1 Channel 6)\n");
    printf("===================================================\n\n");

    while (1) {
        // อ่านค่า ADC Raw จากช่องสัญญาณ
        int raw = adc1_get_raw(ADC_CHANNEL);

        printf("[Lab 3.1] ADC Raw = %d\n", raw);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
