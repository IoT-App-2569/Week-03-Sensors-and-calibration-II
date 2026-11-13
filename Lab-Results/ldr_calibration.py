"""
LAB 5 (Week-03) — LDR และการทำ Calibration
-------------------------------------------------
สคริปต์นี้ทำ 3 อย่าง:
  1) จำลองค่าที่อ่านได้จากวงจร LDR + R 10kΩ (Voltage Divider) บน ESP32 ADC 12-bit
     และค่า Lux จากแอปมือถือ (เพราะไม่มีเซนเซอร์จริงตอนทำ)
  2) หาสมการ Calibration (Linear / Polynomial 2 / Polynomial 3 / Exponential / โมเดล LDR)
     ด้วยวิธี least squares แบบเดียวกับ Trendline ของ Excel
  3) สร้างไฟล์ Excel (มีกราฟ Scatter + Trendline + สมการ + R²) และรูปกราฟ PNG

รัน:  py -3.13 Lab-Results/ldr_calibration.py
ต้องมี: numpy, matplotlib, openpyxl
"""
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from openpyxl import Workbook
from openpyxl.chart import ScatterChart, Reference, Series
from openpyxl.chart.trendline import Trendline, TrendlineLabel
from openpyxl.chart.data_source import NumFmt
from openpyxl.chart.marker import Marker
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

OUT = Path(__file__).resolve().parent
IMG = OUT / "images"
IMG.mkdir(exist_ok=True)

# ---------------------------------------------------------------- 1) แบบจำลอง
VCC = 3.3          # V
R_FIXED = 10_000   # Ω  (R คงที่ในวงจร)
ADC_MAX = 4095     # 12-bit
# LDR (ประมาณรุ่น GL55xx): R_LDR = A · Lux^(-α)  →  กำหนดที่ 10 lux มีค่า 80 kΩ, α = 0.8
R_AT_10LUX = 80_000
ALPHA = 0.8
A_TRUE = R_AT_10LUX * 10 ** ALPHA

ADC_NOISE_SD = 12      # noise ของ ADC (count)
PHONE_ERR_SD = 0.03    # ความคลาดเคลื่อนของแอปมือถือ ±3 %
N_SAMPLES = 10         # อ่านค่า ADC จุดละ 10 ครั้งแล้วเฉลี่ย

rng = np.random.default_rng(2026)


def r_ldr(lux):
    return A_TRUE * np.power(lux, -ALPHA)


def vout(r):
    return VCC * R_FIXED / (r + R_FIXED)


def adc_ideal(lux):
    return vout(r_ldr(lux)) / VCC * ADC_MAX


# จุดเก็บข้อมูล Calibration (7 จุด) และจุดทดสอบใหม่ (3 จุด)
CAL_POINTS = [
    ("มืดมาก (ปิดไฟ ปิดผ้าม่าน)", 3),
    ("มืด (ในลิ้นชัก)", 15),
    ("ห้องแสงสลัว", 60),
    ("ห้องปกติ (ไฟเพดาน)", 150),
    ("ใต้โคมไฟโต๊ะ", 350),
    ("ข้างหน้าต่าง", 700),
    ("กลางแจ้ง (ในร่ม)", 1200),
]
TEST_POINTS = [
    ("ใต้โต๊ะ", 40),
    ("ห้องเรียน", 250),
    ("หน้าต่าง (แดดอ่อน)", 900),
]


def measure(points):
    rows = []
    for name, lux_true in points:
        samples = np.clip(np.round(adc_ideal(lux_true) + rng.normal(0, ADC_NOISE_SD, N_SAMPLES)), 0, ADC_MAX).astype(int)
        lux_phone = int(round(lux_true * (1 + rng.normal(0, PHONE_ERR_SD))))
        rows.append(dict(name=name, samples=samples, adc=int(round(samples.mean())),
                         sd=float(samples.std(ddof=1)), lux=max(lux_phone, 1)))
    return rows


cal = measure(CAL_POINTS)
test = measure(TEST_POINTS)

x = np.array([r["adc"] for r in cal], float)
y = np.array([r["lux"] for r in cal], float)


# ---------------------------------------------------------------- 2) Fit แบบ Excel Trendline
def r2(obs, pred):
    return 1 - np.sum((obs - pred) ** 2) / np.sum((obs - obs.mean()) ** 2)


