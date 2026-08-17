#include <Arduino.h>

#define LDR_PIN 39   // KMITL FIGHT board: PIR/LDR/IR Rx selector -> เลือก LDR ที่ DIP switch, GPIO อาจต้องเช็คจริง

// --- ค่า calibration: ต้องเก็บข้อมูล ADC vs Lux จริงแล้วหา trendline (Excel/Sheet) ก่อนแก้ตรงนี้ ---
// ตัวอย่าง linear:  Lux = m * ADC + b
#define CAL_M   0.42
#define CAL_B  -50.0

float adc_to_lux(int adc) {
  float lux = CAL_M * adc + CAL_B;
  if (lux < 0) lux = 0;
  return lux;
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);        // 0-4095
  analogSetAttenuation(ADC_11db);  // รองรับ ~0-3.3V
}

void loop() {
  int raw = analogRead(LDR_PIN);
  float lux = adc_to_lux(raw);

  Serial.printf("ADC = %d  Lux = %.1f\n", raw, lux);
  delay(500);
}
