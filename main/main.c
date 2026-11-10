#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"

#define ADC1_LDR_CHANNEL ADC1_CHANNEL_6 // GPI34 (ADC1 Channel 6)

void app_main(void) {
    printf("=== Start LDR ADC Reading Test ===\n");

    // 1. ตั้งค่าความละเอียด 12-bit (ช่วงค่า 0 - 4095)
    adc1_config_width(ADC_WIDTH_BIT_12);

    // 2. ตั้งค่า Attenuation 11dB (ช่วงแรงดัน 0 - 3.3V)
    adc1_config_channel_atten(ADC1_LDR_CHANNEL, ADC_ATTEN_DB_11);

    // 3. วนลูปอ่านค่าและปริ้นท์ออกทาง Serial Monitor ทุก 0.5 วินาที
    while (1) {
        int raw_adc = adc1_get_raw(ADC1_LDR_CHANNEL);
        printf("ADC Raw Value: %d\n", raw_adc);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}