models = {}
for deg, key, label in [(1, "lin", "Linear"), (2, "poly2", "Polynomial (อันดับ 2)"), (3, "poly3", "Polynomial (อันดับ 3)")]:
    p = np.polyfit(x, y, deg)
    models[key] = dict(label=label, coef=p, f=(lambda p: lambda a: np.polyval(p, a))(p), r2=r2(y, np.polyval(p, x)))

# Exponential: Excel fit ln(y) = ln(a) + b·x แล้วรายงาน R² บนสเกล ln
b_e, lna_e = np.polyfit(x, np.log(y), 1)
a_e = np.exp(lna_e)
models["exp"] = dict(label="Exponential", coef=(a_e, b_e), f=lambda a: a_e * np.exp(b_e * a),
                     r2=r2(np.log(y), lna_e + b_e * x))

# โมเดล LDR (ส่วน 1.3): คำนวณ R_LDR จาก ADC แล้ว fit Power: R = A·Lux^(-α)
def adc_to_r(a):
    a = np.asarray(a, float)
    return R_FIXED * (ADC_MAX / a - 1)


r_meas = adc_to_r(x)
slope_p, lnA_p = np.polyfit(np.log(y), np.log(r_meas), 1)
A_fit, alpha_fit = np.exp(lnA_p), -slope_p
models["ldr"] = dict(label="โมเดล LDR (R = A·Lux^-α)", coef=(A_fit, alpha_fit),
                     f=lambda a: np.power(adc_to_r(a) / A_fit, -1 / alpha_fit),
                     r2=r2(np.log(r_meas), lnA_p + slope_p * np.log(y)))

for m in models.values():
    m["test_pred"] = [float(m["f"](t["adc"])) for t in test]
    m["test_err"] = [abs(p - t["lux"]) / t["lux"] * 100 for p, t in zip(m["test_pred"], test)]
    m["mape"] = float(np.mean(m["test_err"]))


def eq_text(key):
    c = models[key]["coef"]
    if key == "lin":
        return f"y = {c[0]:.4f}x {c[1]:+.2f}"
    if key == "poly2":
        return f"y = {c[0]:.4E}x² {c[1]:+.4f}x {c[2]:+.2f}"
    if key == "poly3":
        return f"y = {c[0]:.4E}x³ {c[1]:+.4E}x² {c[2]:+.4f}x {c[3]:+.2f}"
    if key == "exp":
        return f"y = {c[0]:.4f}·e^({c[1]:.6f}x)"
    return f"R = {c[0]:.4E}·Lux^(-{c[1]:.4f})"


# ---------------------------------------------------------------- 3) Excel
wb = Workbook()
HDR = PatternFill("solid", fgColor="2A78D6")
HFONT = Font(bold=True, color="FFFFFF")
THIN = Side(style="thin", color="BFBFBF")
BOX = Border(left=THIN, right=THIN, top=THIN, bottom=THIN)


def header(ws, row, values, col=1):
    for i, v in enumerate(values):
        c = ws.cell(row=row, column=col + i, value=v)
        c.fill, c.font, c.border = HDR, HFONT, BOX
        c.alignment = Alignment(horizontal="center", vertical="center", wrap_text=True)


def boxed(ws, r1, r2_, c1, c2):
    for r in range(r1, r2_ + 1):
        for c in range(c1, c2 + 1):
            ws.cell(row=r, column=c).border = BOX


def widths(ws, ws_widths):
    for i, w in enumerate(ws_widths, 1):
        ws.column_dimensions[get_column_letter(i)].width = w


# --- Sheet 1: ส่วน 1.6 คำนวณแรงดันเอาต์พุต
ws = wb.active
ws.title = "1.6 Vout"
ws["A1"] = "ส่วนที่ 1.6 — คำนวณแรงดันเอาต์พุต  Vout = Vcc · R_fixed / (R_LDR + R_fixed)"
ws["A1"].font = Font(bold=True, size=13)
ws["A2"], ws["B2"] = "Vcc (V)", VCC
ws["A3"], ws["B3"] = "R_fixed (Ω)", R_FIXED
header(ws, 5, ["สภาพแสง", "R_LDR สูงสุด (Ω)", "R_LDR ต่ำสุด (Ω)", "Vout ต่ำสุด (V)", "Vout สูงสุด (V)",
               "ADC ต่ำสุด", "ADC สูงสุด", "ช่วงแรงดันเอาต์พุต"])
