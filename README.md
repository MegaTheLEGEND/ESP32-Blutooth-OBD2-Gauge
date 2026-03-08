# ESP32-Bluetooth-OBD2-Gauge
## Forked & Extended — Ram 1500 4.7L V8 Adaptation
Original project by Va&Cob — adapted for 2007 Ram 1500 with significant new gauge types, 80s digital dash styles, touch navigation, and display improvements.

> Original repo archived October 24th, 2025. This fork is actively maintained.

- Original Prototype: https://youtu.be/PkQaUJbzTNM
- Original Demo: https://youtu.be/vvBIeim7XTE

![](/picture/page.jpg)
![](/picture/config.jpg)
![](enclosure/enclosure.jpg)

---

## What's New in This Fork

### 🚗 Ram 1500 4.7L V8 PID Configuration
Reconfigured all PIDs for Chrysler/Dodge OBD2 protocol (ISO 15765-4 CAN):

| Gauge      | PID    | Formula         | Range       | Warn  |
|------------|--------|-----------------|-------------|-------|
| ENG Load   | 0104   | A×100/255 (%)   | 0–100%      | 80%   |
| Coolant    | 0105   | (A-40)×9/5+32°F | 40–240°F    | 210°F |
| MAP        | 010B   | A×0.145 psi     | 0–40 psi    | 35psi |
| ENG SPD    | 010C   | (256A+B)/4 rpm  | 0–5500 rpm  | 4500  |
| PCM Volt   | 0142   | (256A+B)/1000 V | 0–16V       | 15V   |
| Intake Tmp | 010F   | (A-40)×9/5+32°F | 0–150°F     | 120°F |
| Trans Tmp  | 0105*  | (A-40)×9/5+32°F | 40–240°F    | 200°F |

> *Trans temp uses coolant PID as placeholder. Real Chrysler 545RFE trans temp requires Mode 22 PID — probe with `2200` via AlfaOBD or ELM327 terminal to find your specific PID.

- All temperatures displayed in **Fahrenheit**
- Fixed integer division bug in Fahrenheit formula (`9.0/5.0` not `9/5`)
- Formula 5 and 6 include out-of-bounds guard using last known good value

### 🖥️ Display Fix
- Added `tft.invertDisplay(true)` after `tft.init()` to fix white background issue on some 2.8" TFT panels
- If colors still appear wrong, check `User_Setup.h` in the TFT_eSPI library for correct driver chip and `TFT_RGB_ORDER`

### 📊 New Gauge Types (meter.h)

**Type 4 — 7-Segment LCD Meter** (160×80)
- Classic green-on-black LCD look with scan line effect
- Off-segments rendered in dim color for authentic feel
- Color shifts green → orange → red as value approaches warning

**Type 5 — Segmented Bar Meter** (80×240)
- EQ column style with 20 stacked segments
- Three color zones: green (normal), orange (caution), red (warning)
- Scale marks and live value display at bottom

**Type 6 — Sweeping Needle Meter** (160×80)
- Real trig-based analog needle with 135°–225° sweep
- Red zone arc, tick marks with labels, chrome bezel
- Erases/redraws only ticks near old needle position for speed
- 3-line thick needle for visibility

### 🕹️ 80s Digital Dash Styles (layouts 11–13)

**Layout 11 — Corvette C4 Red LED**
- 4 full-height red LED bar columns
- Orange warning zone marker line with triangle indicator
- Dim red off-segments for authentic segmented LED look
- Inspired by 1984 C4 Corvette instrument cluster

**Layout 12 — DeLorean VFD**
- 6 cells with glowing cyan vacuum fluorescent display aesthetic
- Horizontal bar + large digit display per cell
- Color shifts cyan → yellow → orange on warning
- Blinking on warning threshold, decorative dot separator lines
- Inspired by DMC-12 instrument cluster

**Layout 13 — Amber Dot Matrix**
- 6 cells with custom 3×5 dot matrix font at 4px dot size
- Subtle dot-grid background texture
- Thin amber bar graph at cell bottom
- Color shifts amber → red-amber on warning
- Inspired by Buick Riviera / Cadillac trip computer

### 📐 Expanded Layout System
14 total layouts (0–13). `pidInCell[14][7]` — last slot must be PCM volt (pid 4, `0142`) for engine-off detection:

```cpp
const uint8_t pidInCell[14][7] = {
  { 0, 2, 3, 1, 5, 6, 4 },  // 0  - arc left, numeric right
  { 0, 2, 3, 1, 5, 6, 4 },  // 1  - all arc
  { 0, 2, 3, 1, 5, 6, 4 },  // 2  - all numeric
  { 1, 5, 6, 3, 2, 4, 4 },  // 3  - numeric + vbar
  { 2, 1, 5, 6, 4, 0, 4 },  // 4  - vbar outer, numeric inner
  { 2, 0, 1, 5, 6, 4, 4 },  // 5  - vbar left, numeric right
  { 1, 5, 6, 4, 0, 3, 4 },  // 6  - all vbar
  { 3, 2, 0, 4, 1, 5, 4 },  // 7  - all vbar alt pids
  { 0, 2, 3, 1, 5, 6, 4 },  // 8  - needle left, numeric right
  { 0, 2, 3, 1, 5, 6, 4 },  // 9  - all 7seg
  { 1, 5, 6, 3, 2, 4, 4 },  // 10 - all segbar
  { 1, 5, 6, 3, 2, 4, 4 },  // 11 - C4 red LED bars
  { 0, 2, 3, 1, 5, 6, 4 },  // 12 - DeLorean VFD
  { 0, 2, 3, 1, 5, 6, 4 },  // 13 - amber dot matrix
};
```

### 👆 Touchscreen Page Navigation
Tap left or right third of the screen to cycle layouts (touchscreen.h):
- **Left third (0–106px):** previous layout
- **Middle third:** ignored (prevents accidental swipes)
- **Right third (213–320px):** next layout
- 400ms debounce, wraps around at both ends
- Uses existing calibrated `getTouch()` — no recalibration needed

```cpp
// Forward declaration required at top of touchscreen.h:
void initScreen();
```

### 👆 Touchscreen Config Menu Navigation
Touch zones added to configuration menu (config.h):
- **Top third of screen:** previous menu item
- **Middle third of screen:** select (same as 3-second button hold)
- **Bottom third of screen:** next menu item
- Touch navigation also works inside Warning Setting submenu (prev/next parameter)
- Physical button behavior unchanged

### 🔆 Improved autoDim()
Replaced original on/off backlight with smooth phone-style dimming:
- Exponential sensor smoothing (alpha=0.08) eliminates flicker
- Hysteresis buffer (±60 ADC counts) prevents hunting
- Smooth brightness ramp (±2 per 80ms tick)
- Non-blocking — uses internal millis() timer, safe to call every loop()
- Tuning constants: `BRIGHT_LIGHT=200`, `DARK_LIGHT=2000` (adjust to your LDR in serial monitor)

### 🔌 Bluetooth Watchdog / Auto-Reconnect
Added to main loop — if no OBD2 response for 5 seconds:
1. Calls `connectLastOBDII()` to try saved device
2. Falls back to `scanBTdevice()` if that fails
3. `lastResponseTime` resets on each successful prompt and on layout change

### 🌡️ CPU Temperature in Fahrenheit
- `tempRead` now stored and displayed in °F
- Overheat threshold changed from 60°C to `140°F`
- Status line updated to show °F

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
- 7 OBD2 PIDs displayed simultaneously (Ram 4.7L configured)
- **14 selectable layout pages** (up from 8)
- **6 gauge styles:** arc, numeric, vertical bar, 7-segment, segmented bar, needle
- **3 x 80s digital dash styles:** C4 Corvette, DeLorean VFD, Amber Dot Matrix
- **Touchscreen page switching** — tap left/right to cycle layouts
- **Touchscreen config navigation** — tap top/center/bottom to navigate menus
- DTC read and clear (engine warning light)
- Per-PID adjustable warning values
- CPU overheat protection (Fahrenheit)
- Configurable auto turn-off voltage
- Smooth auto screen brightness (LDR)
- Bluetooth watchdog with auto-reconnect
- Custom screen image via micro SD card
- Firmware update via SD card or WiFi
- VIN read on About page

## Setting Default Boot Layout
In the main `.ino`, find:
```cpp
layout = pref.getUShort("layout", 0);
```
Change `0` to any layout number 0–13. This only applies on first boot or after flash erase — once a layout is saved on engine-off it will boot to that saved layout.

To force reset the saved layout, add before that line once then remove:
```cpp
pref.putUShort("layout", 11);  // force C4 layout on next boot
```

## Known Limitations / Pending
- **Trans temp PID:** Ram 545RFE real trans temp requires Chrysler Mode 22 PID — currently using coolant PID (0105) as placeholder
- **Oil temp (015C):** No factory sensor on 4.7L, returns NO DATA
- **LDR calibration:** autoDim `BRIGHT_LIGHT` and `DARK_LIGHT` constants need tuning to your specific LDR and vehicle lighting — check serial monitor output with `#define SERIAL_DEBUG`
- **Display driver:** `invertDisplay(true)` is a workaround — root cause may be wrong driver in `User_Setup.h`
