# ESP32-Bluetooth-OBD2-Gauge
## Forked & Extended — Ram 1500 4.7L V8 Adaptation
Original project by Va&Cob — adapted for 2007 Ram 1500 with significant new gauge types, 80s digital dash styles, touch navigation, full-screen graphs, and display improvements.

> Original repo archived October 24th, 2025. This fork is actively maintained.

- Original Prototype: https://youtu.be/PkQaUJbzTNM
- Original Demo: https://youtu.be/vvBIeim7XTE

![](/picture/page.jpg)
![](/picture/config.jpg)
![](enclosure/enclosure.jpg)

---

## What's New in This Fork

### 🚗 Ram 1500 4.7L V8 PID Configuration
Reconfigured all PIDs for Chrysler/Dodge OBD2 protocol (ISO 15765-4 CAN).
Expanded from 7 to **20 PIDs**:

| # | Gauge      | PID    | Formula              | Range      | Warn   |
|---|------------|--------|----------------------|------------|--------|
| 0 | ENG Load   | 0104   | A×100/255 (%)        | 0–100%     | 80%    |
| 1 | Coolant    | 0105   | (A-40)×9/5+32 °F     | 40–240°F   | 210°F  |
| 2 | MAP        | 010B   | A×0.145 psi          | 0–40 psi   | 35 psi |
| 3 | ENG SPD    | 010C   | (256A+B)/4 rpm       | 0–5500 rpm | 4500   |
| 4 | PCM Volt   | 0142   | (256A+B)/1000 V      | 0–16V      | 15V    |
| 5 | Intake Tmp | 010F   | (A-40)×9/5+32 °F     | 0–150°F    | 120°F  |
| 6 | Trans Tmp  | 0105*  | (A-40)×9/5+32 °F     | 40–240°F   | 200°F  |
| 7 | Throttle   | 0111   | A×100/255 (%)        | 0–100%     | 90%    |
| 8 | VSS        | 010D   | A (km/h raw)         | 0–120      | 110    |
| 9 | Short Fuel | 0106   | (A/1.28)-100 %       | -25–25%    | 20%    |
|10 | Long Fuel  | 0107   | (A/1.28)-100 %       | -25–25%    | 20%    |
|11 | MAF        | 010C   | (256A+B)/100 g/s     | 0–300      | 250    |
|12 | O2 B1S1    | 0114   | A/200 V              | 0–1.275V   | —      |
|13 | O2 B1S2    | 0115   | A/200 V              | 0–1.275V   | —      |
|14 | Timing     | 010E   | (A/2)-64 deg         | 0–60°      | 55°    |
|15 | Baro       | 0133   | A×0.145 psi          | 10–17 psi  | 16 psi |
|16 | EGR Err    | 012D   | (A/1.28)-100 %       | -25–25%    | 20%    |
|17 | Fuel Lvl   | 012F   | A×100/255 (%)        | 0–100%     | 10%    |
|18 | Abs Load   | 0143   | (256A+B)/1000        | 0–100%     | 90%    |
|19 | Rel Throt  | 0145   | A×100/255 (%)        | 0–100%     | 90%    |

> *Trans temp uses coolant PID as placeholder. Real Chrysler 545RFE trans temp requires Mode 22 PID `221F03` — probe with `2200` via AlfaOBD or ELM327 terminal to confirm.

> MAF (pid 11) will return NO DATA on the 4.7L — it is speed-density, not mass-airflow.

Formula reference:
```
0  = raw byte → psi (MAP/baro)
1  = celsius offset → fahrenheit
2  = byte → percentage
3  = two-byte RPM
4  = two-byte voltage or scaled load
5  = two-byte temp with bounds check
6  = Ford-specific (unused)
7  = signed fuel trim percent
8  = MAF grams/sec
9  = O2 sensor voltage (0–1.275V)
10 = ignition timing degrees BTDC
```

### 🖥️ Display Fix
- Added `tft.invertDisplay(true)` after `tft.init()` to fix white background issue on some 2.8" TFT panels
- If colors still appear wrong, check `User_Setup.h` in TFT_eSPI for correct driver chip and `TFT_RGB_ORDER`

