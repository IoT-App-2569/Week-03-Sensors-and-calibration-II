#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
โปรแกรมคำนวณหาสมการเทียบมาตรฐาน (Calibration Models: Linear, Polynomial, Power)
และสร้างแผนภูมิเวกเตอร์ SVG ความละเอียดสูงสำหรับรายงานผลการทดลอง
ผู้พัฒนา: ศุภโชค หอมสมบัติ (68030282)
"""

import csv
import math
import os
import sys

def read_csv(path):
    xs, ys, labels = [], [], []
    with open(path, newline="", encoding="utf-8-sig") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if not row.get("adc") or not row.get("lux"):
                continue
            xs.append(float(row["adc"]))
            ys.append(float(row["lux"]))
            labels.append(row.get("condition", ""))
    return xs, ys, labels

def solve(a, b):
    """Gaussian elimination with partial pivoting"""
    n = len(b)
    m = [row[:] + [b[i]] for i, row in enumerate(a)]
    for c in range(n):
        p = max(range(c, n), key=lambda r: abs(m[r][c]))
        m[c], m[p] = m[p], m[c]
        for r in range(n):
            if r != c:
                k = m[r][c] / m[c][c]
                m[r] = [m[r][j] - k * m[c][j] for j in range(n + 1)]
    return [m[i][n] / m[i][i] for i in range(n)]

def polyfit(xs, ys, deg):
    a = [[sum(x ** (i + j) for x in xs) for j in range(deg + 1)] for i in range(deg + 1)]
    b = [sum(y * x ** i for x, y in zip(xs, ys)) for i in range(deg + 1)]
    return solve(a, b)

def r_squared(ys, preds):
    mean = sum(ys) / len(ys)
    ss_tot = sum((y - mean) ** 2 for y in ys)
    ss_res = sum((y - p) ** 2 for y, p in zip(ys, preds))
    return 1 - ss_res / ss_tot if ss_tot else float("nan")

def generate_svg(xs, ys, labels, models, out_path="images/calibration.svg"):
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    width, height = 800, 520
    pad_left, pad_right, pad_top, pad_bottom = 80, 40, 60, 65
    plot_w = width - pad_left - pad_right
    plot_h = height - pad_top - pad_bottom

    x_min, x_max = 0, 4095
    y_min, y_max = 0, 1600

    def to_svg(x, y):
        sx = pad_left + (x - x_min) / (x_max - x_min) * plot_w
        sy = pad_top + (y_max - y) / (y_max - y_min) * plot_h
        return sx, sy

    svg = []
    svg.append(f"""<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {width} {height}" width="100%" height="100%" style="background:#ffffff; font-family:-apple-system,BlinkMacSystemFont,Segoe UI,Roboto,sans-serif;">""")

    # Title & Subtitle
    svg.append(f"""<text x="{width/2}" y="32" text-anchor="middle" font-size="17" font-weight="bold" fill="#111827">LDR Sensor Calibration Curve (ESP32 ADC 12-bit vs Ground Truth Lux)</text>""")
    svg.append(f"""<text x="{width/2}" y="50" text-anchor="middle" font-size="12" fill="#6b7280">Experimenter: Supachok Homsombat (68030282) | 7 Calibration Levels</text>""")

    # Grid lines & ticks
    for y_val in range(0, 1601, 200):
        _, sy = to_svg(0, y_val)
        svg.append(f"""<line x1="{pad_left}" y1="{sy}" x2="{width-pad_right}" y2="{sy}" stroke="#e5e7eb" stroke-dasharray="4,4" stroke-width="1"/>""")
        svg.append(f"""<text x="{pad_left-12}" y="{sy+4}" text-anchor="end" font-size="11" fill="#4b5563">{y_val}</text>""")

    for x_val in range(0, 4096, 500):
        sx, _ = to_svg(x_val, 0)
        svg.append(f"""<line x1="{sx}" y1="{pad_top}" x2="{sx}" y2="{height-pad_bottom}" stroke="#e5e7eb" stroke-dasharray="4,4" stroke-width="1"/>""")
        svg.append(f"""<text x="{sx}" y="{height-pad_bottom+18}" text-anchor="middle" font-size="11" fill="#4b5563">{x_val}</text>""")

    # Axes
    x0, y0 = to_svg(0, 0)
    x_end, _ = to_svg(4095, 0)
    _, y_end = to_svg(0, 1600)
    svg.append(f"""<line x1="{x0}" y1="{y0}" x2="{x_end}" y2="{y0}" stroke="#1f2937" stroke-width="2"/>""")
    svg.append(f"""<line x1="{x0}" y1="{y0}" x2="{x0}" y2="{y_end}" stroke="#1f2937" stroke-width="2"/>""")

    # Axis Labels
    svg.append(f"""<text x="{pad_left + plot_w/2}" y="{height-20}" text-anchor="middle" font-size="13" font-weight="600" fill="#1f2937">ADC Raw Reading (12-bit: 0 - 4095)</text>""")
    svg.append(f"""<text x="24" y="{pad_top + plot_h/2}" text-anchor="middle" font-size="13" font-weight="600" fill="#1f2937" transform="rotate(-90 24 {pad_top + plot_h/2})">Ground Truth Light Level (Lux)</text>""")

    # Plot Model Lines
    # 1. Linear Fit Line
    f_lin = models[0][3]
    lin_pts = []
    for x_val in range(0, 4096, 40):
        y_val = f_lin(x_val)
        if 0 <= y_val <= 1650:
            sx, sy = to_svg(x_val, y_val)
            lin_pts.append(f"{sx:.1f},{sy:.1f}")
    if lin_pts:
        svg.append(f"""<polyline fill="none" stroke="#dc2626" stroke-width="2" stroke-dasharray="6,4" points="{" ".join(lin_pts)}"/>""")

    # 2. Polynomial Fit Curve
    f_poly = models[1][3]
    poly_pts = []
    for x_val in range(0, 4096, 20):
        y_val = f_poly(x_val)
        if 0 <= y_val <= 1650:
            sx, sy = to_svg(x_val, y_val)
            poly_pts.append(f"{sx:.1f},{sy:.1f}")
    if poly_pts:
        svg.append(f"""<polyline fill="none" stroke="#2563eb" stroke-width="2.5" points="{" ".join(poly_pts)}"/>""")

    # Data Points
    for x, y, lbl in zip(xs, ys, labels):
        sx, sy = to_svg(x, y)
        svg.append(f"""<circle cx="{sx:.1f}" cy="{sy:.1f}" r="5.5" fill="#f59e0b" stroke="#1f2937" stroke-width="1.5"/>""")
        svg.append(f"""<text x="{sx+8:.1f}" y="{sy-6:.1f}" font-size="10.5" font-weight="bold" fill="#111827">({int(x)}, {y:g})</text>""")

    # Legend Card
    r2_lin = r_squared(ys, [models[0][3](x) for x in xs])
    r2_poly = r_squared(ys, [models[1][3](x) for x in xs])
    svg.append(f"""
    <g transform="translate({pad_left+20}, {pad_top+15})">
      <rect width="365" height="90" fill="#ffffff" fill-opacity="0.95" stroke="#cbd5e1" rx="8" />
      <circle cx="20" cy="22" r="5" fill="#f59e0b" stroke="#1f2937" stroke-width="1.5"/>
      <text x="36" y="26" font-size="11.5" font-weight="bold" fill="#1e293b">Measured Data Points (7 Levels)</text>
      
      <line x1="12" y1="48" x2="28" y2="48" stroke="#dc2626" stroke-width="2" stroke-dasharray="6,4"/>
      <text x="36" y="52" font-size="11" font-weight="500" fill="#334155">Linear Fit: Lux = 0.3866·ADC - 219.36 (R² = {r2_lin:.4f})</text>
      
      <line x1="12" y1="73" x2="28" y2="73" stroke="#2563eb" stroke-width="2.5"/>
      <text x="36" y="77" font-size="11" font-weight="bold" fill="#1d4ed8">Poly Deg 2: Lux = 1.48e-4·ADC² - 0.170·ADC + 61.67 (R² = {r2_poly:.4f})</text>
    </g>
    """)

    svg.append("</svg>")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("\n".join(svg))
    print(f"บันทึกกราฟเวกเตอร์สำเร็จ: {out_path}")