ranges = [("มืดสนิท", 2_000_000, 200_000), ("ห้องปกติ", 50_000, 10_000),
          ("ใต้โคมไฟ", 10_000, 2_000), ("กลางแจ้ง", 2_000, 200)]
for i, (n, rmax, rmin) in enumerate(ranges, start=6):
    ws.cell(i, 1, n)
    ws.cell(i, 2, rmax)
    ws.cell(i, 3, rmin)
    ws.cell(i, 4, f"=$B$2*$B$3/(B{i}+$B$3)").number_format = "0.000"
    ws.cell(i, 5, f"=$B$2*$B$3/(C{i}+$B$3)").number_format = "0.000"
    ws.cell(i, 6, f"=ROUND(D{i}/$B$2*4095,0)")
    ws.cell(i, 7, f"=ROUND(E{i}/$B$2*4095,0)")
    ws.cell(i, 8, f'=TEXT(D{i},"0.00")&" – "&TEXT(E{i},"0.00")&" V"')
    ws.cell(i, 2).number_format = ws.cell(i, 3).number_format = "#,##0"
boxed(ws, 6, 9, 1, 8)
widths(ws, [14, 16, 16, 15, 15, 11, 11, 20])

# --- Sheet 2: Raw data (อ่าน ADC 10 ครั้งต่อจุด)
ws = wb.create_sheet("Raw Data")
ws["A1"] = f"ข้อมูลดิบ — อ่านค่า ADC จุดละ {N_SAMPLES} ครั้ง (ห่างกัน 500 ms) แล้วหาค่าเฉลี่ย   *ข้อมูลจำลอง*"
ws["A1"].font = Font(bold=True, size=13)
header(ws, 3, ["สภาพแสง"] + [f"#{i}" for i in range(1, N_SAMPLES + 1)] + ["ADC เฉลี่ย", "SD", "Lux (มือถือ)"])
for i, r in enumerate(cal + test, start=4):
    ws.cell(i, 1, r["name"] + ("  [ทดสอบ]" if r in test else ""))
    for j, s in enumerate(r["samples"], start=2):
        ws.cell(i, j, int(s))
    last = get_column_letter(N_SAMPLES + 1)
    ws.cell(i, N_SAMPLES + 2, f"=ROUND(AVERAGE(B{i}:{last}{i}),0)")
    ws.cell(i, N_SAMPLES + 3, f"=STDEV(B{i}:{last}{i})").number_format = "0.0"
    ws.cell(i, N_SAMPLES + 4, r["lux"])
boxed(ws, 4, 3 + len(cal) + len(test), 1, N_SAMPLES + 4)
widths(ws, [30] + [7] * N_SAMPLES + [11, 7, 12])

# --- Sheet 3: Calibration + กราฟ Trendline
ws = wb.create_sheet("Calibration")
ws["A1"] = "ส่วนที่ 4–5 — ข้อมูล Calibration (ADC vs Lux) และสมการจาก Trendline"
ws["A1"].font = Font(bold=True, size=13)
header(ws, 3, ["สภาพแสง", "ค่า ADC", "ค่า Lux (มือถือ)", "R_LDR (Ω)"])
for i, r in enumerate(cal, start=4):
    ws.cell(i, 1, r["name"])
    ws.cell(i, 2, r["adc"])
    ws.cell(i, 3, r["lux"])
    ws.cell(i, 4, f"=10000*(4095/B{i}-1)").number_format = "#,##0"
first, last_row = 4, 3 + len(cal)
boxed(ws, first, last_row, 1, 4)

