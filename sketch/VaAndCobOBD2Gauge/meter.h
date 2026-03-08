// ============================================================
// meter.h - Complete gauge display functions
// Ram 4.7 OBD2 Gauge - All layouts 0-13
// ============================================================

// ============================================================
// COLOR PALETTE
// ============================================================
#define GAUGE_BG        0x0000  // black face
#define GAUGE_BEZEL     0x4208  // dark gunmetal bezel
#define GAUGE_CHROME    0xC618  // chrome/silver ring
#define GAUGE_AMBER     0xFD20  // warm amber
#define GAUGE_CREAM     0xF7BE  // cream white
#define GAUGE_RED_ZONE  0xF800  // red warning
#define GAUGE_ORANGE    0xFC60  // orange pre-warning
#define GAUGE_DIM       0x2945  // dim grey
#define GAUGE_GOLD      0xFEA0  // gold accent

// 80s dash color palette
#define C4_RED          0xF800  // Corvette C4 red LED
#define C4_RED_DIM      0x3000  // dim red for off segments
#define C4_ORANGE       0xFC00  // C4 orange warning
#define C4_BG           0x0000  // black background
#define C4_BEZEL        0x1800  // very dark red bezel

#define VFD_CYAN        0x07FF  // DeLorean vacuum fluorescent cyan
#define VFD_CYAN_DIM    0x0155  // dim cyan for off state
#define VFD_TEAL        0x0410  // teal accent
#define VFD_BG          0x0001  // near black with blue tint
#define VFD_BEZEL       0x0841  // dark teal bezel

#define DOT_AMBER       0xFD00  // amber dot matrix
#define DOT_AMBER_DIM   0x3200  // dim amber off state
#define DOT_BG          0x0820  // very dark warm black
#define DOT_BEZEL       0x3200  // dark amber bezel
#define DOT_GOLD        0xFEA0  // gold accent lines

// ============================================================
// SCREEN AND CELL GEOMETRY
// ============================================================
const uint16_t screenWidth  = tft.height();  // landscape
const uint16_t screenHeight = tft.width();

/*---- CELL COORDINATES ----
 Type 0: numericMeter / arcMeter / seg7Meter / needleMeter / vfdMeter / dotMeter
 Type 1: vBarMeter / segBarMeter / c4BarMeter

 160x80 cell grid:
 1 , 4 , 7
 2 , 5 , 8
 3 , 6 , 9  */
const uint16_t cell1_x[9] = { 0, 0, 0, 80, 80, 80, 160, 160, 160 };
const uint16_t cell1_y[9] = { 0, 80, 160, 0, 80, 160, 0, 80, 160 };

/* 80x240 vertical bar cell grid:
 1 , 2 , 3 , 4  */
const uint16_t cell2_x[4] = { 0, 80, 160, 240 };

bool blinkCell[9] = { false, false, false, false, false, false, false, false, false };

String  pidList[maxpidIndex]        = {};
uint8_t pidReadSkip[maxpidIndex]    = {};
uint8_t pidCurrentSkip[maxpidIndex] = {};
uint8_t engine_off_count            = 0;
float   old_data[maxpidIndex]       = {};

// autoDim counters (kept for compatibility)
uint8_t low_count  = 0;
uint8_t high_count = 0;

const float factoryECUOff = 12.5;
float ecu_off_volt        = factoryECUOff;

// ============================================================
// MATH
// ============================================================
#include <math.h>

// ============================================================
// SHARED HELPER - horizontal LED bar
// ============================================================
void drawLEDBar(int x, int y, int w, int h, int segments,
                int filled, uint16_t onColor, uint16_t offColor) {
  int gap  = 2;
  int segW = (w - (segments - 1) * gap) / segments;
  for (int i = 0; i < segments; i++) {
    int sx    = x + i * (segW + gap);
    uint16_t c = (i < filled) ? onColor : offColor;
    tft.fillRect(sx, y, segW, h, c);
  }
}

// ============================================================
// SHARED HELPER - 3x5 dot matrix font
// ============================================================
const uint8_t dotFont[11][5] = {
  { 0b111, 0b101, 0b101, 0b101, 0b111 },  // 0
  { 0b010, 0b110, 0b010, 0b010, 0b111 },  // 1
  { 0b111, 0b001, 0b111, 0b100, 0b111 },  // 2
  { 0b111, 0b001, 0b111, 0b001, 0b111 },  // 3
  { 0b101, 0b101, 0b111, 0b001, 0b001 },  // 4
  { 0b111, 0b100, 0b111, 0b001, 0b111 },  // 5
  { 0b111, 0b100, 0b111, 0b101, 0b111 },  // 6
  { 0b111, 0b001, 0b001, 0b001, 0b001 },  // 7
  { 0b111, 0b101, 0b111, 0b101, 0b111 },  // 8
  { 0b111, 0b101, 0b111, 0b001, 0b111 },  // 9
  { 0b000, 0b000, 0b010, 0b000, 0b000 },  // .
};

void drawDotChar(int x, int y, uint8_t ch, int dotSize,
                 uint16_t onColor, uint16_t offColor) {
  uint8_t idx = (ch >= '0' && ch <= '9') ? ch - '0' : 10;
  for (int row = 0; row < 5; row++) {
    for (int col = 0; col < 3; col++) {
      bool     lit = (dotFont[idx][row] >> (2 - col)) & 1;
      uint16_t c   = lit ? onColor : offColor;
      tft.fillRect(x + col * (dotSize + 1),
                   y + row * (dotSize + 1),
                   dotSize, dotSize, c);
    }
  }
}

void drawDotNumber(int x, int y, float value, bool useDecimal,
                   int dotSize, uint16_t onColor, uint16_t offColor) {
  String str = String(value, useDecimal ? 1 : 0);
  while (str.length() < 4) str = " " + str;
  str        = str.substring(str.length() - 4);
  int charW  = 3 * (dotSize + 1) + 2;
  for (uint8_t i = 0; i < 4; i++) {
    char c = str.charAt(i);
    if (c == ' ') {
      tft.fillRect(x + i * charW, y, charW, 5 * (dotSize + 1), offColor);
    } else if (c >= '0' && c <= '9') {
      drawDotChar(x + i * charW, y, c, dotSize, onColor, offColor);
    }
  }
}

// ============================================================
// 7-SEGMENT HELPERS
// ============================================================
const uint8_t seg7[11] = {
  0b0111111,  // 0
  0b0000110,  // 1
  0b1011011,  // 2
  0b1001111,  // 3
  0b1100110,  // 4
  0b1101101,  // 5
  0b1111101,  // 6
  0b0000111,  // 7
  0b1111111,  // 8
  0b1101111,  // 9
  0b1000000   // -
};

