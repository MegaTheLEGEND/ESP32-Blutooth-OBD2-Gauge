/* "config.h"
this header file is for configuration menu
*/
//freeRTOS multi tasking
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "music.h"         //songs data
#include "nes_audio.h"     //nes audio player
#include "firmware_update.h" //firmware updater function
#include "elm327.h"        // read VIN function

#define mypic_filename "/mypic.jpg"
#define FORMAT_SPIFFS_IF_FAILED true

Cartridge player(BUZZER_PIN);

//configuration menu
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

//menu list for warning setting
const String parameterList[8][2] = {
  {"CPU Overheat Temperature "+String(tempOverheat)+"`F ,offset "," `F  "},
  {"- Engine Load"," %  "},
  {"- Coolant Temperature"," `F  "},
  {"- Manifold Air Pressure"," psi "},
  {"- Engine Speed"," rpm "},
  {"- PCM Voltage"," volt"},
  {"- Engine Oil Temperature"," `F  "},
  {"- Transmission Fluid Temperature"," `F  "}
};

const int pidWarnStep[8] = {1, 5, 1, 5, 500, 1, 1, 1};

//Sprites
TFT_eSprite car   = TFT_eSprite(&tft);
TFT_eSprite bk    = TFT_eSprite(&tft);
TFT_eSprite pole  = TFT_eSprite(&tft);
TFT_eSprite fence = TFT_eSprite(&tft);

//------ NES Audio play on Task0
TaskHandle_t TaskHandle0 = NULL;

void TaskPlayMusic(void *pvParameters) {
  esp_task_wdt_init(62, false);
  while (1) {
    player.play_nes(song, true, 0.25);
  }
}

//----------------------------
void animation() {
#define BK_HEIGHT 80
#define BK_WIDTH  235
  uint8_t  cur_x    = 68;
  uint8_t  cur_y    = 10;
  uint8_t  dash_x   = 20;
  int16_t  tag_x    = 235;
  int16_t  fence_x  = 0;
  uint8_t  ani_speed = 2;

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
  tft.drawBitmap(0, 159, qrcode, QRCODE_WIDTH, QRCODE_HEIGHT, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString(">MAP/ENG LOAD/ECT/EOT/TFT/ENG SPD/PCM Volt", 0, 40, 2);
  tft.drawString(">Warning, Automatic Dim/OnOff/O็verheat Shutdown", 0, 60, 2);
  tft.drawString(">Read DTC Code & Clear MIL Status", 0, 80, 2);
  tft.drawString("* FW-\"VaandCobOBD2Gauge.bin\" * Image-\"mypic.jpg\"", 0, 100, 2);
  tft.drawString("* Facebook : www.facebook.com/vaandcob", 0, 120, 2);
  tft.setTextColor(TFT_YELLOW);
  tft.drawString("  [ Manual ] ----------------[ Press button to exit ]", 0, 140, 2);
  tft.setTextColor(TFT_CYAN);
  String txt = "[ " + serial_no + " ] BUILD : " + compile_date;
  tft.drawString(txt, 0, 0, 2);
  getPID("ATE0");
  txt = "VIN :  " + getVIN(getPID("0902"));
  tft.setTextColor(TFT_GREENYELLOW);
  tft.drawString(txt, 0, 20, 2);

  bk.setTextColor(TFT_BLACK, TFT_ORANGE);

  while (digitalRead(SELECTOR_PIN) == HIGH) {
    bk.fillSprite(0x4228);
    bk.fillRect(0, 0, 235, 16, TFT_LIGHTGREY);
    bk.fillRect(0, 16, 235, 3, 0x8430);
    for (int16_t i = 0; i < 5; i++) {
      bk.drawFastHLine((dash_x - 20) * 1.5 + (60 * i), 45, 15, TFT_WHITE);
    }
    dash_x = dash_x - ani_speed;
    if (dash_x <= 0) dash_x = 40;
    tag_x = tag_x - ani_speed;
    if (tag_x <= -80) tag_x = 235;
    bk.drawString("< Thank You >", tag_x, 0, 2);
    int8_t move = random(-1, 2);
    cur_x = cur_x + move;
    move  = random(-1, 2);
    cur_y = cur_y + move;
    if (cur_x > 235 - CAR_WIDTH)  cur_x = 235 - CAR_WIDTH;
    if (cur_x < 1)                 cur_x = 1;
    if (cur_y < 1)                 cur_y = 1;
    if (cur_y > 79 - CAR_HEIGHT)   cur_y = 79 - CAR_HEIGHT;
    car.pushToSprite(&bk, cur_x, cur_y, TFT_BLACK);
    for (int i = 0; i < 8; i++)
      fence.pushToSprite(&bk, fence_x * 2 + (FENCE_WIDTH * i), 80 - FENCE_HEIGHT, TFT_BLACK);
    fence_x = fence_x - ani_speed;
    if (fence_x <= -FENCE_WIDTH + ani_speed) fence_x = 0;
    pole.pushToSprite(&bk, tag_x * 2, 80 - POLE_HEIGHT, TFT_BLACK);
    bk.pushSprite(85, 159);
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

/*------------------*/
void setWarning(int8_t index, int8_t change) {
  for (uint8_t i = 0; i < maxpidIndex + 1; i++) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(parameterList[i][0], 0, 58 + (i * 16), 2);
    tft.drawRightString(parameterList[i][1], 319, 57 + (i * 16), 2);
    String result = "";
    if (i == 0) result = String(tempOffset);
    else        result = warningValue[i - 1];
    switch (result.length()) {
      case 1: result = "   " + result; break;
      case 2: result = "  "  + result; break;
      case 3: result = " "   + result; break;
    }
    tft.drawRightString(result, 285, 57 + (i * 16), 2);
  }

  tft.setTextColor(TFT_BLACK, TFT_YELLOW);
  if (index == 0) {
    int mn = -99, mx = 0;
    if (change == -1) { tempOffset -= pidWarnStep[index]; if (tempOffset < mn) tempOffset = mn; }
    if (change ==  1) { tempOffset += pidWarnStep[index]; if (tempOffset > mx) tempOffset = mx; }
    String result = String(tempOffset);
    switch (result.length()) {
      case 1: result = "   " + result; break;
      case 2: result = "  "  + result; break;
      case 3: result = " "   + result; break;
    }
    tft.drawRightString(result, 285, 57 + (index * 16), 2);
  } else {
    int mn = pidConfig[index - 1][4].toInt();
    int mx = pidConfig[index - 1][5].toInt();
    if (change == -1) {
      warningValue[index-1] = String(warningValue[index-1].toInt() - pidWarnStep[index]);
      if (warningValue[index-1].toInt() < mn) warningValue[index-1] = String(mn);
    }
    if (change == 1) {
      warningValue[index-1] = String(warningValue[index-1].toInt() + pidWarnStep[index]);
      if (warningValue[index-1].toInt() > mx) warningValue[index-1] = String(mx);
    }
    String result = warningValue[index - 1];
    switch (result.length()) {
      case 1: result = "   " + result; break;
      case 2: result = "  "  + result; break;
      case 3: result = " "   + result; break;
    }
    tft.drawRightString(result, 285, 57 + (index * 16), 2);
  }
  delay(300);
}

/*--------------------*/
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

/*----------------*/
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
    if      (i < 4) tft.drawString(menuList[i], 32,           35 + 30 * i,      4);
    else if (i < 6) tft.drawString(menuList[i], 32+(i-4)*159, 35 + 30 * 4,      4);
    else            tft.drawString(menuList[i], 32+(i-6)*159, 35 + 30 * 5,      4);
  }
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("Touch top/bottom to navigate, center to select", 0, 211, 2);
  tft.drawString("Or: Press=Next  Hold=Select", 0, 227, 2);
}

