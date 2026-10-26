#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11
#define ADC_WIDTH       ADC_WIDTH_BIT_12

int adc_to_lux(int raw)
{
    // Polynomial Curve Fit: Lux = a * x^2 + b * x + c
    double x = (double)raw;
    double lux = (0.000085 * x * x) + (0.12 * x) + 5.0;
    if (lux < 0) lux = 0;
    return (int)(lux + 0.5);
}

void app_main(void)
{
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    while (1) {
        int raw = adc1_get_raw(ADC_CHANNEL);
        int lux = adc_to_lux(raw);
        printf("ADC Raw: %d | Light: %d Lux\n", raw, lux);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