# ตารางค่าสัมประสิทธิ์ (ตรงกับที่ Excel แสดงบนกราฟ)
cr = last_row + 3
ws.cell(cr - 1, 1, "สัมประสิทธิ์ของแต่ละสมการ (คำนวณแบบเดียวกับ Trendline)").font = Font(bold=True)
header(ws, cr, ["สมการ", "R²", "a", "b", "c", "d", "รูปสมการ"])
coef_rows = {}
rowspec = [
    ("lin", list(models["lin"]["coef"]), "y = a·x + b"),
    ("poly2", list(models["poly2"]["coef"]), "y = a·x² + b·x + c"),
    ("poly3", list(models["poly3"]["coef"]), "y = a·x³ + b·x² + c·x + d"),
    ("exp", list(models["exp"]["coef"]), "y = a·e^(b·x)"),
    ("ldr", list(models["ldr"]["coef"]), "R = a·Lux^(-b)  →  Lux = (R/a)^(-1/b)"),
]
for k, (key, coefs, form) in enumerate(rowspec, start=cr + 1):
    coef_rows[key] = k
    ws.cell(k, 1, models[key]["label"])
    ws.cell(k, 2, round(models[key]["r2"], 4)).number_format = "0.0000"
    for j, c in enumerate(coefs, start=3):
        ws.cell(k, j, float(c)).number_format = "0.000000E+00"
    ws.cell(k, 7, form)
boxed(ws, cr + 1, cr + len(rowspec), 1, 7)
widths(ws, [30, 10, 15, 15, 15, 15, 34])


def scatter(title, ttype, order=None, anchor="I3", xcol=2, ycol=3, ws_=ws, xt="ค่า ADC", yt="ค่า Lux (มือถือ)", log=False):
    ch = ScatterChart()
    ch.title = title
    ch.style = 13
    ch.x_axis.title, ch.y_axis.title = xt, yt
    ch.height, ch.width = 8.5, 15
    ch.legend = None
    xs = Reference(ws_, min_col=xcol, min_row=first, max_row=last_row)
    ys = Reference(ws_, min_col=ycol, min_row=first, max_row=last_row)
    s = Series(ys, xs, title=yt)
    s.marker = Marker(symbol="circle", size=8)
    s.marker.graphicalProperties.solidFill = "2A78D6"
    s.marker.graphicalProperties.line.solidFill = "2A78D6"
    s.graphicalProperties.line.noFill = True
    tl = Trendline(trendlineType=ttype, dispEq=True, dispRSqr=True)
    if order:
        tl.order = order
    if ttype in ("poly", "exp", "power"):
        # ให้สมการบนกราฟแสดงเลขนัยสำคัญพอ (ค่าเริ่มต้นของ Excel จะปัดเป็น 1E-07)
        tl.trendlineLbl = TrendlineLabel(numFmt=NumFmt(formatCode="0.0000E+00", sourceLinked=False))
    s.trendline = tl
    ch.series.append(s)
    ch.x_axis.delete = False
    ch.y_axis.delete = False
    if log:
        ch.x_axis.scaling.logBase = 10
        ch.y_axis.scaling.logBase = 10
    ws_.add_chart(ch, anchor)


scatter("Linear — ค่า Lux (มือถือ)", "linear", anchor="I3")
scatter("Polynomial อันดับ 2 — ค่า Lux (มือถือ)", "poly", 2, anchor="S3")
scatter("Polynomial อันดับ 3 — ค่า Lux (มือถือ)", "poly", 3, anchor="I21")
scatter("Exponential — ค่า Lux (มือถือ)", "exp", anchor="S21")
scatter("โมเดล LDR: R_LDR vs Lux (Power, log-log)", "power", anchor="I39",
        xcol=3, ycol=4, xt="Lux (มือถือ)", yt="R_LDR (Ω)", log=True)

# --- Sheet 4: ทดสอบจุดใหม่
ws = wb.create_sheet("Test")
ws["A1"] = "ส่วนที่ 7 — ทดสอบ 3 จุดแสงใหม่ (ไม่ได้ใช้ตอนทำ Calibration)"
ws["A1"].font = Font(bold=True, size=13)
header(ws, 3, ["สภาพแสงใหม่", "ADC", "Lux มือถือ",
               "Lux (Linear)", "Lux (Poly2)", "Lux (Poly3)", "Lux (Exp)", "Lux (โมเดล LDR)",
               "%Err Linear", "%Err Poly2", "%Err Poly3", "%Err Exp", "%Err โมเดล LDR"])
