/*
 * LAB 5 (Week-03) — LDR + Calibration (ADC -> Lux)
 * ESP-IDF v6.x  (ไดรเวอร์ ADC แบบเก่า driver/adc.h + esp_adc_cal ถูกถอดออกแล้ว
 *               จึงใช้ esp_adc/adc_oneshot.h + adc_cali แทน — หลักการเหมือนโค้ดในใบงาน)
 *
 * วงจร:  3.3V ── LDR ──┬── GPIO34 (ADC1_CH6)
 *                       └── R 10kΩ ── GND
 *        แสงมาก -> R_LDR ต่ำ -> Vout สูง -> ADC สูง
 */
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"

#define ADC_UNIT        ADC_UNIT_1
#define ADC_CHANNEL     ADC_CHANNEL_6      // GPIO34 บน ESP32
#define ADC_ATTEN       ADC_ATTEN_DB_12    // รองรับ ~0–3.3V (เดิมชื่อ DB_11)
#define ADC_BITWIDTH    ADC_BITWIDTH_12    // 0–4095
#define ADC_MAX         4095.0f
#define R_FIXED         10000.0f           // Ω
#define N_AVG           10                 // อ่านแล้วเฉลี่ย 10 ครั้ง ลด noise

/* ---------- สมการ Calibration (ได้จาก Trendline ใน LDR_Calibration.xlsx) ---------- */
// (1) Polynomial อันดับ 3 : y = 1.1190E-07x³ - 4.3249E-04x² + 0.5060x - 106.25   (R² = 0.9881)
#define P3_A   1.1190482e-07f
#define P3_B  -4.3248733e-04f
#define P3_C   5.0596726e-01f
#define P3_D  -1.0624519e+02f
// (2) Exponential : y = 3.9530·e^(0.0016514x)   (R² = 0.9750)
#define EXP_A  3.9529574f
#define EXP_B  1.6513851e-03f
// (3) โมเดล LDR (ส่วน 1.3) : R_LDR = A·Lux^(-α)  ->  Lux = (R_LDR / A)^(-1/α)   (R² = 0.9998)
#define LDR_A      510626.9f
#define LDR_ALPHA  0.8000936f

static const char *TAG = "LDR";

static float lux_poly3(float adc)
{
    float lux = ((P3_A * adc + P3_B) * adc + P3_C) * adc + P3_D;   // Horner form
    return lux < 0 ? 0 : lux;
}

static float lux_exp(float adc)
{
    return EXP_A * expf(EXP_B * adc);
}

static float adc_to_rldr(float adc)
{
    if (adc < 1) adc = 1;                    // กันหารศูนย์ตอนมืดสนิท
    if (adc > ADC_MAX - 1) adc = ADC_MAX - 1;
    return R_FIXED * (ADC_MAX / adc - 1.0f);
}

static float lux_ldr_model(float adc)
{
    return powf(adc_to_rldr(adc) / LDR_A, -1.0f / LDR_ALPHA);
}

static bool adc_cali_init(adc_cali_handle_t *out)
{
    esp_err_t ret = ESP_FAIL;
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED          // ESP32-S3 / C3 / C6 ...
    adc_cali_curve_fitting_config_t cfg = {
        .unit_id = ADC_UNIT, .chan = ADC_CHANNEL, .atten = ADC_ATTEN, .bitwidth = ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cfg, out);
#elif ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED         // ESP32 รุ่นแรก
    adc_cali_line_fitting_config_t cfg = {
        .unit_id = ADC_UNIT, .atten = ADC_ATTEN, .bitwidth = ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_line_fitting(&cfg, out);
#endif
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ADC calibration ใช้ไม่ได้ (eFuse ไม่มีค่า) — จะแสดงเฉพาะค่า raw");
        return false;
    }
    return true;
}

void app_main(void)
{
    // 1) ตั้งค่า ADC oneshot
    adc_oneshot_unit_handle_t adc;
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc));
    adc_oneshot_chan_cfg_t ch_cfg = { .atten = ADC_ATTEN, .bitwidth = ADC_BITWIDTH };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc, ADC_CHANNEL, &ch_cfg));

    // 2) ADC calibration (แปลง raw -> mV)
    adc_cali_handle_t cali = NULL;
    bool has_cali = adc_cali_init(&cali);

    printf("raw,mV,R_LDR(ohm),Lux_poly3,Lux_exp,Lux_ldr\n");   // หัวตาราง CSV คัดลอกไป Excel ได้เลย
    while (1) {
        // 3) อ่านค่า raw หลายครั้งแล้วเฉลี่ย
        int sum = 0, raw = 0;
        for (int i = 0; i < N_AVG; i++) {
            ESP_ERROR_CHECK(adc_oneshot_read(adc, ADC_CHANNEL, &raw));
            sum += raw;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        float adc_avg = (float)sum / N_AVG;

        // 4) แปลงเป็นแรงดัน (mV)
        int mv = 0;
        if (has_cali) {
            adc_cali_raw_to_voltage(cali, (int)(adc_avg + 0.5f), &mv);
        }

        // 5) แปลง ADC -> Lux ด้วยสมการ Calibration
        printf("%.0f,%d,%.0f,%.1f,%.1f,%.1f\n",
               adc_avg, mv, adc_to_rldr(adc_avg),
               lux_poly3(adc_avg), lux_exp(adc_avg), lux_ldr_model(adc_avg));

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