/*----------------*/
void configMenu() {
  uint8_t select      = 0;
  bool    pressed     = false;
  long    holdtimer   = 0;
  bool    currentFlag = showsystem;

  // --------------------------------------------------------
  // Touch navigation helper
  // top third    = prev  (-1)
  // middle third = select (2)
  // bottom third = next   (1)
  // no touch     = 0
  // --------------------------------------------------------
  auto readTouchNav = [&]() -> int8_t {
    static unsigned long lastTouch = 0;
    const  unsigned long debounce  = 350;
    uint16_t tx, ty;
    if (!getTouch(&tx, &ty))             return 0;
    if (millis() - lastTouch < debounce) return 0;
    lastTouch = millis();
    clickSound();
    if (ty < (_height / 3))              return -1;  // top    = prev
    if (ty > ((_height / 3) * 2))        return  1;  // bottom = next
    return 2;                                         // center = select
  };

  beep();
  tft.fillScreen(TFT_BLACK);
  tft.fillRectVGradient(0, 0, 320, 26, TFT_ORANGE, 0xfc00);
  tft.setTextColor(TFT_BLACK);
  tft.drawCentreString("[---   Configuration Menu   ---]", 159, 0, 4);
  listMenu(select);
  delay(1000);

  while (true) {

    // ---- Physical button handling ----
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

    // ---- Touch navigation ----
    {
      int8_t t = readTouchNav();
      if (t == -1) {
        // top = previous menu item
        select = (select == 0) ? maxMenu - 1 : select - 1;
        listMenu(select);
      } else if (t == 1) {
        // bottom = next menu item
        select++;
        if (select == maxMenu) select = 0;
        listMenu(select);
      } else if (t == 2) {
        // center = select
        goto doSelect;
      }
    }

    checkCPUTemp();
    autoDim();
    continue;  // skip doSelect block unless jumped to

    // ========================================================
    // doSelect - triggered by button hold OR center tap
    // ========================================================
    doSelect:
    pressed = false;
    prompt  = true;

    // -------- TOGGLE SHOW SYSTEM STATUS --------
    if (select == 0) {
      beepbeep();
      showsystem = !showsystem;
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
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
      tft.setTextColor(TFT_BLACK, TFT_ORANGE);
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
          if ((t_x >= 56)  && (t_x < 116)) { clickSound(); updateFirmwareSD();   }
          if ((t_x >= 203) && (t_x < 263)) { clickSound(); updateFirmwareWIFI(); break; }
        }
        delay(300);
      }
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      listMenu(select);
    }

    // -------- WARNING SETTING --------
    if (select == 3) {
      int8_t warningMenuIndex = 0;
      int8_t inc_dec          = 0;
      clickSound();
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawCentreString("[ Warning Parameter Setting]", 159, 30, 2);
      tft.drawFastHLine(0, 50, 320, TFT_RED);
      tft.setTextColor(TFT_WHITE, TFT_RED);
      tft.drawCentreString("[- Press button to Save & Exit -]", 159, 190, 2);
      for (uint8_t i = 0; i < 5; i++)
        tft.fillRoundRect(i * 64, 211, 60, 30, 5, TFT_NAVY);
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("DEFAULT", 31,  217, 2);
      tft.drawCentreString("^ PREV",  95,  217, 2);
      tft.drawCentreString("NEXT v",  159, 217, 2);
      tft.drawCentreString("-",       223, 206, 6);
      tft.fillRect(286, 219, 3, 16, TFT_WHITE);
      tft.fillRect(280, 225, 16, 3, TFT_WHITE);
      setWarning(warningMenuIndex, inc_dec);
      delay(1000);
      while (digitalRead(SELECTOR_PIN) == HIGH) {
        autoDim();
        checkCPUTemp();
        if (foundOBD2) {
          String status = "    " + String(tempRead, 1) + "`F";
          tft.setTextColor(TFT_YELLOW, TFT_BLACK);
          tft.drawRightString(status.c_str(), 319, 30, 2);
        }
        uint16_t t_x = 0, t_y = 0;
        bool touched = getTouch(&t_x, &t_y);
        if (touched) {
          clickSound();
          if (t_y > 211) {
            // bottom button row
            if (t_x < 64) {
              tempOffset = factoryTempOffset;
              for (uint8_t i = 0; i < maxpidIndex; i++) warningValue[i] = pidConfig[i][8];
            }
            if ((t_x >= 64)  && (t_x < 128)) warningMenuIndex--;
            if ((t_x >= 128) && (t_x < 192)) warningMenuIndex++;
            if ((t_x >= 192) && (t_x < 256)) inc_dec = -1;
            if ((t_x >= 256) && (t_x < 320)) inc_dec =  1;
          } else {
            // touch nav within warning list
            // top third = prev param, bottom third = next param
            if      (t_y < _height / 3)       warningMenuIndex--;
            else if (t_y > (_height / 3) * 2) warningMenuIndex++;
          }
          if (warningMenuIndex == maxpidIndex + 1) warningMenuIndex = 0;
          if (warningMenuIndex < 0)                warningMenuIndex = maxpidIndex;
          setWarning(warningMenuIndex, inc_dec);
          inc_dec = 0;
        }
      }
      pref.putInt("tempOffset", tempOffset);
      for (uint8_t i = 0; i < maxpidIndex; i++)
        pref.putString(pidConfig[i][0].c_str(), warningValue[i]);
      tft.setTextColor(TFT_BLACK, TFT_GREEN);
      tft.drawCentreString("Parameters Saved.", 159, 120, 4);
      beep();
      delay(1000);
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
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
              String dtc_2      = hexcode[index];
              dtcList[no_of_dtc] = dtc_1 + dtc_2;
              Serial.print(dtcList[no_of_dtc] + ",");
              no_of_dtc++;
            }
            tft.setTextColor(TFT_RED, TFT_BLACK);
            String txt2 = "MIL is ON - No of DTCs = " + String(no_of_dtc);
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
          }  // else !error_read
        }  // else if A >= 0x80
      }  // else if error_cnt

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
    }  // select == 4

    // -------- AUTO-OFF VOLTAGE --------
    if (select == 5) {
      clickSound();
      tft.fillRect(0, 30, 320, 210, TFT_BLACK);
      tft.pushImage(0, 30, 25, 25, autooff);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.drawString("Gauge Auto-off Setting", 40, 30, 4);
      tft.drawString("Set detection PCM voltage to turn off gauge.", 10, 65, 2);
      tft.drawString("If gauge turn off while the engnine is running", 10, 85, 2);
      tft.drawString("Please lower down the voltage", 10, 105, 2);
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
          if (t_x < 64)                       ecu_off_volt = factoryECUOff;
          if ((t_x >= 192) && (t_x < 256))  { ecu_off_volt -= 0.1; if (ecu_off_volt < 11.0) ecu_off_volt = 11.0; }
          if ((t_x >= 256) && (t_x < 320))  { ecu_off_volt += 0.1; if (ecu_off_volt > 15.0) ecu_off_volt = 15.0; }
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
    }  // select == 5

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