void drawSegDigit(int x, int y, uint8_t digit, uint8_t size,
                  uint16_t color, uint16_t bg) {
  if (digit > 10) digit = 10;
  uint8_t  segs     = seg7[digit];
  int      s        = size * 3;
  int      t        = size;
  int      g        = size / 2;
  uint16_t dimColor = 0x2104;

  tft.fillRect(x + g,     y,             s, t, (segs & 0x01) ? color : dimColor);  // top
  tft.fillRect(x + g + s, y + g,         t, s, (segs & 0x02) ? color : dimColor);  // top-right
  tft.fillRect(x + g + s, y + g + s + g, t, s, (segs & 0x04) ? color : dimColor);  // bot-right
  tft.fillRect(x + g,     y + g+s+g+s,   s, t, (segs & 0x08) ? color : dimColor);  // bottom
  tft.fillRect(x,         y + g + s + g, t, s, (segs & 0x10) ? color : dimColor);  // bot-left
  tft.fillRect(x,         y + g,         t, s, (segs & 0x20) ? color : dimColor);  // top-left
  tft.fillRect(x + g,     y + g + s,     s, t, (segs & 0x40) ? color : dimColor);  // middle
}

void drawSeg7Number(int x, int y, float value, bool digit, uint8_t size,
                    uint16_t color, uint16_t bg) {
  String str  = String(value, digit ? 1 : 0);
  while (str.length() < 4) str = " " + str;
  str         = str.substring(str.length() - 4);
  int charW   = size * 3 + size * 2 + size / 2 + 2;
  for (uint8_t i = 0; i < 4; i++) {
    char c = str.charAt(i);
    if (c == ' ') {
      tft.fillRect(x + i * charW, y, charW, size * 3 * 2 + size * 3, bg);
    } else if (c >= '0' && c <= '9') {
      drawSegDigit(x + i * charW, y, c - '0', size, color, bg);
    } else if (c == '-') {
      drawSegDigit(x + i * charW, y, 10, size, color, bg);
    }
  }
}

// ============================================================
// NEEDLE HELPER
// ============================================================
void drawNeedle(int cx, int cy, int angle_deg, int length, uint16_t color) {
  float rad = angle_deg * 3.14159265 / 180.0;
  int   x2  = cx + length * cos(rad);
  int   y2  = cy + length * sin(rad);
  tft.drawLine(cx,     cy,     x2,     y2,     color);
  tft.drawLine(cx + 1, cy,     x2 + 1, y2,     color);
  tft.drawLine(cx,     cy + 1, x2,     y2 + 1, color);
}

// ============================================================
// GAUGE TYPE 1: NUMERIC METER  160x80
// ============================================================
void numericMeter(uint8_t cell, uint8_t pid) {
  int    w     = screenWidth / 2;
  int    h     = screenHeight / 3;
  int    x     = cell1_x[cell - 1];
  int    y     = cell1_y[cell - 1];
  String label = pidConfig[pid][0];

  tft.drawRect(x, y, w, screenHeight, TFT_GREY);
  tft.fillRect(x + 2, y + 2, w - 5, h - 5, TFT_BLACK);
  tft.fillRectVGradient(x + 2, y + 2, w - 5, h * 0.33, TFT_BLUE, 0x0011);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + w / 2, y + 2, 4);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawRightString("---", x + w - 5, y + (h * 0.25) + 11, 6);
}

void plotNumeric(uint8_t cell, float data, int warn, bool digit) {
  int compare = warn / data;
  if (compare == 0) {
    int    w = screenWidth / 2 - 1;
    int    h = screenHeight / 3 - 1;
    int    x = cell1_x[cell - 1];
    int    y = cell1_y[cell - 1];
    if (blinkCell[cell]) {
      String result = String(data, digit);
      switch (result.length()) {
        case 2: result = "    " + result; break;
        case 3: result = "   " + result; break;
        case 4: result = "  " + result; break;
      }
      old_data[pidIndex] = data;
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.drawRightString(result.c_str(), x + w - 5, y + (h * 0.25) + 11, 6);
    } else {
      tft.fillRect(x + 2, y + 28, w - 5, 50, TFT_BLACK);
    }
    blinkCell[cell] = !blinkCell[cell];
  } else {
    if (data != old_data[pidIndex]) {
      int    w = screenWidth / 2 - 1;
      int    h = screenHeight / 3 - 1;
      int    x = cell1_x[cell - 1];
      int    y = cell1_y[cell - 1];
      String result = String(data, digit);
      switch (result.length()) {
        case 2: result = "    " + result; break;
        case 3: result = "   " + result; break;
        case 4: result = "  " + result; break;
      }
      tft.setTextColor(TFT_GREEN, TFT_BLACK);
      tft.drawRightString(result.c_str(), x + w - 5, y + (h * 0.25) + 11, 6);
      old_data[pidIndex] = data;
    }
  }
}

