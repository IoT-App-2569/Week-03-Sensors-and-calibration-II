# LDR Calibration — ESP-IDF Project

โปรเจ็คนี้ใช้สำหรับอ่านค่า LDR (Light Dependent Resistor) ผ่าน ADC ของ ESP32
และแปลงค่า ADC → Lux ด้วยสมการ Calibration

## ฮาร์ดแวร์ที่ต้องใช้

- บอร์ด ESP32 (เช่น ESP32-DevKitC)
- LDR (Photoresistor / ตัวรับแสง)
- ตัวต้านทานคงที่ 10 kΩ
- เบรดบอร์ด + สาย Jumper

## วงจร Voltage Divider

```
  3.3V (ESP32) ──┬── LDR ──┬── R_fixed (10kΩ) ─── GND
                 │
             V_out
                 │
           ADC1_CH6 (GPIO34)
```

- **LDR** ต่อระหว่าง 3.3V และจุด Junction
- **R_fixed (10kΩ)** ต่อระหว่าง Junction และ GND
- **ADC** อ่านที่จุด Junction (GPIO34 / ADC1_CH6)

## การติดตั้ง ESP-IDF

ถ้ายังไม่ได้ติดตั้ง ESP-IDF บนเครื่อง ให้ทำตามขั้นตอนนี้:

```bash
# ดาวน์โหลด ESP-IDF (ประมาณ 1-2 GB)
# สำหรับ macOS:
bash -c "$(curl -fsSL https://dl.espressif.com/github_com/espressif/esp-idf/espidf-4.4.7.release installer-trusted-fields-v4.4.7.sh)"

# หรือใช้ pip
pip install esptool esp-idf

# หรือใช้ VS Code + ESP-IDF Extension (แนะนำ)
# 1. ติดตั้ง ESP-IDF Extension ใน VS Code
# 2. กด F1 → "ESP-IDF: Configure ESP-IDF Extension"
# 3. เลือก "Express" เพื่อติดตั้งอัตโนมัติ
```

## วิธี Build และ Flash

### วิธีที่ 1: ผ่าน VS Code + ESP-IDF Extension (แนะนำ)

1. เปิด VS Code
2. ติดตั้ง **ESP-IDF Extension** (publisher: Espressif Systems)
3. เปิดโฟลเดอร์นี้ (`ldr_calibration/`) ใน VS Code
4. กด `F1` → พิมพ์ `ESP-IDF: Configure Project`
5. กด `F1` → พิมพ์ `ESP-IDF: Build, Flash and Monitor`

### วิธีที่ 2: ผ่าน Command Line

```bash
# ไปที่โฟลเดอร์โปรเจ็ค
cd ldr_calibration

# ตั้งค่า ESP-IDF environment
# (เปลี่ยน path ตามที่ติดตั้งจริง)
. $HOME/esp/esp-idf/export.sh

# Build
idf.py build

# Flash ไปที่บอร์ด
idf.py -p /dev/ttyUSB0 flash monitor

# (เปลี่ยน /dev/ttyUSB0 เป็น port ที่บอร์ดต่ออยู่)
```

## การใช้งานโปรแกรม

1. เปิด Serial Monitor (baudrate 115200)
2. โปรแกรมจะพิมพ์ค่าทุก 500 ms:

```
I (1234) LDR_CALIBRATION: ========================================
I (1234) LDR_CALIBRATION:   LDR Calibration — ESP-IDF Starter
I (1234) LDR_CALIBRATION: ========================================
I (1234) LDR_CALIBRATION: ADC Channel : ADC1_CH6 (GPIO34)
I (1234) LDR_CALIBRATION: Voltage Divider: R_fixed = 10000 Ohm, R_LDR ต่อไปที่ 3.3V
I (1234) LDR_CALIBRATION: Calibration: Lux = 0.4200 × ADC_raw + (-50.00)
I (1234) LDR_CALIBRATION: ----------------------------------------
I (1234) LDR_CALIBRATION: RAW=1842 | Voltage=1.481 V | R_LDR= 12289 Ohm | Lux=723.6
```

## การ Calibration — ขั้นตอนถัดไป

### ขั้นตอนที่ 1: เก็บข้อมูล

1. เปลี่ยนสภาพแสงรอบ LDR (มืด / ห้องปกติ / ใต้โคมไฟ / แสงแดด)
2. จดค่า **ADC raw** จาก Serial Monitor
3. ใช้แอป Lux Meter ในมือถือวัดค่า Lux จริง
4. เก็บข้อมูลอย่างน้อย **5–7 จุด**

ตัวอย่างตาราง:

| สภาพแสง    | ADC Raw | Lux (มือถือ) |
|------------|--------|--------------|
| มืดมาก      | 300    | 5            |
| มืด         | 450    | 20           |
| ห้องปกติ    | 1200   | 150          |
| ใต้โคมไฟ    | 2200   | 350          |
| กลางแจ้ง    | 3500   | 1200         |

### ขั้นตอนที่ 2: Plot กราฟใน Excel / Google Sheet

1. สร้าง Scatter Chart (ADC Raw vs Lux)
2. เพิ่ม Trendline → Linear หรือ Polynomial
3. ติ๊ก "แสดงสมการ" และ "แสดงค่า R²"
4. เลือก trendline ที่ R² ใกล้ 1.000 มากที่สุด
5. นำสมการมาแทนค่าใน `main.c`

### ขั้นตอนที่ 3: แก้ไขสมการ Calibration ใน main.c

```c
// บรรทัดที่ 25-26 ของ main.c
#define CALIB_M         0.42f    // ความชัน       ← แก้ตามกราฟจริง
#define CALIB_B        -50.0f    // จุดตัดแกน Y   ← แก้ตามกราฟจริง
```

หรือถ้าใช้ Polynomial:

```c
// สมการ: Lux = a·ADC² + b·ADC + c
float lux = 0.0001f * raw_adc * raw_adc + 0.3f * raw_adc - 40.0f;
```

## สมการคำนวณแรงดันจาก Voltage Divider

```
V_out = V_cc × R_fixed / (R_LDR + R_fixed)

ตัวอย่าง: ที่ห้องปกติ R_LDR ≈ 30 kΩ
V_out = 3.3 × 10k / (30k + 10k) = 0.825 V
```

## หมายเหตุ

- **GPIO34** อ่านค่า ADC ได้เฉพาะ **Input** เท่านั้น (ไม่ต้องตั้ง direction)
- ค่า **Vref** เริ่มต้นคือ 1100 mV ถ้า ESP32 ไม่มี eFuse calibration
- ถ้าต้องการลด noise เพิ่มเติม สามารถเฉลี่ยค่า ADC หลายครั้ง (moving average)

## Troubleshooting

| ปัญหา | สาเหตุที่เป็นไปได้ | วิธีแก้ |
|-------|----------------------|---------|
| ไม่เห็น Serial port | ไม่ได้ติดตั้ง USB driver | ติดตั้ง CP210x หรือ CH340 driver |
| ค่า ADC เป็น 0 | ต่อสายผิด / LDR เสีย | ตรวจสอบวงจร + ลองกลับขา� LDR |
| ค่า ADC ไม่เปลี่ยน | ไม่ได้ต่อ R_fixed หรือ LDR ขาด | ตรวจสอบการต่อวงจร |
| ค่า Lux ติดลบ | ค่า ADC ต่ำเกินไป | ปรับ CALIB_B ให้เป็นบวก หรือใช้ if (lux < 0) lux = 0; |