C = lambda key, col: f"Calibration!${col}${coef_rows[key]}"
for i, t in enumerate(test, start=4):
    ws.cell(i, 1, t["name"])
    ws.cell(i, 2, t["adc"])
    ws.cell(i, 3, t["lux"])
    B = f"B{i}"
    ws.cell(i, 4, f"={C('lin','C')}*{B}+{C('lin','D')}")
    ws.cell(i, 5, f"={C('poly2','C')}*{B}^2+{C('poly2','D')}*{B}+{C('poly2','E')}")
    ws.cell(i, 6, f"={C('poly3','C')}*{B}^3+{C('poly3','D')}*{B}^2+{C('poly3','E')}*{B}+{C('poly3','F')}")
    ws.cell(i, 7, f"={C('exp','C')}*EXP({C('exp','D')}*{B})")
    ws.cell(i, 8, f"=((10000*(4095/{B}-1))/{C('ldr','C')})^(-1/{C('ldr','D')})")
    for j in range(4, 9):
        ws.cell(i, j).number_format = "0.0"
        col = get_column_letter(j)
        ws.cell(i, j + 5, f"=ABS({col}{i}-C{i})/C{i}").number_format = "0.0%"
end = 3 + len(test)
ws.cell(end + 1, 1, "ค่าเฉลี่ย %Error (MAPE)").font = Font(bold=True)
for j in range(9, 14):
    col = get_column_letter(j)
    ws.cell(end + 1, j, f"=AVERAGE({col}4:{col}{end})").number_format = "0.0%"
    ws.cell(end + 1, j).font = Font(bold=True)
boxed(ws, 4, end + 1, 1, 13)
widths(ws, [22, 8, 11, 12, 12, 12, 12, 15, 11, 11, 11, 11, 14])

ws.cell(end + 3, 1, "สรุป").font = Font(bold=True)
best = min(models, key=lambda k: models[k]["mape"])
ws.cell(end + 4, 1, f"สมการที่แม่นยำที่สุดบนจุดทดสอบ: {models[best]['label']} (MAPE ≈ {models[best]['mape']:.1f}%)")
ws.cell(end + 5, 1, f"ในกลุ่ม Linear/Polynomial: Polynomial อันดับ 3 ดีที่สุด (MAPE ≈ {models['poly3']['mape']:.1f}%) — Linear ใช้ไม่ได้ (MAPE ≈ {models['lin']['mape']:.1f}%)")

xlsx_path = OUT / "LDR_Calibration.xlsx"
wb.save(xlsx_path)

# ---------------------------------------------------------------- 4) PNG
plt.rcParams.update({"font.family": ["Leelawadee UI", "DejaVu Sans"], "font.size": 10,
                     "axes.spines.top": False, "axes.spines.right": False,
                     "axes.edgecolor": "#8a8a85", "axes.grid": True, "grid.color": "#e6e6e3",
                     "grid.linewidth": 0.8})
BLUE, ORANGE = "#2a78d6", "#eb6834"
xx = np.linspace(x.min() * 0.95, x.max() * 1.01, 400)

fig, axs = plt.subplots(2, 2, figsize=(11, 8), dpi=130)
for ax, key in zip(axs.flat, ["lin", "poly2", "poly3", "exp"]):
    m = models[key]
    ax.plot(xx, m["f"](xx), color=ORANGE, lw=2, ls="--", label="Trendline")
    ax.scatter(x, y, s=46, color=BLUE, edgecolor="white", linewidth=1.5, zorder=3, label="ข้อมูล Calibration")
    ax.set_title(m["label"], fontsize=12, loc="left", fontweight="bold")
    ax.text(0.03, 0.95, f"{eq_text(key)}\nR² = {m['r2']:.4f}", transform=ax.transAxes, va="top", fontsize=9,
            bbox=dict(boxstyle="round,pad=0.4", fc="white", ec="#d0d0cc"))
    ax.set_xlabel("ค่า ADC (12-bit)")
    ax.set_ylabel("ค่า Lux (มือถือ)")
    ax.set_ylim(-150, 1400)
axs[0, 0].legend(loc="lower right", frameon=False, fontsize=9)
fig.suptitle("กราฟ Scatter ADC vs Lux พร้อม Trendline 4 แบบ (ข้อมูลจำลอง)", fontsize=14, fontweight="bold")
fig.tight_layout()
fig.savefig(IMG / "01_trendline_compare.png")
plt.close(fig)

