#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"

// กำหนดขา GPIO 34 เป็น ADC1 Channel 6
#define ADC_CHANNEL ADC1_CHANNEL_6

// ค่าคาลิเบตอ้างอิงจากการทดลอง (สามารถปรับเปลี่ยนค่าตามผลการทดลองจริงได้)
#define ADC_MIN 150   // ค่า Raw ADC ต่ำสุด (เช่น ที่ 0 องศา หรือ มืดสนิท)
#define ADC_MAX 3900  // ค่า Raw ADC สูงสุด (เช่น ที่ 180 องศา หรือ แสงสว่างจ้า)
#define MAX_ANGLE 180.0f // มุมสูงสุด 180 องศา

void app_main(void) {
    // 1. ตั้งค่าความละเอียด ADC เป็น 12-bit (ให้ค่า 0 - 4095)
    adc1_config_width(ADC_WIDTH_BIT_12);

    // 2. ตั้งค่าการลดทอนสัญญาณ Attenuation 11dB (รองรับแรงดัน 0 - 3.1V/3.3V)
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN_DB_11);

    printf("=== เริ่มต้นระบบอ่านค่าเซ็นเซอร์และการคาลิเบต ===\n");

    while (1) {
        // อ่านค่า Raw ADC จากขา GPIO 34 (ช่วงค่า 0 - 4095)
        int raw_adc = adc1_get_raw(ADC_CHANNEL);

        // --- การทดลองที่ 1: คำนวณคาลิเบตเชิงเส้นแบบ 2 จุด สำหรับ Potentiometer (วัดมุม 0 - 180 องศา) ---
        float angle = ((float)(raw_adc - ADC_MIN) / (float)(ADC_MAX - ADC_MIN)) * MAX_ANGLE;

        // จำกัดช่วงมุมไม่ให้ต่ำกว่า 0 หรือเกิน 180 องศา
        if (angle < 0.0f) angle = 0.0f;
        if (angle > MAX_ANGLE) angle = MAX_ANGLE;

        // --- การทดลองที่ 2: คำนวณคาลิเบตแบบ Linear Fit สำหรับ LDR (แปลงเป็น Lux) ---
        // สมการตัวอย่าง: Lux = (0.125 * Raw_ADC) - 15.0
        float calculated_lux = (0.125f * raw_adc) - 15.0f;
        if (calculated_lux < 0.0f) calculated_lux = 0.0f;

        // แสดงผลผ่าน Serial Monitor
        printf("Raw ADC: %4d | Calibrated Angle: %6.2f Deg | Calibrated Light: %6.2f Lux\n",
               raw_adc, angle, calculated_lux);

        // หน่วงเวลา 1 วินาที
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```[cite: 1, 2, 3]
