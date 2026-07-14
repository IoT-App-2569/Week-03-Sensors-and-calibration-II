#include <stdio.h>
#include <inttypes.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับ ~3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // 0-4095

static esp_adc_cal_characteristics_t adc_chars;

// TODO: แทนที่ค่า m, b ด้วยสมการ Trendline ที่ได้จากกราฟ Excel/Google Sheet จริง
// ตัวอย่าง: y = 0.42x - 50  ->  m = 0.42, b = -50
#define CAL_M   0.42f
#define CAL_B   (-50.0f)

// ถ้ากราฟไม่เป็นเส้นตรง ให้เปลี่ยนไปใช้ adc_to_lux_poly() แทน
static float adc_to_lux_linear(int raw)
{
    float lux = CAL_M * raw + CAL_B;
    if (lux < 0) lux = 0;   // Lux ติดลบไม่มีจริง
    return lux;
}

// ตัวอย่าง Polynomial (2nd order): y = a*x^2 + b*x + c
// TODO: แทนที่ค่า a, b, c ด้วยสมการจริงถ้าเลือกใช้ Polynomial
#define CAL_A   0.0001f
#define CAL_PB  0.3f
#define CAL_C   (-40.0f)

static float __attribute__((unused)) adc_to_lux_poly(int raw)
{
    float lux = CAL_A * raw * raw + CAL_PB * raw + CAL_C;
    if (lux < 0) lux = 0;
    return lux;
}

void app_main(void)
{
    // 1) ตั้งค่า ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // 2) ทำ Calibration ของ ADC เอง (ใช้ค่า Vref = 1100mV โดยประมาณ)
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        1100,
        &adc_chars
    );

    while (1) {
        // 3) อ่านค่า ADC raw
        int raw = adc1_get_raw(ADC_CHANNEL);

        // 4) แปลงเป็นแรงดัน (mV) ผ่านการ calibrate ของ ESP32 เอง
        uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(raw, &adc_chars);

        // 5) แปลง ADC raw -> Lux ด้วยสมการ calibration ที่หาได้จากกราฟ
        float lux = adc_to_lux_linear(raw);
        // float lux = adc_to_lux_poly(raw); // ใช้บรรทัดนี้แทนถ้าเลือก Polynomial

        printf("ADC Raw = %d, Voltage = %" PRIu32 " mV, Lux = %.2f\n", raw, voltage_mv, lux);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}