# กราฟแบบเดียวกับตัวอย่างในใบงาน (Polynomial อันดับ 2)
fig, ax = plt.subplots(figsize=(7.5, 4.5), dpi=130)
ax.plot(x, y, color=BLUE, lw=2, marker="o", ms=6, label="ค่า Lux (มือถือ)")
ax.plot(xx, models["poly3"]["f"](xx), color=ORANGE, lw=1.6, ls=":", label="Polynomial อันดับ 3")
ax.text(0.04, 0.94, f"{eq_text('poly3')}\nR² = {models['poly3']['r2']:.4f}", transform=ax.transAxes, va="top", fontsize=10)
ax.set_title("ค่า Lux (มือถือ) เทียบกับค่า ADC", fontsize=13, fontweight="bold")
ax.set_xlabel("ค่า ADC")
ax.set_ylabel("Lux")
ax.legend(loc="center left", frameon=False)
fig.tight_layout()
fig.savefig(IMG / "02_scatter_poly3.png")
plt.close(fig)

# R_LDR vs Lux (log-log) — ยืนยันสมการ 1.3
fig, ax = plt.subplots(figsize=(7.5, 4.5), dpi=130)
ll = np.logspace(np.log10(y.min() * 0.8), np.log10(y.max() * 1.2), 200)
ax.plot(ll, A_fit * ll ** -alpha_fit, color=ORANGE, lw=2, ls="--", label="Power trendline")
ax.scatter(y, r_meas, s=46, color=BLUE, edgecolor="white", linewidth=1.5, zorder=3, label="R_LDR ที่คำนวณจาก ADC")
ax.set_xscale("log"); ax.set_yscale("log")
ax.text(0.45, 0.92, f"R = {A_fit:,.0f}·Lux^(-{alpha_fit:.3f})\nR² = {models['ldr']['r2']:.4f}", transform=ax.transAxes, va="top")
ax.set_title("R_LDR vs Lux (สเกล log-log) — เป็นเส้นตรงตาม R = A·Lux^-α", fontsize=12, fontweight="bold")
ax.set_xlabel("Lux (มือถือ)")
ax.set_ylabel("R_LDR (Ω)")
ax.legend(loc="lower left", frameon=False)
fig.tight_layout()
fig.savefig(IMG / "03_R_vs_Lux_loglog.png")
plt.close(fig)

# ผลทดสอบ
order = ["lin", "poly2", "poly3", "exp", "ldr"]
fig, ax = plt.subplots(figsize=(8, 4.2), dpi=130)
vals = [models[k]["mape"] for k in order]
names = ["Linear", "Poly 2", "Poly 3", "Exponential", "โมเดล LDR"]
bars = ax.barh(names, vals, color=BLUE, height=0.55)
bars[order.index(best)].set_color(ORANGE)
for b_, v in zip(bars, vals):
    ax.text(v + 1, b_.get_y() + b_.get_height() / 2, f"{v:.1f}%", va="center", fontsize=10)
ax.invert_yaxis()
ax.grid(axis="y", visible=False)
ax.set_xlabel("ค่าเฉลี่ยความคลาดเคลื่อน (%) บน 3 จุดทดสอบใหม่")
ax.set_title("เปรียบเทียบความแม่นยำของสมการ Calibration", fontsize=13, fontweight="bold")
ax.set_xlim(0, max(vals) * 1.15)
fig.tight_layout()
fig.savefig(IMG / "04_test_error.png")
plt.close(fig)

# ---------------------------------------------------------------- 5) สรุปตัวเลข
print("== Calibration data")
for r in cal:
    print(f"  {r['name']:<28} ADC={r['adc']:>5}  SD={r['sd']:5.1f}  Lux={r['lux']}")
print("== Models")
for k in order:
    m = models[k]
    print(f"  {m['label']:<26} R2={m['r2']:.4f}  {eq_text(k)}  coef={list(map(float, m['coef']))}")
    print(f"      test pred={[round(v,1) for v in m['test_pred']]}  err%={[round(v,1) for v in m['test_err']]}  MAPE={m['mape']:.1f}%")
print("== Test")
for t in test:
    print(f"  {t['name']:<20} ADC={t['adc']}  Lux={t['lux']}")
print("best:", best)
print("saved:", xlsx_path)
