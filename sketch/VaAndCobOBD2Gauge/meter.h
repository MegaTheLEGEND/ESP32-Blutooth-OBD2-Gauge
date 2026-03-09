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


// graph globals - must be before updateMeter
float    graphBuffer[318] = {};
uint16_t graphHead        = 0;
bool     graphActive      = false;
uint8_t  graphPid         = 0;

void initScreen();  // forward declaration
void graphUpdate(uint8_t pid, float newData); 
void graphExit();
void graphInit(uint8_t pid);
String sendOBDCommand(String cmd);

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



// ── graph defines ─────────────────────────────────────────
#define GRAPH_POINTS  318
#define GRAPH_BG      0x0000   // true black
#define GRAPH_LINE    0xFFFF   // white line
#define GRAPH_FILL    0x2104   // very dark grey fill under line
#define GRAPH_WARN    0xFD20   // amber warning
#define GRAPH_GRID    0x0841   // barely visible dark grey grid
#define GRAPH_LABEL   0xFFFF   // white
#define GRAPH_DIM     0x4A49   // grey for secondary text

void graphInit(uint8_t pid) {
  graphPid    = pid;
  graphActive = true;
  graphHead   = 0;
  float seed  = old_data[pid];
  for (int i = 0; i < GRAPH_POINTS; i++) graphBuffer[i] = seed;

  uint8_t pidIdx  = layoutDefs[layout].cells[pid].pid;
  String  label   = pidConfig[pidIdx][0];
  String  unit    = pidConfig[pidIdx][1];
  int     mn      = pidConfig[pidIdx][4].toInt();
  int     mx      = pidConfig[pidIdx][5].toInt();
  int     warnVal = warningValue[pidIdx].toInt();

  tft.fillScreen(GRAPH_BG);

  // top label — large, left aligned
  tft.setTextColor(GRAPH_LABEL, GRAPH_BG);
  tft.drawString(label, 4, 3, 2);

  // unit — small, right of label, dimmed
  tft.setTextColor(GRAPH_DIM, GRAPH_BG);
  tft.drawRightString(unit, 316, 5, 1);

  // thin white separator line under header
  tft.drawFastHLine(0, 18, 320, 0x2104);

  // thin white separator line above footer
  tft.drawFastHLine(0, 226, 320, 0x2104);

  // footer — min left, exit centre, max right
  tft.setTextColor(GRAPH_DIM, GRAPH_BG);
  tft.drawString(String(mn), 4, 229, 1);
  tft.drawCentreString("tap to exit", 160, 229, 1);
  tft.drawRightString(String(mx), 316, 229, 1);

  // horizontal grid lines — 3 subtle dotted lines
  for (int i = 1; i < 4; i++) {
    int gy = 19 + (206 * i / 4);
    for (int x = 0; x < 320; x += 4)
      tft.drawPixel(x, gy, GRAPH_GRID);
    // scale value
    int labelVal = mx - ((mx - mn) * i / 4);
    tft.setTextColor(GRAPH_GRID, GRAPH_BG);
    tft.drawString(String(labelVal), 4, gy - 7, 1);
  }

  // warn line — solid amber, thin
  int warnY = map(warnVal, mn, mx, 225, 20);
  warnY     = constrain(warnY, 20, 225);
  tft.drawFastHLine(0, warnY, 320, GRAPH_WARN);
  tft.setTextColor(GRAPH_WARN, GRAPH_BG);
  tft.drawRightString("!", 316, warnY - 8, 1);
}