### 📊 Gauge Types (meter.h)

| Constant   | Name          | Size    | Description |
|------------|---------------|---------|-------------|
| GT_ARC     | Arc Meter     | 160×80  | Semicircle sweep arc |
| GT_NUMERIC | Numeric       | 160×80  | Large digital number |
| GT_VBAR    | Vertical Bar  | 80×240  | Gradient fill bar with tick marks |
| GT_SEG7    | 7-Segment LCD | 160×80  | Green-on-black LCD with scan lines |
| GT_SEGBAR  | Segmented Bar | 80×240  | 20-segment EQ column, 3 color zones |
| GT_NEEDLE  | Needle        | 160×80  | Trig-based analog sweep 135–225° |
| GT_C4BAR   | C4 Red LED    | 80×240  | Corvette C4 style red LED segments |
| GT_VFD     | DeLorean VFD  | 160×80  | Cyan vacuum fluorescent display |
| GT_DOT     | Dot Matrix    | 160×80  | Amber 3×5 custom font dot matrix |

### 📐 Unified Layout Descriptor System
All layout configuration lives in a single `layoutDefs[]` array in `layouts.h`.
**Adding a new layout = one new entry. Nothing else changes.**

```cpp
{ "All Needle", GRID_6CELL, 6, {
  {0, GT_NEEDLE}, {2, GT_NEEDLE}, {3, GT_NEEDLE},
  {1, GT_NEEDLE}, {5, GT_NEEDLE}, {6, GT_NEEDLE}
}, 4 },
```

`max_layout` is computed automatically via `sizeof`. `initScreen()`, `updateMeter()`, and `handleTouch()` all read from `layoutDefs` — no switch/case per layout anywhere.

Three grid types:
- `GRID_6CELL` — two 160px columns × three 80px rows (6 cells)
- `GRID_4COL` — four 80px full-height columns (4 cells)
- `GRID_MIXED` — left 160px numeric column + right two 80px bar columns (5 cells)

**37 layouts included (0–36)** covering all gauge types and PID combinations.

### 📈 Full-Screen Scrolling Graph
Tap any sensor cell to open a full-screen scrolling line graph:
- 318 samples, one pixel per sample, scrolls right-to-left
- Orange warning threshold line across the graph
- Live current value updates in header bar
- Color shifts green → red above warning threshold
- Tap anywhere to exit back to gauge view

### 👆 Touchscreen Navigation

**Page switching — swipe:**
- Swipe right → next layout
- Swipe left → previous layout
- Must be >50px horizontal and more horizontal than vertical (prevents false triggers on tap)
- Layout saved to NVS flash on every page change

**Graph — tap:**
- Tap any gauge cell → opens full-screen graph for that sensor
- Tap anywhere on graph → exits back to gauges

**Config menu:**
- Top third → previous menu item
- Middle third → select (same as 3-second button hold)
- Bottom third → next menu item
- Same zones work inside Warning Setting submenu

### ⚙️ Dynamic Warning Settings Page
Warning configuration auto-scales to any number of PIDs:
- Scrollable list — 7 visible rows with orange scrollbar
- Tap any row to select it
- Footer buttons: decrease / reset to default / increase
- Step size auto-scales by PID range (1 / 5 / 10 / 500)
- Built entirely from `pidConfig` at runtime — no hardcoded list

### 🔆 Improved autoDim()
Smooth phone-style backlight dimming:
- Exponential smoothing (alpha=0.08) eliminates flicker
- Hysteresis buffer (±60 ADC counts) prevents hunting
- Smooth ramp (±2 per 80ms tick)
- Non-blocking millis() timer
- Tuning: `BRIGHT_LIGHT=200`, `DARK_LIGHT=2000` — adjust via serial monitor

### 🔌 Bluetooth Watchdog / Auto-Reconnect
If no OBD2 response for 15 seconds:
1. Tries `connectLastOBDII()` with saved device address
2. Falls back to `scanBTdevice()` if that fails
3. `lastResponseTime` resets on each `>` prompt, layout change, and skip loop

