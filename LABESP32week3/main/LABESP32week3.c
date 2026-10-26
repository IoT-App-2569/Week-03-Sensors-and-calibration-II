#include <Arduino.h> // ต้องเพิ่มบรรทัดนี้เข้ามา
#include <stdio.h>
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "esp_log.h"

#define ADC_CHANNEL     ADC1_CHANNEL_6   // GPIO34
#define ADC_ATTEN       ADC_ATTEN_DB_11  // รองรับ ~3.3V
#define ADC_WIDTH       ADC_WIDTH_BIT_12 // 0–4095

static esp_adc_cal_characteristics_t adc_chars;

void setup() {
    Serial.begin(115200); // เปิด Serial สำหรับดูค่าใน Arduino

    // 1) ตั้งค่า ADC
    adc1_config_width(ADC_WIDTH);
    adc1_config_channel_atten(ADC_CHANNEL, ADC_ATTEN);

    // 2) ทำ Calibration
    esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH,
        1100,              
        &adc_chars
    );
}

void loop() {
    // 3) อ่านค่า ADC raw
    int raw = adc1_get_raw(ADC_CHANNEL);
    
    // 4) แปลงเป็นแรงดัน (mV)
    uint32_t voltage = esp_adc_cal_raw_to_voltage(raw, &adc_chars);
    
    // เปลี่ยนมาใช้ Serial.printf เพื่อให้แสดงผลบน Serial Monitor ของ Arduino ได้ง่าย
    Serial.printf("ADC Raw = %d, Voltage = %d mV\n", raw, voltage); 
    
    delay(500); // ใช้ delay ของ Arduino แทน vTaskDelay
}