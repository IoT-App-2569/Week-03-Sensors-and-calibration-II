#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"

#define ADC_CHANNEL     ADC_CHANNEL_6    // GPIO34 (ต่อกับ LDR Voltage Divider)
#define ADC_ATTEN       ADC_ATTEN_DB_12  // รองรับช่วงแรงดัน ~0 - 3.3V

void app_main(void)
{
    adc_oneshot_unit_handle_t adc_handle;
    const adc_oneshot_unit_init_cfg_t unit_config = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    const adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_12,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &channel_config));

    printf("\n===================================================\n");
    printf(" ☀️ Lab 3.1: การอ่านค่าแอนะล็อกดิบจาก LDR (ADC Raw)\n");
    printf(" ต่อ LDR + R 10k เข้าขา GPIO 34 (ADC1 Channel 6)\n");
    printf("===================================================\n\n");

    while (1) {
        int raw;
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw));

        printf("[Lab 3.1] ADC Raw = %d\n", raw);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
