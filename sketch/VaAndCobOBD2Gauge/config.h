/* "config.h"
this header file is for configuration menu
*/
//freeRTOS multi tasking
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "music.h"           // songs data
#include "nes_audio.h"       // nes audio player
#include "firmware_update.h" // firmware updater function
#include "elm327.h"          // read VIN function

#define mypic_filename "/mypic.jpg"
#define FORMAT_SPIFFS_IF_FAILED true

Cartridge player(BUZZER_PIN);

// ============================================================
// MENU
// ============================================================
const String menuList[8] = {
  "Show pid/s & CPU Temp",
  "Load my pic 320x240 px",
  "Update firmware",
  "Warning setting",
  "DTCs",
  "Auto-off",
  "About",
  "Exit"
};
const uint8_t maxMenu = array_length(menuList);

// ============================================================
// WARNING CONFIG - fully dynamic, built from pidConfig
// index 0      = CPU temp offset
// index 1..N   = warningValue[i-1] for each pid
// ============================================================
#define WARN_ROWS     7
#define WARN_ROW_H   24
#define WARN_Y_START 52
#define WARN_TOTAL   (maxpidIndex + 1)

String getWarnLabel(uint8_t i) {
  if (i == 0) return "CPU Temp Offset";
  return pidConfig[i - 1][0];
}

String getWarnUnit(uint8_t i) {
  if (i == 0) return "`F";
  return pidConfig[i - 1][1];
}

String getWarnValue(uint8_t i) {
  if (i == 0) return String(tempOffset);
  return warningValue[i - 1];
}

int getWarnStep(uint8_t i) {
  if (i == 0) return 1;
  int range = pidConfig[i-1][5].toInt() - pidConfig[i-1][4].toInt();
  if (range > 2000) return 500;
  if (range > 200)  return 10;
  if (range > 50)   return 5;
  return 1;
}

void applyWarnChange(uint8_t i, int8_t dir) {
  int step = getWarnStep(i);
  if (i == 0) {
    tempOffset += dir * step;
    if (tempOffset < -99) tempOffset = -99;
    if (tempOffset >   0) tempOffset =   0;
  } else {
    int mn  = pidConfig[i-1][4].toInt();
    int mx  = pidConfig[i-1][5].toInt();
    int val = warningValue[i-1].toInt() + dir * step;
    if (val < mn) val = mn;
    if (val > mx) val = mx;
    warningValue[i-1] = String(val);
  }
}

void drawWarnList(uint8_t selected, uint8_t scrollTop) {
  tft.fillRect(0, WARN_Y_START, 314, WARN_ROWS * WARN_ROW_H, TFT_BLACK);

  for (uint8_t row = 0; row < WARN_ROWS; row++) {
    uint8_t idx = scrollTop + row;
    if (idx >= WARN_TOTAL) break;
    int     y   = WARN_Y_START + row * WARN_ROW_H;
    bool    sel = (idx == selected);

    uint16_t rowBg = sel ? 0x1a3f : TFT_BLACK;
    tft.fillRect(0, y, 314, WARN_ROW_H - 2, rowBg);

    tft.setTextColor(0x4208, rowBg);
    tft.drawString(String(idx), 2, y + 5, 1);

    tft.setTextColor(sel ? TFT_YELLOW : TFT_WHITE, rowBg);
    tft.drawString(getWarnLabel(idx), 18, y + 5, 2);

    tft.setTextColor(sel ? 0xFD20 : TFT_GREEN, rowBg);
    tft.drawRightString(getWarnUnit(idx), 313, y + 5, 2);
    String val = getWarnValue(idx);
    while (val.length() < 5) val = " " + val;
    tft.drawRightString(val, 280, y + 5, 2);
  }

  // scrollbar
  tft.fillRect(315, WARN_Y_START, 5, WARN_ROWS * WARN_ROW_H, 0x2104);
  int totalH = WARN_ROWS * WARN_ROW_H;
  int thumbH = max((int)(totalH * WARN_ROWS / WARN_TOTAL), 8);
  int thumbY = WARN_Y_START + (scrollTop * (totalH - thumbH)) / max((int)(WARN_TOTAL - WARN_ROWS), 1);
  tft.fillRect(315, thumbY, 5, thumbH, TFT_ORANGE);

  tft.setTextColor(0x8410, TFT_BLACK);
  if (scrollTop > 0)
    tft.drawCentreString("^", 308, WARN_Y_START, 1);
  if ((int)scrollTop + WARN_ROWS < (int)WARN_TOTAL)
    tft.drawCentreString("v", 308, WARN_Y_START + WARN_ROWS * WARN_ROW_H - 8, 1);
}