void graphUpdate(uint8_t pid, float newData) {
  if (!graphActive || pid != graphPid) return;

  uint8_t pidIdx  = layoutDefs[layout].cells[pid].pid;
  int     mn      = pidConfig[pidIdx][4].toInt();
  int     mx      = pidConfig[pidIdx][5].toInt();
  int     warnVal = warningValue[pidIdx].toInt();
  bool    digit   = pidConfig[pidIdx][7].toInt();

  graphBuffer[graphHead] = newData;
  graphHead = (graphHead + 1) % GRAPH_POINTS;

  for (int x = 0; x < GRAPH_POINTS; x++) {
    int   idx = (graphHead + x) % GRAPH_POINTS;
    float val = graphBuffer[idx];
    int   y   = map((int)val, mn, mx, 225, 20);
    y         = constrain(y, 20, 225);

    // erase column
    tft.drawFastVLine(x, 20, 206, GRAPH_BG);

    // redraw dotted grid
    for (int i = 1; i < 4; i++) {
      int gy = 19 + (206 * i / 4);
      if (x % 4 == 0) tft.drawPixel(x, gy, GRAPH_GRID);
    }

    bool aboveWarn = (val >= warnVal);
    uint16_t lineCol = aboveWarn ? GRAPH_WARN : GRAPH_LINE;

    // solid fill under line — single pixel wide columns, fades via color
    uint16_t fillCol = aboveWarn ? 0x2000 : GRAPH_FILL;
    tft.drawFastVLine(x, y + 1, 225 - y, fillCol);

    // connect to previous point — 2px thick
    if (x > 0) {
      int   prevIdx = (graphHead + x - 1) % GRAPH_POINTS;
      float prevVal = graphBuffer[prevIdx];
      int   prevY   = map((int)prevVal, mn, mx, 225, 20);
      prevY         = constrain(prevY, 20, 225);
      uint16_t lc   = ((val >= warnVal) || (prevVal >= warnVal)) ? GRAPH_WARN : GRAPH_LINE;
      tft.drawLine(x - 1, prevY,     x, y,     lc);
      tft.drawLine(x - 1, prevY - 1, x, y - 1, lc);
    }

    // bright point
    tft.drawPixel(x, y,     lineCol);
    tft.drawPixel(x, y - 1, lineCol);
  }

  // redraw warn line on top
  int warnY = map(warnVal, mn, mx, 225, 20);
  warnY     = constrain(warnY, 20, 225);
  tft.drawFastHLine(0, warnY, 320, GRAPH_WARN);

  // live value — top right, large
  tft.fillRect(180, 0, 136, 17, GRAPH_BG);
  uint16_t valColor = (newData >= warnVal) ? GRAPH_WARN : GRAPH_LABEL;
  tft.setTextColor(valColor, GRAPH_BG);
  String result = String(newData, digit ? 1 : 0);
  tft.drawRightString(result.c_str(), 316, 2, 2);
}

void graphExit() {
  graphActive = false;
  initScreen();
}

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
  int    cy      = y + h + 18;  // pivot below cell so arc sweeps up into view

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
    int      ro    = major ? 60 : 58;
    int      ri    = major ? 48 : 53;
    float    tv    = minVal + (maxVal - minVal) * i / 18.0;
    uint16_t tc    = (tv >= warnVal) ? 0xF800 : TFT_WHITE;
    tft.drawLine(cx + ro * cos(arad), cy + ro * sin(arad),
                 cx + ri * cos(arad), cy + ri * sin(arad), tc);
    if (major) {
      int lx = cx + 38 * cos(arad);
      int ly = cy + 38 * sin(arad);
      if (ly > y + 28 && ly < y + h - 2) {
        tft.setTextColor(tc, TFT_BLACK);
        tft.drawCentreString(String((int)tv), lx, ly - 4, 1);
      }
    }
  }

  tft.drawSmoothArc(cx, cy, 62, 56, warnAngle, 226, 0xF800, TFT_BLACK, false);
  tft.fillCircle(cx, cy, 6, 0x4208);
  tft.fillCircle(cx, cy, 4, TFT_WHITE);
  tft.fillCircle(cx, cy, 2, 0x4208);
  drawNeedle(cx, cy, 135, 54, TFT_WHITE);
}

