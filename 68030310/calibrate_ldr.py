"""
LDR Calibration and Regression Script for Lab Sheet 03
Student ID: 68030310
"""

import math
import os

# 1. Measured Data Points (Ground Truth Phone Lux vs ESP32 ADC Raw)
calibration_data = [
    {"condition": "กล่องทึบแสงปิดสนิท (Dark Box)", "adc": 420, "lux": 12.0},
    {"condition": "มุมมืดใต้โต๊ะทำงาน (Shaded under desk)", "adc": 850, "lux": 42.0},
    {"condition": "ห้องสลัวช่วงเช้า (Dim room / dawn)", "adc": 1380, "lux": 118.0},
    {"condition": "ห้องเรียนเปิดไฟนีออนปกติ (Normal classroom)", "adc": 1980, "lux": 255.0},
    {"condition": "โต๊ะอ่านหนังสือใต้โคมไฟ (Study desk under lamp)", "adc": 2580, "lux": 485.0},
    {"condition": "โคมไฟตั้งโต๊ะระยะใกล้ 15cm (Bright desk lamp)", "adc": 3180, "lux": 840.0},
    {"condition": "ริมหน้าต่างรับแสงธรรมชาติ (Window daylight)", "adc": 3680, "lux": 1310.0}
]

# 2. Validation Test Points (Unseen Conditions)
validation_data = [
    {"condition": "ใต้เก้าอี้เรียน (Under chair)", "adc": 620, "lux_actual": 25.0},
    {"condition": "โต๊ะอาจารย์หน้าห้อง (Teacher desk)", "adc": 1680, "lux_actual": 180.0},
    {"condition": "ใกล้โคมไฟระยะ 20cm (Close study lamp)", "adc": 2900, "lux_actual": 650.0}
]

def calculate_regressions(data):
    x = [d["adc"] for d in data]
    y = [d["lux"] for d in data]
    n = len(x)

    # Linear Regression: y = m*x + c
    sum_x = sum(x)
    sum_y = sum(y)
    sum_xx = sum(xi**2 for xi in x)
    sum_xy = sum(xi*yi for xi, yi in zip(x, y))

    m = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x**2)
    c = (sum_y - m * sum_x) / n

    y_pred_lin = [m * xi + c for xi in x]
    ss_tot = sum((yi - (sum_y/n))**2 for yi in y)
    ss_res_lin = sum((yi - y_hat)**2 for yi, y_hat in zip(y, y_pred_lin))
    r2_lin = 1 - (ss_res_lin / ss_tot)

    # 2nd-Degree Polynomial Regression: y = a*x^2 + b*x + c
    sum_x3 = sum(xi**3 for xi in x)
    sum_x4 = sum(xi**4 for xi in x)
    sum_x2y = sum((xi**2)*yi for xi, yi in zip(x, y))

    # Cramer's Rule for 3x3 System
    m_mat = [
        [sum_x4, sum_x3, sum_xx],
        [sum_x3, sum_xx, sum_x],
        [sum_xx, sum_x, float(n)]
    ]
    v_vec = [sum_x2y, sum_xy, sum_y]

    def det3(A):
        return (A[0][0]*(A[1][1]*A[2][2] - A[1][2]*A[2][1])
              - A[0][1]*(A[1][0]*A[2][2] - A[1][2]*A[2][0])
              + A[0][2]*(A[1][0]*A[2][1] - A[1][1]*A[2][0]))

    d = det3(m_mat)
    A0 = [[v_vec[r] if col == 0 else m_mat[r][col] for col in range(3)] for r in range(3)]
    A1 = [[v_vec[r] if col == 1 else m_mat[r][col] for col in range(3)] for r in range(3)]
    A2 = [[v_vec[r] if col == 2 else m_mat[r][col] for col in range(3)] for r in range(3)]

    poly_a = det3(A0) / d
    poly_b = det3(A1) / d
    poly_c = det3(A2) / d

    y_pred_poly = [poly_a * (xi**2) + poly_b * xi + poly_c for xi in x]
    ss_res_poly = sum((yi - y_hat)**2 for yi, y_hat in zip(y, y_pred_poly))
    r2_poly = 1 - (ss_res_poly / ss_tot)

    return (m, c, r2_lin), (poly_a, poly_b, poly_c, r2_poly)

if __name__ == "__main__":
    (m, c, r2_lin), (pa, pb, pc, r2_poly) = calculate_regressions(calibration_data)
    print("=" * 60)
    print("  รายงานการวิเคราะห์โมเดลเทียบมาตรฐาน (LDR Calibration)")
    print("  รหัสนักศึกษา: 68030310")
    print("=" * 60)
    print(f"1. Linear Fit Equation:")
    print(f"   Lux = {m:.5f} * ADC + ({c:.4f})")
    print(f"   Coefficient of Determination (R^2) = {r2_lin:.5f}")
    print("-" * 60)
    print(f"2. Polynomial Fit (Degree 2) Equation:")
    print(f"   Lux = ({pa:.7e}) * ADC^2 + ({pb:.5f}) * ADC + ({pc:.4f})")
    print(f"   Coefficient of Determination (R^2) = {r2_poly:.5f}")
    print("=" * 60)
    print("\nการทดสอบความแม่นยำบนจุดทดสอบใหม่ (Validation):")
    for v in validation_data:
        raw = v["adc"]
        act = v["lux_actual"]
        p_lin = m * raw + c
        p_pol = pa * (raw**2) + pb * raw + pc
        err_lin = abs(p_lin - act) / act * 100
        err_pol = abs(p_pol - act) / act * 100
        print(f"สภาพแสง: {v['condition']}")
        print(f"  ADC Raw: {raw} | ค่าจริง (Phone): {act:.1f} Lux")
        print(f"  - Linear:     {p_lin:7.2f} Lux | คลาดเคลื่อน: {err_lin:5.1f}%")
        print(f"  - Polynomial: {p_pol:7.2f} Lux | คลาดเคลื่อน: {err_pol:5.1f}%\n")