// ============================================================
// GAUGE TYPE 2: ARC METER  160x80
// ============================================================
void arcMeter(uint8_t cell, uint8_t pid) {
  int    w     = screenWidth / 2;
  int    h     = screenHeight / 3;
  int    x     = cell1_x[cell - 1];
  int    y     = cell1_y[cell - 1];
  String label = pidConfig[pid][0];
  String unit  = pidConfig[pid][1];
  int    mn    = pidConfig[pid][4].toInt();
  int    mx    = pidConfig[pid][5].toInt();

  tft.fillRect(x, y, w, h, TFT_GREY);
  tft.fillRect(x + 2, y + 2, 156, 76, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(String(mn).c_str(), x + 16, y + 10, 2);
  tft.drawCentreString(String(mx).c_str(), x + w - 16, y + 10, 2);
  tft.drawRightString(unit.c_str(), x + 150, y + 60, 2);
  tft.drawCentreString(label.c_str(), x + 80, y + 58, 2);
  tft.setTextColor(TFT_GREEN);
  tft.drawString("---", x + 10, y + 60, 2);
  for (int angle = 135; angle < 225; angle++)
    tft.drawSmoothArc(x + 80, y + 90, 80, 60, 134, angle, TFT_GREEN, TFT_DARKGREY, false);
  for (int angle = 224; angle > 135; angle--)
    tft.drawSmoothArc(x + 80, y + 90, 80, 60, angle + 1, 226, TFT_WHITE, TFT_DARKGREY, false);
}

void plotArc(uint8_t cell, String label, float data, int warn,
             int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int    x      = cell1_x[cell - 1];
    int    y      = cell1_y[cell - 1];
    String result = String(data, digit);
    switch (result.length()) {
      case 2: result = result + "   "; break;
      case 3: result = result + "  "; break;
      case 4: result = result + " "; break;
    }
    if (data < min) data = min;
    if (data > max) data = max;
    int arcColor = (warn / data == 0) ? TFT_RED : TFT_GREEN;
    tft.setTextColor(arcColor, TFT_BLACK);
    tft.drawString(result.c_str(), x + 10, y + 60, 2);
    int angle = map(data, min, max, 135, 225);
    if (data >= old_data[pidIndex])
      tft.drawSmoothArc(x + 80, y + 90, 80, 60, 134, angle, arcColor, TFT_DARKGREY, false);
    else
      tft.drawSmoothArc(x + 80, y + 90, 80, 60, angle + 1, 226, TFT_WHITE, TFT_DARKGREY, false);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 3: VERTICAL BAR  80x240
// ============================================================
void vBarMeter(uint8_t cell, uint8_t pid) {
  int    x     = cell2_x[cell - 1];
  int    w     = screenWidth / 4;
  String label = pidConfig[pid][0];
  String unit  = pidConfig[pid][1];
  int    mx    = pidConfig[pid][5].toInt();

  tft.drawRect(x, 0, w, screenHeight, TFT_DARKGREY);
  tft.fillRectVGradient(x + 2, 2, w - 3, screenHeight * 0.12 - 10, TFT_BLUE, 0x0011);
  tft.fillRect(x + 2, screenHeight * 0.12, w - 3, screenHeight - screenHeight * 0.24, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawCentreString(label, x + w / 2, 2, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(unit, x + 3, screenHeight * 0.12 - 8, 1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawCentreString("---", x + w / 2, screenHeight - (screenHeight * 0.12) + 1, 4);
  float smalltick = screenHeight * 0.75 / 50;
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  for (int i = 0; i < 51; i++) {
    tft.drawFastHLine(x + w / 3, screenHeight * 0.12 + smalltick * i + 3, 10, TFT_WHITE);
    if (i % 5 == 0) {
      tft.drawFastHLine(x + w / 3, screenHeight * 0.12 + smalltick * i + 3, 20, TFT_WHITE);
      if (i % 10 == 0) {
        int mark = mx - (mx * i * 2 / 100);
        tft.drawRightString(String(mark), x + w - 3, screenHeight * 0.12 + smalltick * i - 7, 2);
      }
    }
  }
  for (int barH = 0; barH < 183; barH++)
    tft.fillRect(x + 5, 212 - barH, 20, barH, TFT_RED);
  for (int barH = 182; barH >= 0; barH--)
    tft.fillRect(x + 5, 29, 20, 183 - barH, TFT_BLACK);
}

void plotVBar(uint8_t cell, float data, int warn, int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int x        = cell2_x[cell - 1];
    int w        = screenWidth / 4;
    if (data < min) data = min;
    if (data > max) data = max;
    int barColor = (warn / data == 0) ? TFT_RED : TFT_GREEN;
    int barH     = map(data, min, max, 0, 182);
    if (data > old_data[pidIndex])
      tft.fillRectVGradient(x + 5, 212 - barH, 20, barH, barColor, 0x8000);
    else
      tft.fillRect(x + 5, 29, 20, 183 - barH, TFT_BLACK);
    tft.setTextColor(barColor, TFT_BLACK);
    String result = String(data, digit);
    switch (result.length()) {
      case 2: result = "   " + result + "   "; break;
      case 3: result = "  " + result + "  "; break;
      case 4: result = " " + result + " "; break;
    }
    tft.drawCentreString(result.c_str(), x + w / 2, screenHeight - (screenHeight * 0.12) + 1, 4);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 4: 7-SEGMENT LCD METER  160x80
// ============================================================
void seg7Meter(uint8_t cell, uint8_t pid) {
  int    w     = screenWidth / 2;
  int    h     = screenHeight / 3;
  int    x     = cell1_x[cell - 1];
  int    y     = cell1_y[cell - 1];
  String label = pidConfig[pid][0];
  String unit  = pidConfig[pid][1];

  tft.fillRect(x, y, w, h, 0x2104);
  tft.fillRect(x + 3, y + 3, w - 6, h - 6, 0x0120);
  for (int ly = y + 3; ly < y + h - 3; ly += 2)
    tft.drawFastHLine(x + 3, ly, w - 6, 0x0100);
  tft.setTextColor(0x07E0, 0x0120);
  tft.drawString(label, x + 6, y + 5, 1);
  tft.setTextColor(0x03E0, 0x0120);
  tft.drawRightString(unit, x + w - 6, y + 5, 1);
  tft.drawFastHLine(x + 3, y + 14, w - 6, 0x0300);
  drawSeg7Number(x + 8, y + 18, 0, false, 3, 0x2104, 0x0120);
}

void plotSeg7(uint8_t cell, float data, int warn, bool digit) {
  if (data != old_data[pidIndex]) {
    int      x        = cell1_x[cell - 1];
    int      y        = cell1_y[cell - 1];
    int      compare  = warn / data;
    uint16_t segColor = (compare == 0)        ? 0xF800 :
                        (data >= warn * 0.85) ? 0xFC00 : 0x07E0;
    if (compare == 0 && blinkCell[cell])
      drawSeg7Number(x + 8, y + 18, data, digit, 3, 0x2104, 0x0120);
    else
      drawSeg7Number(x + 8, y + 18, data, digit, 3, segColor, 0x0120);
    if (compare == 0) blinkCell[cell] = !blinkCell[cell];
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 5: SEGMENTED BAR  80x240
// ============================================================
#define SEG_BAR_SEGMENTS 20
#define SEG_BAR_GAP       2

void segBarMeter(uint8_t cell, uint8_t pid) {
  int    x       = cell2_x[cell - 1];
  int    w       = screenWidth / 4;
  String label   = pidConfig[pid][0];
  String unit    = pidConfig[pid][1];
  int    maxVal  = pidConfig[pid][5].toInt();
  int    warnVal = pidConfig[pid][8].toInt();

  tft.fillRect(x, 0, w, screenHeight, 0x2104);
  tft.fillRect(x + 2, 2, w - 4, screenHeight - 4, TFT_BLACK);
  tft.fillRect(x + 2, 2, w - 4, 22, 0x1082);
  tft.setTextColor(TFT_WHITE, 0x1082);
  tft.drawCentreString(label, x + w / 2, 4, 2);
  tft.drawFastHLine(x + 2, 24, w - 4, 0x4208);
  tft.setTextColor(0x4208, TFT_BLACK);
  tft.drawCentreString(unit, x + w / 2, 26, 1);

  int barTop  = 34;
  int barBot  = screenHeight - 22;
  int barH    = barBot - barTop;
  int segH    = (barH - (SEG_BAR_SEGMENTS - 1) * SEG_BAR_GAP) / SEG_BAR_SEGMENTS;
  int warnSeg = (int)(SEG_BAR_SEGMENTS * (1.0 - (float)warnVal / maxVal));

  for (int i = 0; i < SEG_BAR_SEGMENTS; i++) {
    int      sy       = barTop + i * (segH + SEG_BAR_GAP);
    uint16_t offColor = (i < warnSeg * 0.2) ? 0x3000 :
                        (i < warnSeg * 0.5) ? 0x3200 : 0x0240;
    tft.fillRect(x + 8, sy, w - 16, segH, offColor);
  }
  for (int i = 0; i <= 4; i++) {
    int my      = barTop + (barH * i / 4);
    int markVal = maxVal - (maxVal * i / 4);
    tft.drawFastHLine(x + w - 8, my, 6, 0x4208);
    tft.setTextColor(0x4208, TFT_BLACK);
    if (i < 4) tft.drawRightString(String(markVal), x + w - 10, my + 2, 1);
  }
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString("---", x + w / 2, screenHeight - 18, 4);
}

void plotSegBar(uint8_t cell, float data, int warn, int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int x       = cell2_x[cell - 1];
    int w       = screenWidth / 4;
    if (data < min) data = min;
    if (data > max) data = max;

    int barTop  = 34;
    int barBot  = screenHeight - 22;
    int barH    = barBot - barTop;
    int segH    = (barH - (SEG_BAR_SEGMENTS - 1) * SEG_BAR_GAP) / SEG_BAR_SEGMENTS;
    int warnSeg = (int)(SEG_BAR_SEGMENTS * (1.0 - (float)warn / max));
    int litSegs = map(data, min, max, 0, SEG_BAR_SEGMENTS);

    for (int i = 0; i < SEG_BAR_SEGMENTS; i++) {
      int      sy  = barTop + (SEG_BAR_SEGMENTS - 1 - i) * (segH + SEG_BAR_GAP);
      bool     lit = (i < litSegs);
      uint16_t segColor;
      if (i >= SEG_BAR_SEGMENTS - warnSeg)
        segColor = lit ? 0xF800 : 0x3000;
      else if (i >= SEG_BAR_SEGMENTS - warnSeg - (SEG_BAR_SEGMENTS / 5))
        segColor = lit ? 0xFC00 : 0x3200;
      else
        segColor = lit ? 0x07E0 : 0x0240;
      tft.fillRect(x + 8, sy, w - 16, segH, segColor);
    }
    uint16_t txtColor = (warn / data == 0) ? 0xF800 : TFT_WHITE;
    String   result   = String(data, digit);
    while (result.length() < 3) result = " " + result;
    tft.setTextColor(txtColor, TFT_BLACK);
    tft.drawCentreString(result.c_str(), x + w / 2, screenHeight - 18, 4);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 6: SWEEPING NEEDLE METER  160x80
// ============================================================
void needleMeter(uint8_t cell, uint8_t pid) {
  int    w       = screenWidth / 2;
  int    h       = screenHeight / 3;
  int    x       = cell1_x[cell - 1];
  int    y       = cell1_y[cell - 1];
  String label   = pidConfig[pid][0];
  String unit    = pidConfig[pid][1];
  int    minVal  = pidConfig[pid][4].toInt();
  int    maxVal  = pidConfig[pid][5].toInt();
  int    warnVal = pidConfig[pid][8].toInt();
  int    cx      = x + w / 2;
  int    cy      = y + h + 10;

  tft.fillRect(x, y, w, h, 0x1082);
  tft.fillRect(x + 3, y + 3, w - 6, h - 6, TFT_BLACK);
  tft.drawRect(x, y, w, h, GAUGE_CHROME);
  tft.drawRect(x + 1, y + 1, w - 2, h - 2, GAUGE_CHROME);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawCentreString(label, cx, y + 4, 2);
  tft.setTextColor(0x8410, TFT_BLACK);
  tft.drawCentreString(unit, cx, y + 18, 1);
  tft.drawFastHLine(x + 4, y + 27, w - 8, 0x2104);

  int warnAngle = map(warnVal, minVal, maxVal, 135, 225);
  for (int i = 0; i <= 18; i++) {
    float    adeg  = 135.0 + i * 5.0;
    float    arad  = adeg * 3.14159 / 180.0;
    bool     major = (i % 3 == 0);
    int      ro    = major ? 68 : 66;
    int      ri    = major ? 56 : 61;
    float    tv    = minVal + (maxVal - minVal) * i / 18.0;
    uint16_t tc    = (tv >= warnVal) ? 0xF800 : TFT_WHITE;
    tft.drawLine(cx + ro * cos(arad), cy + ro * sin(arad),
                 cx + ri * cos(arad), cy + ri * sin(arad), tc);
    if (major) {
      int lx = cx + 46 * cos(arad);
      int ly = cy + 46 * sin(arad);
      if (ly > y + 28 && ly < y + h - 2) {
        tft.setTextColor(tc, TFT_BLACK);
        tft.drawCentreString(String((int)tv), lx, ly - 4, 1);
      }
    }
  }
  tft.drawSmoothArc(cx, cy, 70, 64, warnAngle, 226, 0xF800, TFT_BLACK, false);
  tft.fillCircle(cx, cy, 6, 0x4208);
  tft.fillCircle(cx, cy, 4, TFT_WHITE);
  tft.fillCircle(cx, cy, 2, 0x4208);
  drawNeedle(cx, cy, 135, 62, TFT_WHITE);
}

void plotNeedle(uint8_t cell, String label, float data, int warn,
                int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int w  = screenWidth / 2;
    int h  = screenHeight / 3;
    int x  = cell1_x[cell - 1];
    int y  = cell1_y[cell - 1];
    int cx = x + w / 2;
    int cy = y + h + 10;
    if (data < min) data = min;
    if (data > max) data = max;

    int oldAngle = map((int)old_data[pidIndex], min, max, 135, 225);
    drawNeedle(cx, cy, oldAngle, 62, TFT_BLACK);

    for (int i = 0; i <= 18; i++) {
      float adeg = 135.0 + i * 5.0;
      if (abs(adeg - oldAngle) < 8) {
        float    arad  = adeg * 3.14159 / 180.0;
        bool     major = (i % 3 == 0);
        int      ro    = major ? 68 : 66;
        int      ri    = major ? 56 : 61;
        float    tv    = min + (max - min) * i / 18.0;
        uint16_t tc    = (tv >= warn) ? 0xF800 : TFT_WHITE;
        tft.drawLine(cx + ro * cos(arad), cy + ro * sin(arad),
                     cx + ri * cos(arad), cy + ri * sin(arad), tc);
        if (major) {
          int lx = cx + 46 * cos(arad);
          int ly = cy + 46 * sin(arad);
          if (ly > y + 28 && ly < y + h - 2) {
            tft.setTextColor(tc, TFT_BLACK);
            tft.drawCentreString(String((int)tv), lx, ly - 4, 1);
          }
        }
      }
    }
    int      newAngle   = map((int)data, min, max, 135, 225);
    uint16_t needleColor = (data >= warn) ? 0xF800 : TFT_WHITE;
    drawNeedle(cx, cy, newAngle, 62, needleColor);
    tft.fillCircle(cx, cy, 6, 0x4208);
    tft.fillCircle(cx, cy, 4, TFT_WHITE);
    tft.fillCircle(cx, cy, 2, 0x4208);

    uint16_t txtColor = (data >= warn)        ? 0xF800 :
                        (data >= warn * 0.85) ? 0xFC00 : TFT_WHITE;
    String result = String(data, digit);
    while (result.length() < 4) result = " " + result;
    tft.setTextColor(txtColor, TFT_BLACK);
    tft.drawCentreString(result.c_str(), cx, y + h - 14, 2);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 7: CORVETTE C4 RED LED BAR  80x240
// ============================================================
void c4BarMeter(uint8_t cell, uint8_t pid) {
  int    x       = cell2_x[cell - 1];
  int    w       = screenWidth / 4;
  String label   = pidConfig[pid][0];
  String unit    = pidConfig[pid][1];
  int    maxVal  = pidConfig[pid][5].toInt();
  int    warnVal = pidConfig[pid][8].toInt();

  tft.fillRect(x, 0, w, screenHeight, C4_BEZEL);
  tft.fillRect(x + 2, 2, w - 4, screenHeight - 4, C4_BG);
  tft.fillRect(x + 2, 2, w - 4, 20, 0x2800);
  tft.drawFastHLine(x + 2, 22, w - 4, C4_RED_DIM);
  tft.setTextColor(C4_RED, 0x2800);
  tft.drawCentreString(label, x + w / 2, 4, 2);
  tft.setTextColor(C4_RED_DIM, C4_BG);
  tft.drawCentreString(unit, x + w / 2, 24, 1);

  int warnY = map(warnVal, 0, maxVal, screenHeight - 28, 32);
  tft.drawFastHLine(x + 2, warnY, w - 4, C4_ORANGE);
  tft.fillTriangle(x + 2, warnY, x + 6, warnY - 4, x + 6, warnY + 4, C4_ORANGE);

  tft.setTextColor(C4_RED_DIM, C4_BG);
  for (int i = 0; i <= 4; i++) {
    int sy  = 32 + ((screenHeight - 60) * i / 4);
    int val = maxVal - (maxVal * i / 4);
    tft.drawString(String(val), x + 3, sy, 1);
  }

  int barX   = x + w - 18;
  int barTop = 32;
  int barH2  = screenHeight - 60;
  int segH   = (barH2 - 19 * 2) / 20;
  for (int i = 0; i < 20; i++) {
    int sy = barTop + i * (segH + 2);
    tft.fillRect(barX, sy, 14, segH, C4_RED_DIM);
  }
  tft.fillRect(x + 2, screenHeight - 26, w - 4, 22, 0x1000);
  tft.setTextColor(C4_RED, 0x1000);
  tft.drawCentreString("----", x + w / 2, screenHeight - 22, 2);
}

void plotC4Bar(uint8_t cell, float data, int warn, int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int x      = cell2_x[cell - 1];
    int w      = screenWidth / 4;
    int barX   = x + w - 18;
    int barTop = 32;
    int barH2  = screenHeight - 60;
    int segH   = (barH2 - 19 * 2) / 20;
    if (data < min) data = min;
    if (data > max) data = max;
    int litSegs = map(data, min, max, 0, 20);

    for (int i = 0; i < 20; i++) {
      int      sy       = barTop + (19 - i) * (segH + 2);
      bool     lit      = (i < litSegs);
      float    segVal   = min + (max - min) * i / 20.0;
      uint16_t segColor = (segVal >= warn) ? (lit ? C4_ORANGE : C4_RED_DIM)
                                           : (lit ? C4_RED    : C4_RED_DIM);
      tft.fillRect(barX, sy, 14, segH, segColor);
    }
    tft.fillRect(x + 2, screenHeight - 26, w - 4, 22, 0x1000);
    uint16_t txtColor = (data >= warn) ? C4_ORANGE : C4_RED;
    String   result   = String(data, digit);
    while (result.length() < 4) result = " " + result;
    tft.setTextColor(txtColor, 0x1000);
    tft.drawCentreString(result.c_str(), x + w / 2, screenHeight - 22, 2);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 8: DELOREAN VFD METER  160x80
// ============================================================
void vfdMeter(uint8_t cell, uint8_t pid) {
  int    w       = screenWidth / 2;
  int    h       = screenHeight / 3;
  int    x       = cell1_x[cell - 1];
  int    y       = cell1_y[cell - 1];
  String label   = pidConfig[pid][0];
  String unit    = pidConfig[pid][1];
  int    minVal  = pidConfig[pid][4].toInt();
  int    maxVal  = pidConfig[pid][5].toInt();
  int    warnVal = pidConfig[pid][8].toInt();

  tft.fillRect(x, y, w, h, VFD_BEZEL);
  tft.fillRect(x + 2, y + 2, w - 4, h - 4, VFD_BG);
  tft.drawRect(x + 3, y + 3, w - 6, h - 6, VFD_TEAL);

  tft.setTextColor(VFD_CYAN, VFD_BG);
  tft.drawCentreString(label, x + w / 2, y + 5, 2);

  for (int i = x + 10; i < x + w - 10; i += 6)
    tft.fillRect(i, y + 18, 3, 2, VFD_TEAL);

  int barY  = y + 22;
  int barH2 = 12;
  int barW  = w - 20;
  int segs  = 16;
  drawLEDBar(x + 10, barY, barW, barH2, segs, 0, VFD_CYAN, VFD_CYAN_DIM);

  tft.setTextColor(VFD_TEAL, VFD_BG);
  tft.drawString(String(minVal), x + 4, barY + 2, 1);
  tft.drawRightString(String(maxVal), x + w - 4, barY + 2, 1);

  int warnSeg = map(warnVal, minVal, maxVal, 0, segs);
  int warnX   = x + 10 + warnSeg * (barW / segs);
  tft.drawFastVLine(warnX, barY - 2, barH2 + 4, C4_ORANGE);

  tft.setTextColor(VFD_CYAN, VFD_BG);
  tft.drawCentreString("----", x + w / 2, y + 40, 4);
  tft.setTextColor(VFD_TEAL, VFD_BG);
  tft.drawRightString(unit, x + w - 5, y + h - 12, 1);
}

void plotVFD(uint8_t cell, String label, float data,
             int warn, int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int w = screenWidth / 2;
    int h = screenHeight / 3;
    int x = cell1_x[cell - 1];
    int y = cell1_y[cell - 1];
    if (data < min) data = min;
    if (data > max) data = max;

    uint16_t mainColor = (data >= warn)        ? C4_ORANGE :
                         (data >= warn * 0.85) ? 0xFFE0    : VFD_CYAN;

    int barY    = y + 22;
    int barH2   = 12;
    int barW    = w - 20;
    int segs    = 16;
    int filled  = map(data, min, max, 0, segs);
    drawLEDBar(x + 10, barY, barW, barH2, segs, filled, mainColor, VFD_CYAN_DIM);

    int warnSeg = map(warn, min, max, 0, segs);
    int warnX   = x + 10 + warnSeg * (barW / segs);
    tft.drawFastVLine(warnX, barY - 2, barH2 + 4, C4_ORANGE);

    String result = String(data, digit);
    while (result.length() < 4) result = " " + result;

    if (data >= warn && blinkCell[cell]) {
      tft.fillRect(x + 4, y + 38, w - 8, 22, VFD_BG);
    } else {
      tft.setTextColor(mainColor, VFD_BG);
      tft.drawCentreString(result.c_str(), x + w / 2, y + 40, 4);
    }
    if (data >= warn) blinkCell[cell] = !blinkCell[cell];
    old_data[pidIndex] = data;
  }
}

// ============================================================
// GAUGE TYPE 9: AMBER DOT MATRIX METER  160x80
// ============================================================
void dotMeter(uint8_t cell, uint8_t pid) {
  int    w      = screenWidth / 2;
  int    h      = screenHeight / 3;
  int    x      = cell1_x[cell - 1];
  int    y      = cell1_y[cell - 1];
  String label  = pidConfig[pid][0];
  String unit   = pidConfig[pid][1];
  int    minVal = pidConfig[pid][4].toInt();
  int    maxVal = pidConfig[pid][5].toInt();

  tft.fillRect(x, y, w, h, DOT_BEZEL);
  tft.fillRect(x + 2, y + 2, w - 4, h - 4, DOT_BG);

  // dot grid background
  for (int gx = x + 4; gx < x + w - 4; gx += 4)
    for (int gy = y + 4; gy < y + h - 4; gy += 4)
      tft.drawPixel(gx, gy, 0x1820);

  tft.setTextColor(DOT_AMBER, DOT_BG);
  tft.drawString(label, x + 5, y + 5, 2);
  tft.drawFastHLine(x + 4, y + 20, w - 8, DOT_GOLD);
  tft.setTextColor(DOT_AMBER_DIM, DOT_BG);
  tft.drawRightString(unit, x + w - 5, y + 22, 1);

  // large dot matrix placeholder
  drawDotNumber(x + 12, y + 28, 0, false, 4, DOT_AMBER_DIM, DOT_BG);

  int barY = y + h - 10;
  drawLEDBar(x + 4, barY, w - 8, 6, 20, 0, DOT_AMBER, DOT_AMBER_DIM);
  tft.setTextColor(DOT_AMBER_DIM, DOT_BG);
  tft.drawString(String(minVal), x + 4, barY - 8, 1);
  tft.drawRightString(String(maxVal), x + w - 4, barY - 8, 1);
}

void plotDot(uint8_t cell, String label, float data,
             int warn, int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int w = screenWidth / 2;
    int h = screenHeight / 3;
    int x = cell1_x[cell - 1];
    int y = cell1_y[cell - 1];
    if (data < min) data = min;
    if (data > max) data = max;

    uint16_t dotColor = (data >= warn)        ? 0xF400 :
                        (data >= warn * 0.85) ? 0xFD80 : DOT_AMBER;

    tft.fillRect(x + 4, y + 26, w - 8, 30, DOT_BG);
    for (int gx = x + 4; gx < x + w - 4; gx += 4)
      for (int gy = y + 26; gy < y + 56; gy += 4)
        tft.drawPixel(gx, gy, 0x1820);

    if (data >= warn && blinkCell[cell]) {
      // blank on blink
    } else {
      drawDotNumber(x + 12, y + 28, data, digit, 4, dotColor, DOT_BG);
    }
    if (data >= warn) blinkCell[cell] = !blinkCell[cell];

    int barY   = y + h - 10;
    int filled = map(data, min, max, 0, 20);
    drawLEDBar(x + 4, barY, w - 8, 6, 20, filled, dotColor, DOT_AMBER_DIM);
    old_data[pidIndex] = data;
  }
}

// ============================================================
// initScreen - draw static frame for current layout
// ============================================================
void initScreen() {
  #ifdef SERIAL_DEBUG
  Serial.printf("Display layout -> %d\n", layout);
  #endif

  for (int i = 0; i < 7; i++) {
    pidList[i]        = pidConfig[pidInCell[layout][i]][2];
    pidReadSkip[i]    = pidConfig[pidInCell[layout][i]][6].toInt();
    pidCurrentSkip[i] = pidReadSkip[i];
    old_data[i]       = 0;
  }
  tft.fillScreen(TFT_BLACK);

  switch (layout) {

    case 0:  // arc left, numeric right
      arcMeter(1, pidInCell[0][0]);
      arcMeter(2, pidInCell[0][1]);
      arcMeter(3, pidInCell[0][2]);
      numericMeter(7, pidInCell[0][3]);
      numericMeter(8, pidInCell[0][4]);
      numericMeter(9, pidInCell[0][5]);
      break;

    case 1:  // all arc
      arcMeter(1, pidInCell[1][0]);
      arcMeter(2, pidInCell[1][1]);
      arcMeter(3, pidInCell[1][2]);
      arcMeter(7, pidInCell[1][3]);
      arcMeter(8, pidInCell[1][4]);
      arcMeter(9, pidInCell[1][5]);
      break;

    case 2:  // all numeric
      numericMeter(1, pidInCell[2][0]);
      numericMeter(2, pidInCell[2][1]);
      numericMeter(3, pidInCell[2][2]);
      numericMeter(7, pidInCell[2][3]);
      numericMeter(8, pidInCell[2][4]);
      numericMeter(9, pidInCell[2][5]);
      break;

    case 3:  // numeric left, vbar right
      numericMeter(1, pidInCell[3][0]);
      numericMeter(2, pidInCell[3][1]);
      numericMeter(3, pidInCell[3][2]);
      vBarMeter(3, pidInCell[3][3]);
      vBarMeter(4, pidInCell[3][4]);
      break;

    case 4:  // vbar outer, numeric inner
      vBarMeter(1, pidInCell[4][0]);
      numericMeter(4, pidInCell[4][1]);
      numericMeter(5, pidInCell[4][2]);
      numericMeter(6, pidInCell[4][3]);
      vBarMeter(4, pidInCell[4][4]);
      break;

    case 5:  // vbar left, numeric right
      vBarMeter(1, pidInCell[5][0]);
      vBarMeter(2, pidInCell[5][1]);
      numericMeter(7, pidInCell[5][2]);
      numericMeter(8, pidInCell[5][3]);
      numericMeter(9, pidInCell[5][4]);
      break;

    case 6:  // all vbar 4 col
      vBarMeter(1, pidInCell[6][0]);
      vBarMeter(2, pidInCell[6][1]);
      vBarMeter(3, pidInCell[6][2]);
      vBarMeter(4, pidInCell[6][3]);
      break;

    case 7:  // all vbar 4 col alt pids
      vBarMeter(1, pidInCell[7][0]);
      vBarMeter(2, pidInCell[7][1]);
      vBarMeter(3, pidInCell[7][2]);
      vBarMeter(4, pidInCell[7][3]);
      break;

    case 8:  // needle left, numeric right
      needleMeter(1, pidInCell[8][0]);
      needleMeter(2, pidInCell[8][1]);
      needleMeter(3, pidInCell[8][2]);
      numericMeter(7, pidInCell[8][3]);
      numericMeter(8, pidInCell[8][4]);
      numericMeter(9, pidInCell[8][5]);
      break;

    case 9:  // all 7seg
      seg7Meter(1, pidInCell[9][0]);
      seg7Meter(2, pidInCell[9][1]);
      seg7Meter(3, pidInCell[9][2]);
      seg7Meter(7, pidInCell[9][3]);
      seg7Meter(8, pidInCell[9][4]);
      seg7Meter(9, pidInCell[9][5]);
      break;

    case 10:  // all segbar
      segBarMeter(1, pidInCell[10][0]);
      segBarMeter(2, pidInCell[10][1]);
      segBarMeter(3, pidInCell[10][2]);
      segBarMeter(4, pidInCell[10][3]);
      break;

    case 11:  // C4 Corvette red LED bars
      c4BarMeter(1, pidInCell[11][0]);
      c4BarMeter(2, pidInCell[11][1]);
      c4BarMeter(3, pidInCell[11][2]);
      c4BarMeter(4, pidInCell[11][3]);
      break;

    case 12:  // DeLorean VFD
      vfdMeter(1, pidInCell[12][0]);
      vfdMeter(2, pidInCell[12][1]);
      vfdMeter(3, pidInCell[12][2]);
      vfdMeter(7, pidInCell[12][3]);
      vfdMeter(8, pidInCell[12][4]);
      vfdMeter(9, pidInCell[12][5]);
      break;

    case 13:  // Amber dot matrix
      dotMeter(1, pidInCell[13][0]);
      dotMeter(2, pidInCell[13][1]);
      dotMeter(3, pidInCell[13][2]);
      dotMeter(7, pidInCell[13][3]);
      dotMeter(8, pidInCell[13][4]);
      dotMeter(9, pidInCell[13][5]);
      break;

  }  // switch layout
}  // initScreen

// ============================================================
// getAB - parse ELM327 response into bytes A and B
// ============================================================
void getAB(String elm_rsp) {
  if (elm_rsp == "NO DATA\r\r>") {
    A = 0;
    B = 0;
  } else {
    uint8_t charcnt    = pidList[pidIndex].length() / 2;
    String  strs[8];
    uint8_t StringCount = 0;
    while (elm_rsp.length() > 0) {
      int index = elm_rsp.indexOf(' ');
      if (index == -1) {
        strs[StringCount++] = elm_rsp;
        A = strtol(strs[charcnt].c_str(),     NULL, 16);
        B = strtol(strs[charcnt + 1].c_str(), NULL, 16);
        break;
      } else {
        strs[StringCount++] = elm_rsp.substring(0, index);
        elm_rsp             = elm_rsp.substring(index + 1);
      }
    }
  }
}

// ============================================================
// engine_onoff - sleep when voltage drops (engine off)
// ============================================================
void engine_onoff(float data, uint8_t pid) {
  if (pidList[pid] == "0142") {
    int compare = ecu_off_volt / data;
    if (compare > 0) engine_off_count++;
    else             engine_off_count = 0;
    if (engine_off_count > 20) {
      if (pref.getUShort("layout", false) != layout)
        pref.putUShort("layout", layout);
      show_spiffs_jpeg_image("/mypic.jpg", 0, 0);
      ledcWriteTone(buzzerChannel, 3136); delay(100);
      ledcWriteTone(buzzerChannel, 2637); delay(100);
      ledcWriteTone(buzzerChannel, 2093); delay(100);
      ledcWriteTone(buzzerChannel, 0);
      for (int i = 255; i > 0; i--) { ledcWrite(backlightChannel, i); delay(10); }
      BTSerial.print("ATLP\r");
      BTSerial.disconnect();
      tft.fillScreen(TFT_BLACK);
      tft.writecommand(0x10);
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);
      esp_deep_sleep_start();
    }
  }
}

// ============================================================
// updateMeter - calculate value and route to correct plot fn
// ============================================================
void updateMeter(uint8_t pidNo, String response) {
  String  label   = pidConfig[pidInCell[layout][pidNo]][0];
  uint8_t formula = pidConfig[pidInCell[layout][pidNo]][3].toInt();
  int     mn      = pidConfig[pidInCell[layout][pidNo]][4].toInt();
  int     mx      = pidConfig[pidInCell[layout][pidNo]][5].toInt();
  bool    digit   = pidConfig[pidInCell[layout][pidNo]][7].toInt();
  int     warn    = warningValue[pidInCell[layout][pidNo]].toInt();

  getAB(response);
  float data = 0.0;
  switch (formula) {
    case 0: data = A * 0.145; break;                          // MAP psi
    case 1: data = (A - 40) * 9.0 / 5.0 + 32; break;         // temp F
    case 2: data = A * 100.0 / 255; break;                    // load %
    case 3: data = (256 * A + B) / 4.0; break;                // RPM
    case 4: data = (256 * A + B) / 1000.0; break;             // voltage
    case 5:                                                    // trans temp
      data = (256 * A + B) / 16.0;
      if (data > 150 || data < 0) data = old_data[pidIndex];
      break;
    case 6:                                                    // Ford T5 (unused on Ram)
      data = (A * 256 + B) * 5 / 72 - 18;
      if (data > 150 || data < 0) data = old_data[pidIndex];
      break;
  }

  switch (layout) {
    case 0:
      switch (pidNo) {
        case 0: plotArc(1, label, data, warn, mn, mx, digit); break;
        case 1: plotArc(2, label, data, warn, mn, mx, digit); break;
        case 2: plotArc(3, label, data, warn, mn, mx, digit); break;
        case 3: plotNumeric(7, data, warn, digit); break;
        case 4: plotNumeric(8, data, warn, digit); break;
        case 5: plotNumeric(9, data, warn, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 1:
      switch (pidNo) {
        case 0: plotArc(1, label, data, warn, mn, mx, digit); break;
        case 1: plotArc(2, label, data, warn, mn, mx, digit); break;
        case 2: plotArc(3, label, data, warn, mn, mx, digit); break;
        case 3: plotArc(7, label, data, warn, mn, mx, digit); break;
        case 4: plotArc(8, label, data, warn, mn, mx, digit); break;
        case 5: plotArc(9, label, data, warn, mn, mx, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 2:
      switch (pidNo) {
        case 0: plotNumeric(1, data, warn, digit); break;
        case 1: plotNumeric(2, data, warn, digit); break;
        case 2: plotNumeric(3, data, warn, digit); break;
        case 3: plotNumeric(7, data, warn, digit); break;
        case 4: plotNumeric(8, data, warn, digit); break;
        case 5: plotNumeric(9, data, warn, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 3:
      switch (pidNo) {
        case 0: plotNumeric(1, data, warn, digit); break;
        case 1: plotNumeric(2, data, warn, digit); break;
        case 2: plotNumeric(3, data, warn, digit); break;
        case 3: plotVBar(3, data, warn, mn, mx, digit); break;
        case 4: plotVBar(4, data, warn, mn, mx, digit); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 4:
      switch (pidNo) {
        case 0: plotVBar(1, data, warn, mn, mx, digit); break;
        case 1: plotNumeric(4, data, warn, digit); break;
        case 2: plotNumeric(5, data, warn, digit); break;
        case 3: plotNumeric(6, data, warn, digit); break;
        case 4: plotVBar(4, data, warn, mn, mx, digit); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 5:
      switch (pidNo) {
        case 0: plotVBar(1, data, warn, mn, mx, digit); break;
        case 1: plotVBar(2, data, warn, mn, mx, digit); break;
        case 2: plotNumeric(7, data, warn, digit); break;
        case 3: plotNumeric(8, data, warn, digit); break;
        case 4: plotNumeric(9, data, warn, digit); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 6:
      switch (pidNo) {
        case 0: plotVBar(1, data, warn, mn, mx, digit); break;
        case 1: plotVBar(2, data, warn, mn, mx, digit); break;
        case 2: plotVBar(3, data, warn, mn, mx, digit); break;
        case 3: plotVBar(4, data, warn, mn, mx, digit); break;
        case 4: engine_onoff(data, pidNo); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 7:
      switch (pidNo) {
        case 0: plotVBar(1, data, warn, mn, mx, digit); break;
        case 1: plotVBar(2, data, warn, mn, mx, digit); break;
        case 2: plotVBar(3, data, warn, mn, mx, digit); break;
        case 3: plotVBar(4, data, warn, mn, mx, digit); break;
        case 4: engine_onoff(data, pidNo); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 8:  // needle showcase
      switch (pidNo) {
        case 0: plotNeedle(1, label, data, warn, mn, mx, digit); break;
        case 1: plotNeedle(2, label, data, warn, mn, mx, digit); break;
        case 2: plotNeedle(3, label, data, warn, mn, mx, digit); break;
        case 3: plotNumeric(7, data, warn, digit); break;
        case 4: plotNumeric(8, data, warn, digit); break;
        case 5: plotNumeric(9, data, warn, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 9:  // 7seg showcase
      switch (pidNo) {
        case 0: plotSeg7(1, data, warn, digit); break;
        case 1: plotSeg7(2, data, warn, digit); break;
        case 2: plotSeg7(3, data, warn, digit); break;
        case 3: plotSeg7(7, data, warn, digit); break;
        case 4: plotSeg7(8, data, warn, digit); break;
        case 5: plotSeg7(9, data, warn, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 10:  // segbar showcase
      switch (pidNo) {
        case 0: plotSegBar(1, data, warn, mn, mx, digit); break;
        case 1: plotSegBar(2, data, warn, mn, mx, digit); break;
        case 2: plotSegBar(3, data, warn, mn, mx, digit); break;
        case 3: plotSegBar(4, data, warn, mn, mx, digit); break;
        case 4: engine_onoff(data, pidNo); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 11:  // C4 Corvette red LED
      switch (pidNo) {
        case 0: plotC4Bar(1, data, warn, mn, mx, digit); break;
        case 1: plotC4Bar(2, data, warn, mn, mx, digit); break;
        case 2: plotC4Bar(3, data, warn, mn, mx, digit); break;
        case 3: plotC4Bar(4, data, warn, mn, mx, digit); break;
        case 4: engine_onoff(data, pidNo); break;
        case 5: engine_onoff(data, pidNo); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 12:  // DeLorean VFD
      switch (pidNo) {
        case 0: plotVFD(1, label, data, warn, mn, mx, digit); break;
        case 1: plotVFD(2, label, data, warn, mn, mx, digit); break;
        case 2: plotVFD(3, label, data, warn, mn, mx, digit); break;
        case 3: plotVFD(7, label, data, warn, mn, mx, digit); break;
        case 4: plotVFD(8, label, data, warn, mn, mx, digit); break;
        case 5: plotVFD(9, label, data, warn, mn, mx, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

    case 13:  // Amber dot matrix
      switch (pidNo) {
        case 0: plotDot(1, label, data, warn, mn, mx, digit); break;
        case 1: plotDot(2, label, data, warn, mn, mx, digit); break;
        case 2: plotDot(3, label, data, warn, mn, mx, digit); break;
        case 3: plotDot(7, label, data, warn, mn, mx, digit); break;
        case 4: plotDot(8, label, data, warn, mn, mx, digit); break;
        case 5: plotDot(9, label, data, warn, mn, mx, digit); break;
        case 6: engine_onoff(data, pidNo); break;
      }
      break;

  }  // switch layout
  bt_message = "";
}  // updateMeter

// ============================================================
// autoDim - smooth phone-style backlight control
// ============================================================
void autoDim() {
  static unsigned long lastUpdate       = 0;
  const  unsigned long interval         = 80;
  if (millis() - lastUpdate < interval) return;
  lastUpdate = millis();

  static float smoothedLight    = 0;
  static int   currentBrightness = 255;
  static int   targetBrightness  = 255;

  const float alpha       = 0.08;
  const int   lightBuffer = 60;

  int rawLight    = analogRead(LDR_PIN);
  smoothedLight   = alpha * rawLight + (1.0 - alpha) * smoothedLight;
  int light       = (int)smoothedLight;

  const int BRIGHT_LIGHT = 200;
  const int DARK_LIGHT   = 2000;

  int newTarget = map(light, BRIGHT_LIGHT, DARK_LIGHT, 255, 10);
  newTarget     = constrain(newTarget, 10, 255);

  if (abs(newTarget - targetBrightness) > lightBuffer)
    targetBrightness = newTarget;

  if (currentBrightness < targetBrightness) currentBrightness += 2;
  if (currentBrightness > targetBrightness) currentBrightness -= 2;

  ledcWrite(backlightChannel, currentBrightness);

  #ifdef SERIAL_DEBUG
  Serial.printf("LDR:%d Smooth:%d Target:%d Bright:%d\n",
                rawLight, light, targetBrightness, currentBrightness);
  #endif
}
