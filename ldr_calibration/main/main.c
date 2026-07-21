/*
 *  LDR — Interactive Calibration Mode (เหมือน Week-02) + LED Control
 *
 *  วงจร: 3.3V ──┬── LDR ──┬── R_fixed (10kΩ) ─── GND
 *                    │
 *                V_out → GPIO34 (ADC1_CHANNEL_6)
 *
 *  LED:  GPIO33 (แสดงผลสถานะเปิดอัตโนมัติเมื่อมืด)
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#define ADC_CHANNEL     ADC_CHANNEL_6    // GPIO34
#define ADC_UNIT        ADC_UNIT_1
#define ADC_ATTEN       ADC_ATTEN_DB_12  // รองรับ ~3.3V
#define ADC_WIDTH       ADC_BITWIDTH_12  // 0–4095

#define LED_GPIO        GPIO_NUM_33      // ขา LED

// ============================================================
//  ตั้งค่าระบบคาลิเบรตแบบ Interactive ( 5 จุดสภาพแสง )
// ============================================================
#define NUM_POINTS   5
#define CAL_SAMPLES  64   // จำนวนครั้งที่อ่านเฉลี่ยตอนกดบันทึกคาลิเบรต
#define READ_SAMPLES 32   // จำนวนครั้งที่อ่านเฉลี่ยตอนรันจริง

static adc_oneshot_unit_handle_t adc_handle;

// ตารางคาลิเบรต (กำหนดค่าเริ่มต้นตามตาราง)
static int cal_adc[NUM_POINTS] = {533, 711, 1648, 3435, 4095};
static const float cal_lux[NUM_POINTS] = {273.86, 348.62, 522.16, 1292.7, 1869.9};
static const char *cal_name[NUM_POINTS] = {
    "มืดมาก",
    "มืด",
    "ห้องปกติ",
    "ใต้โคมไฟ",
    "กลางแจ้ง"
};
static const char *cal_desc[NUM_POINTS] = {
    "มืดมาก (~273.86 Lux)",
    "มืด (~348.62 Lux)",
    "ห้องปกติ (~522.16 Lux)",
    "ใต้โคมไฟ (~1292.7 Lux)",
    "กลางแจ้ง / สว่างมาก (~1869.9 Lux)"
};

// ============================================================
//  อ่านค่า ADC แบบเฉลี่ย (Moving Average Filter)
// ============================================================
int read_adc_averaged(int samples)
{
    int sum = 0;
    for (int i = 0; i < samples; i++) {
        int raw = 0;
        adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw);
        sum += raw;
    }
    return sum / samples;
}

// ============================================================
//  แปลง ADC → Lux ด้วย Piecewise Linear Interpolation (เทียบบัญญัติไตรยางศ์ทีละช่วง)
// ============================================================
float adc_to_lux(int adc)
{
    if (adc <= cal_adc[0])              return cal_lux[0];
    if (adc >= cal_adc[NUM_POINTS - 1]) return cal_lux[NUM_POINTS - 1];

    for (int i = 0; i < NUM_POINTS - 1; i++) {
        if (adc >= cal_adc[i] && adc <= cal_adc[i + 1]) {
            float ratio = (float)(adc - cal_adc[i]) / (float)(cal_adc[i + 1] - cal_adc[i]);
            return cal_lux[i] + ratio * (cal_lux[i + 1] - cal_lux[i]);
        }
    }
    return 0.0f;
}

// ============================================================
//  ฟังก์ชันประเมินสภาพแสงจากค่า Lux
// ============================================================
const char* get_light_condition(float lux)
{
    if (lux <= 311.24f)  return "มืดมาก";
    if (lux <= 435.39f)  return "มืด";
    if (lux <= 907.43f)  return "ห้องปกติ";
    if (lux <= 1581.3f)  return "ใต้โคมไฟ";
    return "กลางแจ้ง/สว่างมาก";
}

// ============================================================
//  โหมดคาลิเบรตแบบ Interactive (Live ADC + กด Enter บันทึก)
// ============================================================
void run_calibration(void)
{
    printf("\n");
    printf("========================================================\n");
    printf("   LDR CALIBRATION MODE  (%d จุดวัด)\n", NUM_POINTS);
    printf("========================================================\n");
    printf("  (ข้ามการรับค่า Interactive เพื่อใช้ค่าคงที่ตามตาราง)\n");
    printf("========================================================\n\n");

    // แสดงตารางสรุปผลคาลิเบรต (พร้อมคอลัมน์ สภาพแสง | ค่า ADC | ค่า Lux)
    printf("=================================================================\n");
    printf("   ตารางสรุปค่า Calibration LDR ของคุณ\n");
    printf("=================================================================\n");
    printf("   สภาพแสง          |  ค่า ADC ที่บันทึกได้ |  ค่า Lux (เป้าหมาย)\n");
    printf("  ------------------+-----------------------+--------------------\n");
    for (int i = 0; i < NUM_POINTS; i++) {
        printf("   %-16s |         %4d          |     %6.2f Lux\n", cal_name[i], cal_adc[i], cal_lux[i]);
    }
    printf("=================================================================\n");
    printf("  คาลิเบรตเสร็จสิ้น! เริ่มทำงานโหมดวัดแสงจริง + ควบคุม LED...\n");
    printf("  (หากต้องการคาลิเบรตใหม่ ให้กดปุ่ม RST บนบอร์ด ESP32)\n");
    printf("=================================================================\n\n");
}

// ============================================================
//  app_main
// ============================================================
void app_main(void)
{
    /* ตั้งค่าขา LED GPIO33 เป็น Output */
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    /* 1) สร้าง ADC handle */
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT,
    };
    adc_oneshot_new_unit(&init_cfg, &adc_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN,
        .bitwidth = ADC_WIDTH,
    };
    adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &chan_cfg);

    /* 2) รันโหมดคาลิเบรต Interactive ตอนเริ่มบูต */
    run_calibration();

    /* 3) วนลูปอ่านค่าจริง แปลงมุม และควบคุม LED */
    while (1) {
        int raw = read_adc_averaged(READ_SAMPLES);
        int voltage_mv = (raw * 3300) / 4095;
        float lux = adc_to_lux(raw);
        const char *condition = get_light_condition(lux);

        printf("ADC Raw: %4d | Voltage: %4d mV | Lux: %6.2f | สภาพแสง: %-18s | LED: %s\n",
               raw, voltage_mv, lux, condition, (raw < cal_adc[1]) ? "ON 💡" : "OFF");

        // ควบคุม LED: ถ้ามืด (ค่า ADC ต่ำกว่าจุด "มืด 20 Lux") ให้เปิด LED
        if (raw < cal_adc[1]) {
            gpio_set_level(LED_GPIO, 1);
        } else {
            gpio_set_level(LED_GPIO, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