void plotNeedle(uint8_t cell, String label, float data, int warn,
                int min, int max, bool digit) {
  if (data != old_data[pidIndex]) {
    int w  = screenWidth / 2;
    int h  = screenHeight / 3;
    int x  = cell1_x[cell - 1];
    int y  = cell1_y[cell - 1];
    int cx = x + w / 2;
    int cy = y + h + 45;  // must match needleMeter
    if (data < min) data = min;
    if (data > max) data = max;

    int oldAngle = map((int)old_data[pidIndex], min, max, 135, 225);
    drawNeedle(cx, cy, oldAngle, 54, TFT_BLACK);

    for (int i = 0; i <= 18; i++) {
      float adeg = 135.0 + i * 5.0;
      if (abs(adeg - oldAngle) < 8) {
        float    arad  = adeg * 3.14159 / 180.0;
        bool     major = (i % 3 == 0);
        int      ro    = major ? 60 : 58;
        int      ri    = major ? 48 : 53;
        float    tv    = min + (max - min) * i / 18.0;
        uint16_t tc    = (tv >= warn) ? 0xF800 : TFT_WHITE;
        tft.drawLine(cx + ro * cos(arad), cy + ro * sin(arad),
                     cx + ri * cos(arad), cy + ri * sin(arad), tc);
        if (major) {
          int lx = cx + 38 * cos(arad);
          int ly = cy + 38 * sin(arad);
          if (ly > y + 28 && ly < y + h - 2) {
            tft.setTextColor(tc, TFT_BLACK);
            tft.drawCentreString(String((int)tv), lx, ly - 4, 1);
          }
        }
      }
    }

    int      newAngle    = map((int)data, min, max, 135, 225);
    uint16_t needleColor = (data >= warn) ? 0xF800 : TFT_WHITE;
    drawNeedle(cx, cy, newAngle, 54, needleColor);
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
// CELL ADDRESS HELPERS
// ============================================================
void getCellAddress(uint8_t grid, uint8_t slot, int &x, int &y, int &w, int &h) {
  if (grid == GRID_6CELL) {
    uint8_t col = slot / 3;
    uint8_t row = slot % 3;
    x = col * 160;  y = row * 80;  w = 160;  h = 80;
  } else if (grid == GRID_MIXED) {
    if (slot < 3) {
      // left 160px numeric column
      x = 0;  y = slot * 80;  w = 160;  h = 80;
    } else {
      // right two 80px vbar columns
      x = 160 + (slot - 3) * 80;  y = 0;  w = 80;  h = 240;
    }
  } else {
    x = slot * 80;  y = 0;  w = 80;  h = 240;
  }
}
// get the cell number (1-9) that the TFT draw functions expect
uint8_t getCellNum(uint8_t grid, uint8_t slot) {
  if (grid == GRID_6CELL) {
    const uint8_t cellMap[6] = {1, 2, 3, 7, 8, 9};
    return cellMap[slot];
  } else if (grid == GRID_MIXED) {
    // slot 0-2 = left 160px col (cells 1,2,3), slot 3-4 = right vbar cols (cells 3,4)
    const uint8_t cellMap[5] = {1, 2, 3, 3, 4};
    return cellMap[slot];
  } else {
    return slot + 1;
  }
}


// ============================================================
// UNIFIED initScreen
// ============================================================
void initScreen() {
  #ifdef SERIAL_DEBUG
  Serial.printf("Layout -> %d (%s)\n", layout, layoutDefs[layout].name);
  #endif

  const LayoutDef &ld = layoutDefs[layout];

  // rebuild pid lists from layout descriptor
  // slot 0..cellCount-1 = active cells
  // enginePid fills remaining slots up to maxpidIndex
  uint8_t slotsFilled = 0;
  for (uint8_t i = 0; i < ld.cellCount; i++) {
    if (ld.cells[i].pid == 255) continue;  // empty slot
    pidList[slotsFilled]        = pidConfig[ld.cells[i].pid][2];
    pidReadSkip[slotsFilled]    = pidConfig[ld.cells[i].pid][6].toInt();
    pidCurrentSkip[slotsFilled] = pidReadSkip[slotsFilled];
    slotsFilled++;
  }
  // fill remaining with engine pid (0142) for engine_onoff
  for (uint8_t i = slotsFilled; i < maxpidIndex; i++) {
    pidList[i]        = pidConfig[ld.enginePid][2];
    pidReadSkip[i]    = 0;
    pidCurrentSkip[i] = 0;
  }
  for (uint8_t i = 0; i < maxpidIndex; i++) old_data[i] = 0;

  tft.fillScreen(TFT_BLACK);

  // draw each cell
  for (uint8_t slot = 0; slot < ld.cellCount; slot++) {
    if (ld.cells[slot].pid == 255) continue;
    uint8_t cellNum = getCellNum(ld.grid, slot);
    uint8_t pid     = ld.cells[slot].pid;
    uint8_t gt      = ld.cells[slot].gaugeType;
    switch (gt) {
      case GT_ARC:     arcMeter(cellNum,    pid); break;
      case GT_NUMERIC: numericMeter(cellNum, pid); break;
      case GT_VBAR:    vBarMeter(cellNum,   pid); break;
      case GT_SEG7:    seg7Meter(cellNum,   pid); break;
      case GT_SEGBAR:  segBarMeter(cellNum, pid); break;
      case GT_NEEDLE:  needleMeter(cellNum, pid); break;
      case GT_C4BAR:   c4BarMeter(cellNum,  pid); break;
      case GT_VFD:     vfdMeter(cellNum,    pid); break;
      case GT_DOT:     dotMeter(cellNum,    pid); break;
    }
  }
  if (showsystem) {
    // show layout name briefly in top-right corner
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawRightString(layoutDefs[layout].name, 318, 2, 1);
  }
}






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
// UNIFIED updateMeter
// ============================================================
void updateMeter(uint8_t pidNo, String response) {
  if (graphActive) return;
  const LayoutDef &ld = layoutDefs[layout];

  // graph mode intercept
  if (graphActive && pidNo == graphPid) {
    uint8_t  formula = pidConfig[ld.cells[pidNo].pid][3].toInt();
    getAB(response);
    float data = 0.0;
    switch (formula) {
      case 0: data = A * 0.145; break;
      case 1: data = (A - 40) * 9.0 / 5.0 + 32; break;
      case 2: data = A * 100.0 / 255; break;
      case 3: data = (256 * A + B) / 4.0; break;
      case 4: data = (256 * A + B) / 1000.0; break;
      case 5: data = (256*A+B)/16.0; if (data>150||data<0) data=old_data[pidIndex]; break;
      case 6: data = (A*256+B)*5/72-18; if (data>150||data<0) data=old_data[pidIndex]; break;
    }
    graphUpdate(pidNo, data);
    bt_message = "";
    return;
  }

  // bounds check - if pidNo is beyond active cells it's an engine_onoff slot
  if (pidNo >= ld.cellCount || ld.cells[pidNo].pid == 255) {
    getAB(response);
    float data = (256 * A + B) / 1000.0;  // voltage formula
    engine_onoff(data, pidNo);
    bt_message = "";
    return;
  }

  uint8_t  pid     = ld.cells[pidNo].pid;
  uint8_t  gt      = ld.cells[pidNo].gaugeType;
  String   label   = pidConfig[pid][0];
  uint8_t  formula = pidConfig[pid][3].toInt();
  int      mn      = pidConfig[pid][4].toInt();
  int      mx      = pidConfig[pid][5].toInt();
  bool     digit   = pidConfig[pid][7].toInt();
  int      warn    = warningValue[pid].toInt();
  uint8_t  cellNum = getCellNum(ld.grid, pidNo);

  float data = 0.0;

  // odometer (formula 13) uses 4-byte parse on the response already passed in
  if (formula == 13) {
    float raw_km = getABCD(response) / 10.0;
    data = raw_km;  // case 13 in switch converts km → miles
  } else {
    getAB(response);
  }

  switch (formula) {
    case 0:  data = A * 0.145; break;                                                          // raw byte to psi (MAP/baro)
    case 1:  data = (A - 40) * 9.0 / 5.0 + 32; break;                                         // celsius offset to fahrenheit
    case 2:  data = A * 100.0 / 255; break;                                                    // byte to percentage
    case 3:  data = (256 * A + B) / 4.0; break;                                                // two-byte RPM
    case 4:  data = (256 * A + B) / 1000.0; break;                                             // two-byte voltage or scaled load
    case 5:  data = (256*A+B)/16.0; if (data>150||data<0) data=old_data[pidIndex]; break;      // two-byte temp with bounds
    case 6:  data = (A*256+B)*5/72-18; if (data>150||data<0) data=old_data[pidIndex]; break;   // Ford-specific (unused)
    case 7:  data = (A / 1.28) - 100; break;                                                   // signed fuel trim percent
    case 8:  data = (256 * A + B) / 100.0; break;                                              // MAF grams/sec
    case 9:  data = A / 200.0; break;                                                          // O2 sensor voltage (0-1.275v)
    case 10: data = (A / 2.0) - 64; break;                                                     // ignition timing degrees BTDC
    case 11: data = 3.0 * A * 0.14504; break;                                                  // fuel pressure kPa → psi
    case 12: data = ((256.0 * A + B) / 20.0) * 0.264172; break;                               // fuel rate L/h → gal/h
    case 13: data = data * 0.621371; break;                                                    // odometer km → miles (pre-set above)
    case 14: data = 256.0 * A + B; break;                                                      // run time seconds
  }

  switch (gt) {
    case GT_ARC:     plotArc(cellNum, label, data, warn, mn, mx, digit); break;
    case GT_NUMERIC: plotNumeric(cellNum, data, warn, digit); break;
    case GT_VBAR:    plotVBar(cellNum, data, warn, mn, mx, digit); break;
    case GT_SEG7:    plotSeg7(cellNum, data, warn, digit); break;
    case GT_SEGBAR:  plotSegBar(cellNum, data, warn, mn, mx, digit); break;
    case GT_NEEDLE:  plotNeedle(cellNum, label, data, warn, mn, mx, digit); break;
    case GT_C4BAR:   plotC4Bar(cellNum, data, warn, mn, mx, digit); break;
    case GT_VFD:     plotVFD(cellNum, label, data, warn, mn, mx, digit); break;
    case GT_DOT:     plotDot(cellNum, label, data, warn, mn, mx, digit); break;
  }

  // check engine off on voltage pid
  if (pidConfig[pid][2] == "0142") engine_onoff(data, pidNo);

  bt_message = "";
}

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