"""Build the bill of materials: bom/bom_v2.(xlsx|csv) (round GC9A01, default) and
bom/bom_v1.(xlsx|csv) (2.0" ST7789 variant).

Prices were checked in October 2026 via web search; anything marked ESTIMATE was not
verified. Gross prices (incl. VAT). Two sourcing columns: Polish retail shops (PLN)
and AliExpress (USD). Run: python3 tools/make_bom.py
"""
import csv, os
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment
from openpyxl.utils import get_column_letter

USD_PLN = 3.9247  # NBP table A, 2026-10-23
MARGIN = 0.40     # of the selling price
VAT = 0.23
BATCH = 10
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "bom")

BOARD = ("ESP32-C3 SuperMini (ESP32-C3FN4, 4 MB flash, USB-C)", "MCU, Wi-Fi, NTP, web server", "yes", 1,
         "Gotronik", "https://www.gotronik.pl/plytka-deweloperska-esp32-c3-supermini-wifi-bluetooth-risc-v-p-15201.html",
         22.00, 1, "verified (product page, 2026-10-13)",
         "https://www.aliexpress.us/item/3256806205678831.html", 3.00,
         "from search results (typically 1.45-3.00 USD), listing not opened")
SCREEN_GC9A01 = ("Round 1.28\" IPS TFT 240x240, GC9A01, 7 pins (RST CS DC SDA SCL GND VCC), no BL pin", "display", "yes", 1,
                 "Kamami", "https://kamami.pl/wyswietlacze-tftips/1193819-gc9a01-okragly-wyswietlacz-tft-ips-128-240240-px-spi-5902186315617.html",
                 27.90, 1, "verified (Kamami 27.90 PLN, ~4 week lead time)",
                 "https://www.aliexpress.com/w/wholesale-gc9a01-round-display.html", 1.33,
                 "from search results (0.99-1.33 USD; 0.99 = new-customer promo), listing not opened")
SCREEN_ST7789 = ("2.0\" IPS TFT 240x320 SPI, ST7789 (e.g. Waveshare 17344)", "display", "yes", 1,
                 "Kamami", "https://kamami.pl/wyswietlacze-tftips/1182299-waveshare-17344-240-320-general-2inch-ips-lcd-display-module-5906623425068.html",
                 44.43, 1, "verified (Kamami, incl. PH2.0 8-pin cable)",
                 "https://www.aliexpress.com/w/wholesale-2.0-inch-st7789-240x320-ips.html", 4.56,
                 "from search results; check 'IPS' and 'SPI' in the listing")
COMMON = [
    ("Photoresistor GL5528 10-20 kOhm (pack of 10)", "auto-dimming (optional)", "optional", 1,
     "Botland", "https://botland.com.pl/fotorezystory/4598-fotorezystor-10-20k-gl5528-10szt-5903351245715.html",
     4.90, 10, "verified (Botland, 4.90 PLN / 10 pcs)",
     "https://www.aliexpress.com/w/wholesale-GL5528.html", 0.05, "ESTIMATE"),
    ("Resistor 10 kOhm 0.25 W", "LDR divider (optional)", "optional", 1,
     "any shop", "https://botland.com.pl/szukaj?s=rezystor+10k", 0.10, 1, "ESTIMATE (usually sold in packs)",
     "https://www.aliexpress.com/w/wholesale-10k-resistor.html", 0.02, "ESTIMATE"),
    ("Tact switch 6x6 mm THT (pack of 5)", "button: light up / Wi-Fi reset (optional)", "optional", 1,
     "Botland", "https://botland.com.pl/tact-switch/3495-tact-switch-6x6mm-5mm-tht-2pin-5szt-5904422307639.html",
     1.99, 5, "verified (Botland, 1.99 PLN / 5 pcs)",
     "https://www.aliexpress.com/w/wholesale-tact-switch-6x6.html", 0.03, "ESTIMATE"),
    ("Female-female jumper wires 10 cm (pack of 40)", "display-to-board wiring (no soldering)", "yes", 10,
     "Botland", "https://botland.com.pl/przewody-polaczeniowe-zensko-zenskie/19948-przewody-polaczeniowe-zensko-zenskie-justpi-10cm-40szt-5904422328672.html",
     4.99, 40, "verified (Botland, 4.99 PLN / 40 pcs)",
     "https://www.aliexpress.com/w/wholesale-dupont-female-female-10cm.html", 0.03, "ESTIMATE"),
    ("USB-A to USB-C cable 1 m", "5 V power", "yes", 1,
     "any shop", "", 10.00, 1, "ESTIMATE", "https://www.aliexpress.com/w/wholesale-usb-c-cable-1m.html", 1.50, "ESTIMATE"),
    ("3D-printed PLA enclosure (~40 g) + 2x M2 screws", "enclosure", "yes", 1,
     "self-printed", "", 5.00, 1, "ESTIMATE (PLA ~80 PLN/kg)", "", 1.27, "ESTIMATE"),
]
OPTIONAL_PSU = ("Optional (not in totals): 5V/3A USB-C power supply - any phone charger works",
                "https://botland.com.pl/zasilacze-do-raspberry-pi-4b/14516-zasilacz-justpi-usb-c-5v3a-do-raspberry-pi-4b-5903351241403.html",
                32.90, "verified (Botland 32.90 PLN)")
