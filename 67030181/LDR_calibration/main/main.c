#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับแรงดันประมาณ 0–3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // ความละเอียด 12 บิต (0–4095)

static esp_adc_cal_characteristics_t adc_chars;

void app_main(void)
{
    // ตั้งค่า ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // Calibration
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        1100,          // ค่า Vref โดยประมาณ
        &adc_chars
    );

    while (1)
    {
        // อ่านค่า ADC
        int raw = adc1_get_raw(ADC_CHANNEL);

        // แปลงเป็นแรงดัน (mV)
        uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

        printf("ADC Raw = %d, Voltage = %lu mV\n",
               raw,
               (unsigned long)voltage);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}