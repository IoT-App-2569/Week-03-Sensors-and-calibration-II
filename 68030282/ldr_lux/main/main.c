// LAB-5 Week 03: วัดแสงด้วย LDR
// โค้ดตามใบงาน "Week-03-Lab-sheet-03-LDR-and-calibration" ปรับมาใช้ adc_oneshot ของ ESP-IDF v5
#include <stdio.h>
#include <math.h>  // powf() ถ้าใช้ trendline แบบ Power
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_idf_version.h"
#include "esp_log.h"

#define LDR_CHANNEL ADC_CHANNEL_6   // GPIO34 = ADC1_CH6
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
#define LDR_ATTEN   ADC_ATTEN_DB_12 // ชื่อใหม่ของ 11 dB
#else
#define LDR_ATTEN   ADC_ATTEN_DB_11
#endif

// ส่วนที่ 3-4: ตั้ง MEASURE_ONLY = 1 เพื่ออ่านค่า ADC ไปจดคู่กับค่า Lux จากมือถือ
// ส่วนที่ 5-7: ใส่สมการจาก trendline ใน adc_to_lux() แล้วตั้ง MEASURE_ONLY = 0
#define MEASURE_ONLY 0
#define SAMPLES      16     // เฉลี่ยหลายครั้งเพื่อลด noise

static const char *TAG = "ldr_lux";

static float adc_to_lux(int adc)
{
    // สมการจากการสอบเทียบแบบ Polynomial ดีกรี 2 (R² = 0.9886)
    float lux = (0.000164183f * (float)adc * (float)adc) - (0.211775f * (float)adc) + 72.1968f;
    if (lux < 0.0f) {
        lux = 0.0f;
    }
    return lux;
}

void app_main(void)
{
    adc_oneshot_unit_handle_t adc1;
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT_1 };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc1));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = LDR_ATTEN,
        .bitwidth = ADC_BITWIDTH_12, // 0-4095
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1, LDR_CHANNEL, &chan_cfg));

    // ใบงานใช้ esp_adc_cal แปลงเป็น mV — ESP-IDF v5 ใช้ adc_cali แทน
    adc_cali_handle_t cali = NULL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1, .chan = LDR_CHANNEL, .atten = LDR_ATTEN, .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_curve_fitting(&cali_cfg, &cali) != ESP_OK) cali = NULL;
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    adc_cali_line_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_1, .atten = LDR_ATTEN, .bitwidth = ADC_BITWIDTH_12,
    };
    if (adc_cali_create_scheme_line_fitting(&cali_cfg, &cali) != ESP_OK) cali = NULL;
#endif
    if (cali == NULL) {
        ESP_LOGW(TAG, "ADC calibration not available, showing raw values only");
    }

    while (1) {
        int sum = 0, raw = 0;
        for (int i = 0; i < SAMPLES; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc1, LDR_CHANNEL, &raw));
            sum += raw;
        }
        int adc = sum / SAMPLES;

        int mv = 0;
        if (cali) {
            adc_cali_raw_to_voltage(cali, adc, &mv);
        }

#if MEASURE_ONLY
        printf("ADC Raw = %4d, Voltage = %4d mV\n", adc, mv);
#else
        printf("ADC Raw = %4d, Voltage = %4d mV, Lux = %.1f\n", adc, mv, adc_to_lux(adc));
#endif
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