SHIP_PL = 42.00       # ESTIMATE: 3 shops x ~14 PLN (parcel locker from 13.99 PLN)
SHIP_PL_PROTO = 28.00 # ESTIMATE: prototype without the screen = 2 shops
SHIP_ALI_USD = 2.00   # ESTIMATE: many listings ship free above 10 USD

VARIANTS = {
    "v2": {"title": "Kido Clock - BOM v2 (ESP32-C3 SuperMini + round 1.28\" GC9A01)", "rows": [BOARD, SCREEN_GC9A01] + COMMON},
    "v1": {"title": "Kido Clock - BOM v1 (ESP32-C3 SuperMini + 2.0\" ST7789)", "rows": [BOARD, SCREEN_ST7789] + COMMON},
}
ON_HAND = {1}  # row index of the display: "prototype if you already have the screen" columns count it as 0

HDR = ["#", "Part", "Function", "Required", "Qty / device",
       "Shop (PL)", "Link (PL)", "PL price per pack [PLN gross]", "Pcs per pack", "PL cost / device [PLN]", "PL price status",
       "AliExpress link", "Ali price per pc [USD]", "Ali cost / device [USD]", "Ali cost / device [PLN]", "Ali price status",
       "Prototype, screen on hand, PL [PLN]", "Prototype, screen on hand, Ali [USD]"]