void drawWarnHeader() {
  tft.fillRect(0, 0, 320, WARN_Y_START, TFT_BLACK);
  tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
  tft.setTextColor(TFT_BLACK);
  tft.drawCentreString("Warning Settings", 159, 4, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Tap row to select   Btn=save & exit", 0, 28, 1);
}

void drawWarnFooter() {
  tft.fillRect(0, 220, 320, 20, TFT_BLACK);
  tft.fillRoundRect(0,   220, 70, 20, 4, 0x3000);
  tft.setTextColor(TFT_RED, 0x3000);
  tft.drawCentreString("-  decrease", 35, 222, 2);
  tft.fillRoundRect(250, 220, 70, 20, 4, 0x0240);
  tft.setTextColor(TFT_GREEN, 0x0240);
  tft.drawCentreString("increase  +", 285, 222, 2);
  tft.fillRoundRect(100, 220, 120, 20, 4, 0x1082);
  tft.setTextColor(TFT_WHITE, 0x1082);
  tft.drawCentreString("RESET DEFAULT", 160, 222, 2);
}

// ============================================================
// SPRITES for animation
// ============================================================
TFT_eSprite car   = TFT_eSprite(&tft);
TFT_eSprite bk    = TFT_eSprite(&tft);
TFT_eSprite pole  = TFT_eSprite(&tft);
TFT_eSprite fence = TFT_eSprite(&tft);

// ============================================================
// NES Audio - play on Core 0
// ============================================================
TaskHandle_t TaskHandle0 = NULL;

void TaskPlayMusic(void *pvParameters) {
  esp_task_wdt_init(62, false);
  while (1) {
    player.play_nes(song, true, 0.25);
  }
}

// ============================================================
// ABOUT PAGE ANIMATION
// ============================================================
void animation() {
#define BK_HEIGHT 80
#define BK_WIDTH  235
  uint8_t  cur_x     = 68;
  uint8_t  cur_y     = 10;
  uint8_t  dash_x    = 20;
  int16_t  tag_x     = 235;
  int16_t  fence_x   = 0;
  uint8_t  ani_speed = 2;

  int           scrollOffset   = 0;
  unsigned long scrollLastTick = 0;
  const char*   scrollStr      = "  github.com/MegaTheLEGEND/ESP32-Blutooth-OBD2-Gauge  ";

  bk.createSprite(BK_WIDTH, BK_HEIGHT);
  car.createSprite(CAR_WIDTH, CAR_HEIGHT);
  car.setSwapBytes(true);
  car.pushImage(0, 0, CAR_WIDTH, CAR_HEIGHT, futurecar);
  pole.createSprite(POLE_WIDTH, POLE_HEIGHT);
  pole.setSwapBytes(true);
  pole.pushImage(0, 0, POLE_WIDTH, POLE_HEIGHT, pole1);
  fence.createSprite(FENCE_WIDTH, FENCE_HEIGHT);
  fence.setSwapBytes(true);
  fence.pushImage(0, 0, FENCE_WIDTH, FENCE_HEIGHT, fence1);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(">MAP/ENG LOAD/ECT/EOT/TFT/ENG SPD/PCM Volt", 0, 40, 2);
  tft.drawString(">Warning, Automatic Dim/OnOff/Overheat Shutdown", 0, 60, 2);
  tft.drawString(">Read DTC Code & Clear MIL Status", 0, 80, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("   ---  [ Press button to exit ]  ---", 0, 140, 2);
  tft.setTextColor(TFT_CYAN);
  String txt = "[ " + serial_no + " ] BUILD : " + compile_date;
  tft.drawString(txt, 0, 0, 2);
  getPID("ATE0");
  txt = "VIN :  " + getVIN(getPID("0902"));
  tft.setTextColor(TFT_GREENYELLOW);
  tft.drawString(txt, 0, 20, 2);

  bk.setTextColor(TFT_BLACK, TFT_ORANGE);
  int strW = tft.textWidth(scrollStr, 2);

  while (digitalRead(SELECTOR_PIN) == HIGH) {
    bk.fillSprite(0x4228);
    bk.fillRect(0, 0, 235, 16, TFT_LIGHTGREY);
    bk.fillRect(0, 16, 235, 3, 0x8430);
    for (int16_t i = 0; i < 5; i++)
      bk.drawFastHLine((dash_x - 20) * 1.5 + (60 * i), 45, 15, TFT_WHITE);
    dash_x -= ani_speed;
    if (dash_x <= 0) dash_x = 40;
    tag_x -= ani_speed;
    if (tag_x <= -80) tag_x = 235;
    bk.drawString("< FOOD >", tag_x, 0, 2);
    int8_t move = random(-1, 2);
    cur_x += move;
    move   = random(-1, 2);
    cur_y += move;
    if (cur_x > 235 - CAR_WIDTH)  cur_x = 235 - CAR_WIDTH;
    if (cur_x < 1)                 cur_x = 1;
    if (cur_y < 1)                 cur_y = 1;
    if (cur_y > 79 - CAR_HEIGHT)   cur_y = 79 - CAR_HEIGHT;
    car.pushToSprite(&bk, cur_x, cur_y, TFT_BLACK);
    for (int i = 0; i < 8; i++)
      fence.pushToSprite(&bk, fence_x * 2 + (FENCE_WIDTH * i), 80 - FENCE_HEIGHT, TFT_BLACK);
    fence_x -= ani_speed;
    if (fence_x <= -FENCE_WIDTH + ani_speed) fence_x = 0;
    pole.pushToSprite(&bk, tag_x * 2, 80 - POLE_HEIGHT, TFT_BLACK);
    bk.pushSprite(85, 159);

    if (millis() - scrollLastTick >= 30) {
      scrollLastTick = millis();
      tft.fillRect(0, 120, 320, 18, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString(scrollStr, -scrollOffset, 120, 2);
      scrollOffset += 2;
      if (scrollOffset > strW) scrollOffset = 0;
    }

    digitalWrite(LED_RED_PIN,   random(0, 2));
    digitalWrite(LED_GREEN_PIN, random(0, 2));
    digitalWrite(LED_BLUE_PIN,  random(0, 2));
    checkCPUTemp();
    autoDim();
  }

  car.deleteSprite();
  bk.deleteSprite();
  pole.deleteSprite();
  fence.deleteSprite();
}

// ============================================================
// LOAD MY PIC
// ============================================================
void loadMyPic() {
  if (!SD.begin()) {
    tft.pushImage(129, 44, 60, 60, sdcard);
    Serial.println(F("Micro SD Card not mounted!"));
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawCentreString("Please insert Micro SD Card", 159, 120, 4);
  } else {
    Serial.println(F("Micro SD Card..OK"));
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
      Serial.println(F("Micro SD Card Error!"));
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawCentreString("Micro SD Card Error!", 159, 120, 4);
    } else {
      if (!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)) {
        Serial.println(F("Error: SPIFFS failed!"));
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawCentreString("Error: SPIFFS failed!", 159, 120, 4);
      } else {
        tft.setTextColor(TFT_BLACK, TFT_GREEN);
        tft.drawCentreString("Copying...\"mypic.jpg\"", 159, 120, 4);
        Serial.println(F("Deleting...\"mypic.jpg\""));
        SPIFFS.remove(mypic_filename);
        File sourceFile = SD.open(mypic_filename);
        if (sourceFile) {
          File destFile = SPIFFS.open(mypic_filename, FILE_WRITE);
          Serial.println(F("Copying...\"mypic.jpg\""));
          tft.setTextColor(TFT_BLACK, TFT_GREEN);
          static uint8_t buf[512];
          while (sourceFile.read(buf, 512)) destFile.write(buf, 512);
          sourceFile.close();
          destFile.close();
          show_spiffs_jpeg_image(mypic_filename, 0, 0);
        } else {
          Serial.println(F("\"mypic.jpg\" not found!"));
          tft.pushImage(130, 44, 60, 60, nofile);
          tft.setTextColor(TFT_WHITE, TFT_RED);
          tft.drawCentreString("\"mypic.jpg\" not found!", 159, 120, 4);
        }
      }
    }
  }
  SD.end();
  SPIFFS.end();
  beep();
}

// ============================================================
// MENU LIST RENDERER
// ============================================================
void listMenu(uint8_t choice) {
  if (showsystem) tft.pushImage(0, 35, 25, 25, switchon);
  else            tft.pushImage(0, 35, 25, 25, switchoff);
  tft.pushImage(0,   65,  25, 25, image);
  tft.pushImage(0,   95,  25, 25, cpu);
  tft.pushImage(0,   125, 25, 25, warning);
  tft.pushImage(0,   155, 25, 25, engine);
  tft.pushImage(159, 155, 25, 25, autooff);
  tft.pushImage(0,   185, 25, 25, about);
  tft.pushImage(159, 185, 25, 25, quit);

  for (uint8_t i = 0; i < maxMenu; i++) {
    if (choice == i) tft.setTextColor(TFT_WHITE, TFT_BLUE);
    else             tft.setTextColor(TFT_WHITE, TFT_BLACK);
    if      (i < 4) tft.drawString(menuList[i], 32,           35 + 30 * i, 4);
    else if (i < 6) tft.drawString(menuList[i], 32+(i-4)*159, 35 + 30 * 4, 4);
    else            tft.drawString(menuList[i], 32+(i-6)*159, 35 + 30 * 5, 4);
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Touch top/bottom to navigate, center to select", 0, 211, 2);
  tft.drawString("Or: Press=Next  Hold=Select", 0, 227, 2);
}

// ============================================================
// MAIN CONFIG MENU
// ============================================================
void configMenu() {
  uint8_t select      = 0;
  bool    pressed     = false;
  long    holdtimer   = 0;
  bool    currentFlag = showsystem;

  auto readTouchNav = [&]() -> int8_t {
    static unsigned long lastTouch = 0;
    const  unsigned long debounce  = 350;
    uint16_t tx, ty;
    if (!getTouch(&tx, &ty))             return 0;
    if (millis() - lastTouch < debounce) return 0;
    lastTouch = millis();
    clickSound();
    if (ty < (_height / 3))       return -1;
    if (ty > ((_height / 3) * 2)) return  1;
    return 2;
  };

  beep();
  tft.fillScreen(TFT_BLACK);
  tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
  tft.setTextColor(TFT_BLACK);
  tft.drawCentreString("[---   Configuration Menu   ---]", 159, 0, 4);
  listMenu(select);
  delay(1000);

  while (true) {

    if (digitalRead(SELECTOR_PIN) == LOW) {
      if (!pressed) {
        pressed   = true;
        holdtimer = millis();
      } else if (millis() - holdtimer > 3000) {
        goto doSelect;
      }
      delay(250);
    } else {
      if (pressed) {
        select++;
        if (select == maxMenu) select = 0;
        listMenu(select);
        clickSound();
        pressed = false;
        delay(200);
      }
    }

    {
      int8_t t = readTouchNav();
      if (t == -1) {
        select = (select == 0) ? maxMenu - 1 : select - 1;
        listMenu(select);
      } else if (t == 1) {
        select++;
        if (select == maxMenu) select = 0;
        listMenu(select);
      } else if (t == 2) {
        goto doSelect;
      }
    }

    checkCPUTemp();
    autoDim();
    continue;

    doSelect:
    pressed = false;
    prompt  = true;

    // -------- TOGGLE SHOW SYSTEM --------
    if (select == 0) {
      beepbeep();
      showsystem = !showsystem;
      if (showsystem) tft.pushImage(0, 35, 25, 25, switchon);
      else            tft.pushImage(0, 35, 25, 25, switchoff);
    }

    // -------- LOAD MY PIC --------
    if (select == 1) {
      clickSound();
      tft.fillRect(0, 30, 320, 239, TFT_BLACK);
      loadMyPic();
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.drawCentreString("[- Press button to exit -]", 159, 215, 4);
      while (digitalRead(SELECTOR_PIN) == HIGH) {
        checkCPUTemp();
        autoDim();
      }
      clickSound();
      tft.fillScreen(TFT_BLACK);
      tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
      tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("[---   Configuration Menu   ---]", 159, 0, 4);
      listMenu(select);
    }

    // -------- UPDATE FIRMWARE --------
    if (select == 2) {
      clickSound();
      tft.fillRect(0, 30, 320, 239, TFT_BLACK);
      tft.pushImage(56,  44, 60, 60, sdcard);
      tft.pushImage(203, 44, 60, 60, wifi);
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      tft.drawCentreString("Touch an icon to choose", 159, 125, 4);
      tft.drawCentreString("firmware update method.", 159, 155, 4);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.drawCentreString("[- Press button to exit -]", 159, 215, 4);
      delay(1000);
      while (digitalRead(SELECTOR_PIN) == HIGH) {
        autoDim();
        checkCPUTemp();
        uint16_t t_x = 0, t_y = 0;
        bool touched = getTouch(&t_x, &t_y);
        if (touched && (t_y > 44) && (t_y < 104)) {
          if ((t_x >= 56)  && (t_x < 116)) { clickSound(); updateFirmwareSD();         }
          if ((t_x >= 203) && (t_x < 263)) { clickSound(); updateFirmwareWIFI(); break; }
        }
        delay(300);
      }
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      listMenu(select);
    }

    // -------- WARNING SETTING --------
    if (select == 3) {
      clickSound();
      uint8_t warnSelected  = 0;
      uint8_t warnScrollTop = 0;

      tft.fillScreen(TFT_BLACK);
      drawWarnHeader();
      drawWarnList(warnSelected, warnScrollTop);
      drawWarnFooter();
      delay(400);

      while (digitalRead(SELECTOR_PIN) == HIGH) {
        autoDim();
        checkCPUTemp();

        if (foundOBD2) {
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.drawRightString(String(tempRead, 1) + "`F  ", 319, 28, 1);
        }

        uint16_t t_x = 0, t_y = 0;
        if (getTouch(&t_x, &t_y)) {
          clickSound();
          delay(150);

          if (t_y >= 220) {
            if (t_x < 100) {
              applyWarnChange(warnSelected, -1);
            } else if (t_x >= 220) {
              applyWarnChange(warnSelected,  1);
            } else {
              if (warnSelected == 0)
                tempOffset = factoryTempOffset;
              else
                warningValue[warnSelected-1] = pidConfig[warnSelected-1][8];
            }
            drawWarnList(warnSelected, warnScrollTop);

          } else if (t_y >= WARN_Y_START &&
                     t_y <  WARN_Y_START + WARN_ROWS * WARN_ROW_H) {
            uint8_t tappedRow = (t_y - WARN_Y_START) / WARN_ROW_H;
            uint8_t tappedIdx = warnScrollTop + tappedRow;
            if (tappedIdx < WARN_TOTAL) {
              warnSelected = tappedIdx;
              drawWarnList(warnSelected, warnScrollTop);
              drawWarnFooter();
            }

          } else if (t_y < WARN_Y_START) {
            if (warnScrollTop > 0) {
              warnScrollTop--;
              drawWarnList(warnSelected, warnScrollTop);
            }
          }

          // tap bottom row = scroll down
          int bottomRowY = WARN_Y_START + (WARN_ROWS - 1) * WARN_ROW_H;
          if (t_y >= (uint16_t)bottomRowY &&
              t_y <  (uint16_t)(bottomRowY + WARN_ROW_H) &&
              (int)warnScrollTop + WARN_ROWS < (int)WARN_TOTAL) {
            warnScrollTop++;
            drawWarnList(warnSelected, warnScrollTop);
          }
        }
        yield();
      }

      pref.putInt("tempOffset", tempOffset);
      for (uint8_t i = 0; i < maxpidIndex; i++)
        pref.putString(pidConfig[i][0].c_str(), warningValue[i]);

      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.drawCentreString("Settings Saved.", 159, 115, 4);
      beep();
      delay(800);
      tft.fillScreen(TFT_BLACK);
      tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
      tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("[---   Configuration Menu   ---]", 159, 0, 4);
      listMenu(select);
    }

    // -------- DTCs --------
    if (select == 4) {
      clickSound();
      tft.fillRect(0, 30, 320, 239, TFT_BLACK);
      tft.pushImage(0, 28, 25, 25, engine);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawString("Diagnostic Trouble Codes", 30, 28, 4);
      delay(500);
      getPID("ATE0");

      uint8_t error_cnt = 0;
      uint8_t no_of_dtc = 0;
      A = 0xFF;
      while ((A == 0xFF) && (error_cnt < 3)) {
        Serial.print(F("MIL Status 0101->"));
        getAB2(getPID("0101"), "41", "01");
        error_cnt++;
        delay(100);
      }

#ifdef TEST_DTC
      A = 0x86;
#endif

      if (A == 0xFF) {
        Serial.println(F("Error Reading MIL Status!"));
        tft.setTextColor(TFT_WHITE, TFT_RED);
        tft.drawCentreString("* Error Reading MIL Status! *", 159, 120, 4);
        tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        tft.drawCentreString("[ Please try again ]", 159, 148, 4);
        tft.setTextColor(TFT_BLACK, TFT_WHITE);
        tft.drawCentreString("[- Press button to exit -]", 159, 215, 4);
        beep();
      } else {
        if (A == 0x00) {
          tft.setTextColor(TFT_GREEN, TFT_BLACK);
          tft.drawCentreString("MIL is OFF - No DTC", 159, 120, 4);
          tft.setTextColor(TFT_BLACK, TFT_WHITE);
          tft.drawCentreString("[- Press button to exit -]", 159, 215, 4);
        } else if (A >= 0x80) {
          Serial.print(F("A = ")); Serial.println(A, HEX);
          uint8_t error_cnt2 = 0;
          String  dtcRead    = "";
          while ((dtcRead.substring(0, 2) != "43") &&
                 (dtcRead.substring(0, 2) != "00") &&
                 (error_cnt2 < 5)) {
            Serial.print(F("Read DTC 03->"));
            dtcRead = getPID("03");
#ifdef TEST_DTC
            dtcRead = "00E 0: 43 06 00 7D C6 93 1: 01 08 C6 0F 02 E9 02 2: E0 CC CC CC CC CC CC 43 01 C4 01";
#endif
            error_cnt2++;
            delay(100);
          }

          if (error_cnt2 >= 10) {
            Serial.println(F("Error Reading DTCs!"));
            tft.setTextColor(TFT_WHITE, TFT_RED);
            tft.drawCentreString("* Error Reading DTCs! *", 159, 120, 4);
            tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
            tft.drawCentreString("[ Please try again ]", 159, 148, 4);
            tft.setTextColor(TFT_BLACK, TFT_WHITE);
            tft.drawCentreString("[- Press button to exit -]", 159, 215, 4);
            beep();
          } else {
            beepbeep();
            int     byteCount   = 0;
            String  hexcode[128];
            uint8_t StringCount = 0;
            dtcRead.trim();
            while (dtcRead.length() > 0) {
              int    index   = dtcRead.indexOf(' ');
              String getByte = dtcRead.substring(0, index);
              if (getByte.indexOf(':') == -1) {
                if (getByte.length() >= 3) {
                  byteCount = strtol(getByte.c_str(), NULL, 16);
                  Serial.print(byteCount);
                  Serial.print(" = ");
                } else {
                  hexcode[StringCount++] = getByte;
                  Serial.print(getByte + ",");
                  if (StringCount == byteCount) break;
                }
              }
              dtcRead = dtcRead.substring(index + 1);
            }
            Serial.println();

            String  dtcList[16] = {};
            no_of_dtc = 0;
            for (uint8_t index = 0; index < StringCount; index++) {
              if (hexcode[index] == "43") { index += 2; }
              String  dtc_1 = hexcode[index];
              uint8_t code  = strtol(dtc_1.substring(0, 1).c_str(), 0, 16);
              dtc_1         = dtcMap[code] + dtc_1.substring(1);
              index++;
              String dtc_2       = hexcode[index];
              dtcList[no_of_dtc] = dtc_1 + dtc_2;
              Serial.print(dtcList[no_of_dtc] + ",");
              no_of_dtc++;
            }
            tft.setTextColor(TFT_RED, TFT_BLACK);
            String txt2 = "MIL is ON - DTCs = " + String(no_of_dtc);
            tft.drawCentreString(txt2, 159, 58, 4);
            tft.drawFastHLine(0, 85, 320, TFT_WHITE);
            Serial.printf("\nNo of DTCs = %d\n", no_of_dtc);

            tft.setTextColor(TFT_WHITE, TFT_BLACK);
            for (uint8_t i = 0; i < no_of_dtc; i = i + 4) {
              tft.setTextColor(TFT_PINK,   TFT_BLACK); tft.drawString(dtcList[i].c_str(),   0,   92 + 30 * round(i / 4), 4);
              tft.setTextColor(TFT_YELLOW, TFT_BLACK); tft.drawString(dtcList[i+1].c_str(), 80,  92 + 30 * round(i / 4), 4);
              tft.setTextColor(TFT_GREEN,  TFT_BLACK); tft.drawString(dtcList[i+2].c_str(), 160, 92 + 30 * round(i / 4), 4);
              tft.setTextColor(TFT_BLUE,   TFT_BLACK); tft.drawString(dtcList[i+3].c_str(), 240, 92 + 30 * round(i / 4), 4);
            }
            tft.setTextColor(TFT_BLACK, TFT_WHITE);
            tft.drawRightString("[Press button to exit]", 319, 220, 2);
            tft.fillRoundRect(0, 215, 127, 24, 12, TFT_RED);
            tft.setTextColor(TFT_WHITE);
            tft.drawCentreString("Clear MIL", 63, 217, 4);
          }
        }
      }

      while (digitalRead(SELECTOR_PIN) == HIGH) {
        checkCPUTemp();
        autoDim();
        if (no_of_dtc != 0) {
          uint16_t t_x = 0, t_y = 0;
          bool touched = getTouch(&t_x, &t_y);
          if (touched && (t_y > 225) && (t_x < 127)) {
            clickSound();
            BTSerial.print("04\r");
            tft.setTextColor(TFT_BLACK, TFT_GREEN);
            tft.drawCentreString("* Successful Clear MIL *", 159, 120, 4);
            Serial.println(F("* Successful Clear MIL *"));
            beepbeep();
            delay(3000);
            break;
          }
        }
        yield();
      }
      tft.fillRect(0, 28, 320, 239, TFT_BLACK);
      listMenu(select);
    }

    // -------- AUTO-OFF VOLTAGE --------
    if (select == 5) {
      clickSound();
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      tft.pushImage(0, 30, 25, 25, autooff);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Gauge Auto-off Setting", 40, 30, 4);
      tft.drawString("Set detection PCM voltage to turn off gauge.", 10, 65, 2);
      tft.drawString("If gauge turns off while engine is running,", 10, 85, 2);
      tft.drawString("please lower the voltage value.", 10, 105, 2);
      tft.drawString("Volt", 220, 135, 4);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.drawCentreString("[- Press button to exit -]", 159, 180, 4);
      for (uint8_t i = 0; i < 5; i++)
        tft.fillRoundRect(i * 64, 211, 60, 30, 5, TFT_NAVY);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("DEFAULT", 31,  217, 2);
      tft.drawCentreString("-",       223, 206, 6);
      tft.fillRect(286, 219, 3, 16, TFT_WHITE);
      tft.fillRect(280, 225, 16, 3,  TFT_WHITE);
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.drawCentreString(String(ecu_off_volt, 1).c_str(), 159, 135, 4);
      delay(1000);
      while (digitalRead(SELECTOR_PIN) == HIGH) {
        checkCPUTemp();
        autoDim();
        uint16_t t_x = 0, t_y = 0;
        if (getTouch(&t_x, &t_y) && (t_y > 211)) {
          clickSound();
          if (t_x < 64)                      ecu_off_volt = factoryECUOff;
          if ((t_x >= 192) && (t_x < 256)) { ecu_off_volt -= 0.1; if (ecu_off_volt < 11.0) ecu_off_volt = 11.0; }
          if ((t_x >= 256) && (t_x < 320)) { ecu_off_volt += 0.1; if (ecu_off_volt > 15.0) ecu_off_volt = 15.0; }
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.drawCentreString(String(ecu_off_volt, 1).c_str(), 159, 135, 4);
          delay(300);
        }
      }
      pref.putFloat("ecu_off_volt", ecu_off_volt);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.drawCentreString("Parameters Saved.", 159, 120, 4);
      beep();
      delay(1000);
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      listMenu(select);
    }

    // -------- ABOUT --------
    if (select == 6) {
      clickSound();
      delay(500);
      xTaskCreatePinnedToCore(
        &TaskPlayMusic,
        "play music in core 0",
        4096,
        NULL,
        3,
        &TaskHandle0,
        0);
      animation();
      vTaskDelete(TaskHandle0);
      pinMode(BUZZER_PIN, OUTPUT);
      ledcAttachPin(BUZZER_PIN, buzzerChannel);
      clickSound();
      tft.fillScreen(TFT_BLACK);
      tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
      tft.setTextColor(TFT_BLACK);
      tft.drawCentreString("[---   Configuration Menu   ---]", 159, 0, 4);
      listMenu(select);
    }

    // -------- EXIT --------
    if (select == 7) {
      clickSound();
      if (showsystem != currentFlag)
        pref.putBool("showsystem", showsystem);
      break;
    }

  }  // while(true)

  tft.fillScreen(TFT_BLACK);
}  // configMenu