### 🌡️ CPU Temperature in Fahrenheit
- `tempRead` stored and displayed in °F
- Overheat threshold: `140°F`

### 🎞️ About Page
- NES chiptune music plays on Core 0 during animation
- Non-blocking scrolling credit line
- VIN read from ECU displayed on page
- Animation exits immediately on button press

---

## File Structure

| File | Purpose |
|------|---------|
| `VaAndCobOBD2Gauge.ino` | Main sketch — globals, setup(), loop() |
| `layouts.h` | Layout descriptor structs and `layoutDefs[]` table |
| `meter.h` | All gauge drawing, graph, initScreen, updateMeter |
| `touchscreen.h` | Touch calibration, getTouch, handleTouch |
| `config.h` | Config menu, warning settings, about page |
| `bluetooth.h` | BT scan and connect |
| `image.h` | Bitmap image data |
| `elm327.h` | VIN read helper |
| `firmware_update.h` | OTA / SD firmware update |
| `music.h` / `nes_audio.h` | About page chiptune |

**Include order in `.ino` matters:**
```cpp
#include "image.h"
// ... globals and functions ...
#include "bluetooth.h"
#include "layouts.h"      // before meter.h
#include "meter.h"        // before touchscreen.h
#include "touchscreen.h"
#include "config.h"
```

---

## Software
- Arduino IDE 1.8.18 or 2.3.2
- ESP32 Arduino core **2.0.17** — ⚠️ does NOT work with 3.x.x
- Modified TFT_eSPI library 2.5.43

## Hardware
- ESP32 TFT 2.8" 320×240 with **resistive** touch — https://s.click.aliexpress.com/e/_Ew95gMl
- ELM327 Bluetooth adapter v1.5 recommended (not v2.1) — https://s.click.aliexpress.com/e/_oo3THvG
- Small speaker 1W 8Ω 9×28mm
- Push button 6×6×5 2-leg
- 3D printed enclosure
- 12V→5V DC micro USB power regulator
- Gauge magnetic stand

## Features
- 20 OBD2 PIDs configured (Ram 4.7L)
- **37 selectable layout pages**
- **9 gauge styles:** arc, numeric, vertical bar, 7-segment, segmented bar, needle, C4 LED, VFD, dot matrix
- **Full-screen scrolling line graph** — tap any cell
- **Swipe to change pages**, tap to open graph
- **Dynamic warning settings** — auto-scales to any number of PIDs
- DTC read and clear (engine warning light)
- Per-PID adjustable warning values with auto step sizing
- CPU overheat protection (°F)
- Configurable auto turn-off voltage
- Smooth auto screen brightness (LDR)
- Bluetooth watchdog with auto-reconnect
- Layout saved to flash on every page change
- Custom screen image via micro SD card
- Firmware update via SD card or WiFi
- VIN read on About page
- NES chiptune on About page

## Setting Default Boot Layout
```cpp
// In VaAndCobOBD2Gauge.ino:
layout = pref.getUShort("layout", 0);  // change 0 to desired layout 0-36
```
Only applies on first boot or after flash erase — once a layout is saved it boots to that layout.

---

## Known Limitations / Pending
- **Trans temp PID:** Ram 545RFE real trans temp requires Mode 22 PID `221F03` — currently using coolant PID as placeholder. Verify with AlfaOBD before enabling.
- **Oil temp (015C):** No factory sensor on 4.7L, returns NO DATA
- **MAF (pid 11):** 4.7L is speed-density — MAF will return NO DATA
- **VSS (pid 8):** Returns km/h raw — reads metric but useful for relative comparison
- **LDR calibration:** `BRIGHT_LIGHT=200` and `DARK_LIGHT=2000` need tuning to your LDR — check serial monitor with `#define SERIAL_DEBUG`
- **Display driver:** `invertDisplay(true)` is a workaround — root cause may be wrong driver chip in `User_Setup.h`