def build(tag, v):
    rows = v["rows"]
    wb = Workbook(); ws = wb.active; ws.title = "BOM"
    ws["A1"] = v["title"]; ws["A1"].font = Font(bold=True, size=14)
    ws["A2"] = f"USD/PLN = {USD_PLN} (NBP table A, 2026-10-23). Gross prices. Checked 2026-10-26; ESTIMATE = not verified."
    for cell, label, val in (("S1", "USD/PLN", USD_PLN), ("S2", "Margin (of selling price)", MARGIN), ("S3", "VAT", VAT), ("S4", "Batch [pcs]", BATCH)):
        ws[cell] = label; ws[cell].font = Font(bold=True); ws["T" + cell[1:]] = val
    r0 = 4
    for j, h in enumerate(HDR, 1):
        c = ws.cell(row=r0, column=j, value=h)
        c.font = Font(bold=True, color="FFFFFF"); c.fill = PatternFill("solid", fgColor="1B1640")
        c.alignment = Alignment(wrap_text=True, vertical="center")
    est = PatternFill("solid", fgColor="FFF2CC")
    for i, row in enumerate(rows, 1):
        (name, fn, req, qty, shop, link, ppack, npack, st, alink, ausd, ast) = row
        r = r0 + i
        for j, val in enumerate([i, name, fn, req, qty, shop, link, ppack, npack, None, st, alink, ausd, None, None, ast], 1):
            ws.cell(row=r, column=j, value=val)
        ws.cell(row=r, column=10, value=f"=H{r}/I{r}*E{r}")
        ws.cell(row=r, column=14, value=f"=M{r}*E{r}")
        ws.cell(row=r, column=15, value=f"=N{r}*$T$1")
        ws.cell(row=r, column=17, value=0 if (i - 1) in ON_HAND else f"=J{r}")
        ws.cell(row=r, column=18, value=0 if (i - 1) in ON_HAND else f"=N{r}")
        if "ESTIMATE" in st: ws.cell(row=r, column=8).fill = est
        if "ESTIMATE" in ast: ws.cell(row=r, column=13).fill = est
    last = r0 + len(rows); rs = last + 1
    ws.cell(row=rs, column=2, value="TOTAL parts / device").font = Font(bold=True)
    for col, L in ((10, "J"), (14, "N"), (15, "O"), (17, "Q"), (18, "R")):
        ws.cell(row=rs, column=col, value=f"=SUM({L}{r0+1}:{L}{last})").font = Font(bold=True)
    ws.cell(row=rs + 1, column=2, value="Shipping per order (ESTIMATE: PL batch 3 shops, prototype 2 shops; Ali ~2 USD)")
    for col, val in ((10, SHIP_PL), (14, SHIP_ALI_USD), (17, SHIP_PL_PROTO), (18, SHIP_ALI_USD)):
        ws.cell(row=rs + 1, column=col, value=val).fill = est
    ws.cell(row=rs + 1, column=15, value=f"=N{rs+1}*$T$1")
    ws.cell(row=rs + 3, column=2, value=OPTIONAL_PSU[0]); ws.cell(row=rs + 3, column=7, value=OPTIONAL_PSU[1])
    ws.cell(row=rs + 3, column=8, value=OPTIONAL_PSU[2]); ws.cell(row=rs + 3, column=11, value=OPTIONAL_PSU[3])
    for j, w in enumerate([4, 46, 34, 10, 9, 13, 40, 12, 8, 12, 34, 40, 10, 11, 11, 34, 14, 14, 26, 10], 1):
        ws.column_dimensions[get_column_letter(j)].width = w
    ws.row_dimensions[r0].height = 45; ws.freeze_panes = "C5"

    s = wb.create_sheet("Summary")
    s["A1"] = "Landed cost and retail price for a 40% margin"; s["A1"].font = Font(bold=True, size=13)
    s["A2"] = "Margin is of the selling price: price = cost / (1 - 0.40). Gross price = net x 1.23 (Polish VAT)."
    hdr = ["Scenario", "Landed cost / pc [PLN]", "Landed cost / pc [USD]", "Net price (40% margin) [PLN]",
           "Net price (40% margin) [USD]", "Gross price incl. 23% VAT [PLN]"]
    for j, h in enumerate(hdr, 1):
        c = s.cell(row=4, column=j, value=h); c.font = Font(bold=True, color="FFFFFF")
        c.fill = PatternFill("solid", fgColor="1B1640"); c.alignment = Alignment(wrap_text=True)
    B = "'BOM'!"
    sc = [("A. Prototype, screen on hand, rest from PL shops", f"={B}Q{rs}+{B}Q{rs+1}"),
          (f"B. PL shops, batch of {BATCH} (shipping / {BATCH})", f"={B}J{rs}+{B}J{rs+1}/{B}T4"),
          ("C. Prototype, screen on hand, rest from AliExpress", f"=({B}R{rs}+{B}R{rs+1})*{B}T1"),
          (f"D. AliExpress, batch of {BATCH}", f"={B}O{rs}+{B}O{rs+1}/{B}T4")]
    for i, (n, f) in enumerate(sc, 5):
        s.cell(row=i, column=1, value=n); s.cell(row=i, column=2, value=f)
        s.cell(row=i, column=3, value=f"=B{i}/{B}T1"); s.cell(row=i, column=4, value=f"=B{i}/(1-{B}T2)")
        s.cell(row=i, column=5, value=f"=D{i}/{B}T1"); s.cell(row=i, column=6, value=f"=D{i}*(1+{B}T3)")
        for j in range(2, 7): s.cell(row=i, column=j).number_format = "0.00"
    s["A11"] = "Notes:"; s["A11"].font = Font(bold=True)
    for k, n in enumerate(["Excludes labour (~20-30 min/unit), marketplace fees and conformity assessment (CE/RED/EMC; toy safety for children's products).",
                           "AliExpress prices come from search results, include EU VAT collected by AliExpress and change daily.",
                           "Yellow cells = estimates. Prices are a snapshot from October 2026."], 12):
        s.cell(row=k, column=1, value="- " + n)
    for j, w in enumerate([52, 16, 16, 18, 18, 18], 1): s.column_dimensions[get_column_letter(j)].width = w
    os.makedirs(OUT, exist_ok=True)
    wb.save(os.path.join(OUT, f"bom_{tag}.xlsx"))

    pl = [r[6] / r[7] * r[3] for r in rows]; ali = [r[10] * r[3] for r in rows]
    plp = sum(x for i, x in enumerate(pl) if i not in ON_HAND); alip = sum(x for i, x in enumerate(ali) if i not in ON_HAND)
    res = []
    with open(os.path.join(OUT, f"bom_{tag}.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(HDR)
        for i, r in enumerate(rows, 1):
            (name, fn, req, qty, shop, link, ppack, npack, st, alink, ausd, ast) = r
            w.writerow([i, name, fn, req, qty, shop, link, f"{ppack:.2f}", npack, f"{pl[i-1]:.2f}", st, alink, f"{ausd:.2f}",
                        f"{ali[i-1]:.2f}", f"{ali[i-1]*USD_PLN:.2f}", ast,
                        "0.00" if (i - 1) in ON_HAND else f"{pl[i-1]:.2f}", "0.00" if (i - 1) in ON_HAND else f"{ali[i-1]:.2f}"])
        w.writerow(["", "TOTAL parts", "", "", "", "", "", "", "", f"{sum(pl):.2f}", "", "", "", f"{sum(ali):.2f}",
                    f"{sum(ali)*USD_PLN:.2f}", "", f"{plp:.2f}", f"{alip:.2f}"])
        w.writerow(["", "Shipping (ESTIMATE)", "", "", "", "", "", "", "", f"{SHIP_PL:.2f}", "", "", "", f"{SHIP_ALI_USD:.2f}",
                    f"{SHIP_ALI_USD*USD_PLN:.2f}", "", f"{SHIP_PL_PROTO:.2f}", f"{SHIP_ALI_USD:.2f}"])
        w.writerow([])
        w.writerow(["Scenario", "Landed [PLN]", "Landed [USD]", "Net price 40% margin [PLN]", "[USD]", "Gross incl. VAT [PLN]"])
        for n, c in [("A. Prototype, screen on hand, PL", plp + SHIP_PL_PROTO), (f"B. PL batch {BATCH}", sum(pl) + SHIP_PL / BATCH),
                     ("C. Prototype, screen on hand, Ali", (alip + SHIP_ALI_USD) * USD_PLN),
                     (f"D. Ali batch {BATCH}", (sum(ali) + SHIP_ALI_USD / BATCH) * USD_PLN)]:
            p = c / (1 - MARGIN)
            w.writerow([n, f"{c:.2f}", f"{c/USD_PLN:.2f}", f"{p:.2f}", f"{p/USD_PLN:.2f}", f"{p*(1+VAT):.2f}"])
            res.append((n, c, p))
    return res


if __name__ == "__main__":
    for tag, v in VARIANTS.items():
        print(tag)
        for n, c, p in build(tag, v):
            print(f"  {n:40s} landed {c:7.2f} PLN ({c/USD_PLN:6.2f} USD)  net {p:7.2f} PLN  gross {p*1.23:7.2f} PLN")