def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "data.csv"
    xs, ys, labels = read_csv(path)
    if len(xs) < 3:
        sys.exit(f"ต้องมีข้อมูลอย่างน้อย 3 จุดใน {path} (ตอนนี้มี {len(xs)})")

    models = []

    c0, c1 = polyfit(xs, ys, 1)
    f_lin = lambda x, c0=c0, c1=c1: c1 * x + c0
    models.append(("Linear", f"y = {c1:.6g}x + {c0:.6g}",
                   f"return {c1:.6g}f * adc + ({c0:.6g}f);", f_lin))

    c0, c1, c2 = polyfit(xs, ys, 2)
    f_poly = lambda x, c0=c0, c1=c1, c2=c2: c2 * x * x + c1 * x + c0
    models.append(("Polynomial (ดีกรี 2)", f"y = {c2:.6g}x² + {c1:.6g}x + {c0:.6g}",
                   f"return {c2:.6g}f * adc * adc + ({c1:.6g}f) * adc + ({c0:.6g}f);", f_poly))

    if all(x > 0 for x in xs) and all(y > 0 for y in ys):
        lb, la = polyfit([math.log(x) for x in xs], [math.log(y) for y in ys], 1)[::-1]
        a = math.exp(la)
        f_pow = lambda x, a=a, lb=lb: a * x ** lb
        models.append(("Power", f"y = {a:.6g}x^{lb:.6g}",
                       f"return {a:.6g}f * powf((float)adc, {lb:.6g}f);  // ต้อง #include <math.h>", f_pow))

    print(f"=================================================================")
    print(f"สรุปผลการวิเคราะห์สมการเทียบมาตรฐาน (ข้อมูล {len(xs)} จุดจาก {path})")
    print(f"=================================================================")
    print(f"{'ชนิดของโมเดล':<24}{'R²':>8}   สมการเทียบเคียง")
    print("-" * 65)
    for name, eq, _, f in models:
        r2 = r_squared(ys, [f(x) for x in xs])
        print(f"{name:<24}{r2:>8.4f}   {eq}")

    best = max(models, key=lambda m: r_squared(ys, [m[3](x) for x in xs]))
    print(f"\nโมเดลที่มีค่า R² สูงสุด: {best[0]}")
    print(f"สมการสำหรับ adc_to_lux():\n    {best[2]}")

    print("\nตารางเปรียบเทียบค่าที่วัดได้กับสมการ (" + best[0] + "):")
    print(f"{'สภาวะแสง':<25}{'ADC':>6}{'Lux จริง':>10}{'Lux โมเดล':>12}{'Error (%)':>10}")
    print("-" * 65)
    for lbl, x, y in zip(labels, xs, ys):
        p = best[3](x)
        err = abs(p - y) / y * 100 if y else float("nan")
        print(f"{lbl:<25}{x:>6.0f}{y:>10.1f}{p:>12.1f}{err:>10.1f}%")

    generate_svg(xs, ys, labels, models, "images/calibration.svg")

if __name__ == "__main__":
    main()
