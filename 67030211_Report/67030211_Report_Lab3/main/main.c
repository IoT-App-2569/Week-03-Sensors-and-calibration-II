#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6
#define ADC_ATTEN       ADC_ATTEN_DB_11
#define ADC_WIDTH       ADC_WIDTH_BIT_12

int adc_to_lux(int adc)
{
    if (adc <= 280) return 0;
    
    // คำนวณจากสมการ: Lux = 0.000112 * ADC^2 - 0.0602 * ADC + 28.92
    float lux = (0.000112f * adc * adc) - (0.0602f * adc) + 28.92f;
    if (lux < 0.0f) lux = 0.0f;
    
    return (int)(lux + 0.5f);
}

void app_main(void)
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    printf("--- ESP32 LDR Lux Meter Ready ---\n");

    while (1) {
        int raw_adc = adc1_get_raw(ADC_CHANNEL);
        int lux = adc_to_lux(raw_adc);

        printf("ADC = %d | Lux = %d lx\n", raw_adc, lux);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
