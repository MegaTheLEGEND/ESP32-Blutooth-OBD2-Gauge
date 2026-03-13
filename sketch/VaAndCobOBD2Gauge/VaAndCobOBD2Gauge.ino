/*=========  Va&Cob OBD2 Gauge ==========
Programmed:  by Ratthanin W.
Hardware:
- Hardware 2.8" TFT ESP32 LVGL https://www.youtube.com/watch?v=d2OXlVcRYrU
Configuration:
- Partition scheme
  Minimal SPIFFS 1.9MB APP/ 190kB SPIFFS /OTA (for OTA ,must reduce code size)

Important Note:
- New device must flash "gauge_factory_init.ino" first to get it work by checking 'serialno' in pref.
  or you can skip genuine checking by comment out line no. 338
- CPU temp read never work until Bluetooth or Wifi is connected

About opening logo image and goodbye image. 
- you can save welcome page logo into gauge by saving file name "vaandcob.jpg"  size 320x240 pixels into littleFS storage.
- you can save goodbye page image into gauge by saving file name "mypic.jpg" size 320z240 pixels into littleFS storage
* if you don't want to use it, you can comment out code line no. 320 - 322
*/

//--- FLAG SETTING ----- for debugging
//#define TERMINAL //temrinal mode (no gauge)
#define SERIAL_DEBUG //to show data in serial port
//#define SKIP_CONNECTION //skip elm327 BT connection to view meter
//#define LoadTestData //load test data without connect to ELM327, for testing meter display and layout
//#define TEST_DTC //test DTC

//#define FORD_T5   // FOR FORD T5 uncomment this line

//Intermal temperature sensor function declaration
#ifdef __cplusplus
extern "C" {
#endif
  uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();  //declare intermal temperature sensor functio

//Macro
#define array_length(x) (sizeof(x) / sizeof(x[0]))  //macro to calculate array length

//library load
#include <Preferences.h>  //save permanent data
Preferences pref;         //create preference
#include <BluetoothSerial.h>
#include <TFT_eSPI.h>  //Hardware-specific library


//Pin configuration
#define SCA_PIN 21  //not used same pin with TFT_BL
#define SCL_PIN 22  //not used (avaialable) CAN_TX
#define ADC_PIN 35  //not used (available)  CAN_RX
#define LED_RED_PIN 4
#define LED_GREEN_PIN 16
#define LED_BLUE_PIN 17
#define LDR_PIN 34       //LDR sensor
#define BUZZER_PIN 26    //speaker
#define SELECTOR_PIN 0  //push button
// setting PWM properties
#define backlightChannel 0
#define buzzerChannel 2

//DISPLAY
TFT_eSPI tft = TFT_eSPI();
#define TFT_GREY 0x5AEB

unsigned long lastResponseTime   = 0;
const unsigned long BT_TIMEOUT   = 15000;  // was 5000 — 15sec gives full PID cycle room

//ELM327 init https://www.elmelectronics.com/wp-content/uploads/2016/07/ELM327DS.pdf
const uint8_t elm327InitCount = 8;
const String elm327Init[elm327InitCount] = { "ATZ", "ATBRD23", "ATSP0", "ATAT2", "ATL0", "ATH0", "ATE0", "0100" };  //agressive wait response time from ECU
//const String elm327Init[elm327InitCount] = {"ATZ","ATPP0CSV35","ATPP0CON","ATSP0","ATAT2","ATL0","ATH0","ATE0","0100"};//agressive wait response time from ECU

/*------ PIDS total 7 now -------------------
Array data -> { ( [1]Label , [2]unit, [3]pid, [4]fomula, [5]min, [6]max, [7]skip, [8]digit, [9]warn }
label = Pid label show on meter
unit = unit for pid
pid = string pid 0104
formula = formula no to calculate 
min = lowest value
max = highest value
skip = delay reading 0 - 3; 3 = max delay read
digit = want to show digit or not
warn = default warning value
*/
const String pidConfig[26][9] = {
//  Label         unit     pid      fmt  min     max      skip digit warn
  { "ENG Load",   "%",    "0104",  "2", "0",   "100",   "0", "0", "80"   }, // 0
  { "Coolant",    "`F",   "0105",  "1", "40",  "240",   "3", "0", "210"  }, // 1
  { "MAP",        "psi",  "010B",  "0", "0",   "40",    "0", "1", "35"   }, // 2
  { "ENG SPD",    "rpm",  "010C",  "3", "0",   "5500",  "0", "0", "4500" }, // 3
  { "PCM Volt",   "volt", "0142",  "4", "0",   "16",    "1", "1", "15"   }, // 4
  { "Intake Tmp", "`F",   "010F",  "1", "0",   "150",   "3", "0", "120"  }, // 5
  { "Trans Tmp",  "`F",   "0105",  "1", "40",  "240",   "3", "0", "200"  }, // 6  placeholder
  { "Throttle",   "%",    "0111",  "2", "0",   "100",   "0", "0", "90"   }, // 7
  { "VSS",        "mph",  "010D",  "0", "0",   "120",   "0", "0", "110"  }, // 8
  { "Short Fuel", "%",    "0106",  "7", "-25", "25",    "1", "1", "20"   }, // 9
  { "Long Fuel",  "%",    "0107",  "7", "-25", "25",    "1", "1", "20"   }, // 10
  { "MAF",        "g/s",  "010C",  "8", "0",   "300",   "0", "1", "250"  }, // 11
  { "O2 B1S1",    "v",    "0114",  "9", "0",   "1",     "2", "2", "0"    }, // 12
  { "O2 B1S2",    "v",    "0115",  "9", "0",   "1",     "2", "2", "0"    }, // 13
  { "Timing",     "deg",  "010E",  "10","0",   "60",    "1", "1", "55"   }, // 14
  { "Baro",       "psi",  "0133",  "0", "10",  "17",    "3", "1", "16"   }, // 15
  { "EGR Err",    "%",    "012D",  "7", "-25", "25",    "3", "1", "20"   }, // 16
  { "Fuel Lvl",   "%",    "012F",  "2", "0",   "100",   "3", "0", "10"   }, // 17
  { "Abs Load",   "%",    "0143",  "4", "0",   "100",   "1", "0", "90"   }, // 18
  { "Rel Throt",  "%",    "0145",  "2", "0",   "100",   "0", "0", "90"   }, // 19
  { "Cmd EGR",    "%",    "012C",  "2", "0",   "100",   "0", "0", "90"   }, // 20
  { "Fuel Pres",  "psi",  "010A",  "11","0",   "111",   "0", "1", "100"  }, // 21
  { "Fuel Rate",  "g/h",  "015E",  "12","0",   "30",    "0", "0", "25"   }, // 22
  { "Odo",        "mi",   "01A6",  "13","0",   "999999","0", "0", "0"    }, // 23
  { "Amb Tmp",    "`F",   "0146",  "1", "-40", "150",   "3", "0", "120"  }, // 24
  { "Run Time",   "s",    "011F",  "14","0",   "65535", "0", "0", "0"    }, // 25
};

String warningValue[26] = {
  "80", "210", "35", "4500", "15", "120", "200",
  "90", "110", "20", "20",   "250", "0",  "0",
  "55", "16",  "20", "10",   "90", "90",
  "90", "100", "25", "0",    "120", "0"
};
/*  User configuration here to change display 
      layout 0      layout 1       layout 2     layout 3      layout 4     layout 5
    █ 1 █ █ 7 █   █ 1 █  █  █    █  █ 4 █  █   █  █  █ 7 █   █  █  █  █   █  █  █  █
    █ 2 █ █ 8 █   █ 2 █  3  4    1  █ 5 █  4   1  2  █ 8 █   1  2  3  4   1  2  3  4
    █ 3 █ █ 9 █   █ 3 █  █  █    █  █ 6 █  █   █  █  █ 9 █   █  █  █  █   █  █  █  █

set up meter here which pid to use on each cell
0 - engine load
1 - coolant
2 - manifold Pressure
3 - engine Speed
4 - pcm volt
5 - oil Temp
6 - trans Temp
*/
//if NO DATA value will set to 0 to skip reading
uint8_t layout = 0;                                   //pidInCelltype 0-4 EEPROM 0x00

uint8_t pidIndex = 0;                                 //point to PID List
const uint8_t maxpidIndex = array_length(pidConfig);  //total 7 pids in picConfig
bool prompt = false;                                  //bt elm ready flag
bool skip = false;                                    //pid skip flag
uint8_t A = 0;                                        //get A
uint8_t B = 0;                                        //get B

//Global variable
const uint32_t SLEEP_DURATION = 180 * 1000000;                         //180 sec* us = 3 min
String line[12] = { "", "", "", "", "", "", "", "", "", "", "", "" };  //buffer for each line in terminal function
bool dim = false;                                                      //back light dim flag
const uint8_t LIGHT_LEVEL = 40;                                        //backlight control by light level Higher = darker light
long runtime = 0;                                                      //to check FPS
long holdtime = 0;                                                     //hold time for button pressed check
const String compile_date = __DATE__ " - " __TIME__;                   //get built date and time
bool press = false;                                                    // button press flag
bool showsystem = false;                                               //show fps and temp EEPROM 0x01
uint8_t pidRead = 0;                                                   //counter that hold number of pids been read to check pid/sec
String serial_no = "";                                                 //keep serial no.

//CPU Temp
const uint8_t tempOverheat = 140;      // 140°F 
const int8_t factoryTempOffset = -50;  //default offset adjustment temp up to each tested esp32 max at -50
float tempRead = 0.0;                  //current reading cpu temp
int8_t tempOffset = 0;                 //offset adjustment temp up to each tested esp32
uint8_t temp_read_delay = 0;           //hold delay counter to read temp
uint8_t temp_overheat_count = 0;       //counter to read continuiosly overheat

//BLUETOOTH
String bt_message = "";                                                     //bluetooth message buffer
String se_message = "";                                                     //serial port message buffer
String deviceName[8] = { "", "", "", "", "", "", "", "" };                  //discoveryBT name
String deviceAddr[8] = { "", "", "", "", "", "", "", "" };                  //discover BT addr
uint8_t btDeviceCount = 0;                                                  //discovered bluetooth devices counter
#define BT_DISCOVER_TIME 5000                                               //bluetooth discoery time
esp_bd_addr_t client_addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };         //obdII mac addr
esp_bd_addr_t recent_client_addr = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };  //keep last btaddr in RTC memory
const String client_name = "OBDII";                                         //adaptor name to search
esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;                                  // or ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE to request pincode confirmation
esp_spp_role_t role = ESP_SPP_ROLE_SLAVE;                                   // or ESP_SPP_ROLE_MASTER
bool foundOBD2 = false;
BluetoothSerial BTSerial;  //bluetooth serial device

//---- Include Header File ---
#include "image.h"


/*----------- global function ---------*/

//------------------------------------------------------------------------------------------
void checkCPUTemp() {  //temperature can read only when BT or Wifi Connected
  temp_read_delay++;
  if ((temp_read_delay >= 50) && (foundOBD2)) {  //delay loop 50 then read temp, avoid error eading
    temp_read_delay = 0;
    uint8_t temperature = temprature_sens_read();      //read internal temp sensor
    tempRead = temperature + tempOffset;
#ifdef SERIAL_DEBUG
    Serial.print(tempRead, 1);  //print Celsius temperature and tab
    Serial.println("°c");
#endif
    //read 10 times to confirm it's really overtemp.
    if (tempRead >= tempOverheat) temp_overheat_count++;  //count overheat time
    else temp_overheat_count = 0;                         //reset
    if (temp_overheat_count > 10) {                       //overheat read more than 10 times.
      Serial.printf("CPU Overheat Shutdown at %d°F\n", tempOverheat);
      tft.fillRectVGradient(0, 0, 320, 240, TFT_RED, TFT_BLACK);
      tft.pushImage(127, 5, 64, 64, overheat, TFT_RED);  //show sdcard icon
      tft.setTextColor(TFT_WHITE);
      tft.drawCentreString("CPU Overheat Shutdown!", 159, 75, 4);
      tft.setTextColor(TFT_WHITE, TFT_BLUE);
      tft.drawCentreString("Auto-restart within 3 minutes", 159, 140, 4);
      tft.drawCentreString("OR", 159, 170, 4);
      tft.drawCentreString("Press button to restart", 159, 200, 4);
      ledcWriteTone(buzzerChannel, 1500);
      delay(5000);
      ledcWriteTone(buzzerChannel, 0);
      BTSerial.disconnect();  //disconnect bluetooth
      tft.fillScreen(TFT_BLACK);
      tft.writecommand(0x10);  //TFT sleep
      //enter deep sleep
      esp_sleep_enable_timer_wakeup(SLEEP_DURATION);   //sleep timer 3 min
      esp_sleep_enable_ext0_wakeup(GPIO_NUM_27, LOW);  //wake when button pressed
      esp_deep_sleep_start();                          //sleep shutdown backlight auto off with esp32
    }                                                  //if temp_overheat_coun
  }                                                    //if temp_read_delay
}  //check CPU Temp
/*------------------*/




//TERMINAL terminal for debugging display text
void Terminal(String texts, uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint8_t max_line = round((h - y) / 16.0);
  tft.fillRect(x, y, x + w, y + h, TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  for (uint8_t i = 0; i < max_line; i++) {
    if (i <= max_line - 2) {
      line[i] = line[i + 1];
    } else {
      line[i] = texts;
    }
    tft.drawString(line[i], x, i * 20 + y, 2);
  }                 //for
  bt_message = "";  //reset bt message
}
/*------------------*/

void checkGenuine() {                          //check if genuine obd2 gauge - must flash "gauge_factory_init.ino" first
                                               //security {serialno: "V&C-OBDII-001"}
  pref.begin("security", true);                //read only
  serial_no = pref.getString("serialno", "");  //if no serial no, then empty
  Serial.print(F("Serial No: "));
  if (serial_no == "") {
    tft.setTextColor(TFT_WHITE, TFT_RED);
    tft.drawString("Not genuine Va&Cob Gauge!", 0, 115, 4);
    Serial.println(F("Not genuine Va&Cob Gauge!"));
    while (true) {  //beep
      ledcWriteTone(buzzerChannel, 2000);
      digitalWrite(LED_RED_PIN, LOW);  //red on
      delay(500);
      ledcWriteTone(buzzerChannel, 0);
      digitalWrite(LED_RED_PIN, HIGH);  //red off
      delay(500);
    }
  } else {
    Serial.println(serial_no);  //print serial no
    pref.end();
  }
}
//----------------------------------------------




#ifdef LoadTestData
float getTestData(uint8_t pidIdx) {
  float t = millis() / 1000.0;
  switch (pidIdx) {
    case 0:  return 35.0  + sin(t * 0.3)  * 12.0 + random(-2, 2);           // ENG Load 23-47%
    case 1:  return 195.0 + sin(t * 0.05) * 8.0  + random(-1, 1);           // Coolant 187-203°F
    case 2:  return 14.0  + sin(t * 0.4)  * 4.0  + random(-1, 1);           // MAP 10-18 psi
    case 3:  return 850.0 + sin(t * 0.2)  * 400.0 + random(-20, 20);        // RPM 450-1250
    case 4:  return 13.8  + sin(t * 0.1)  * 0.4  + random(-1, 1) * 0.1;    // Voltage 13.4-14.2v
    case 5:  return 85.0  + sin(t * 0.15) * 15.0 + random(-2, 2);           // Intake 70-100°F
    case 6:  return 178.0 + sin(t * 0.07) * 10.0 + random(-1, 1);           // Trans 168-188°F
    case 7:  return 18.0  + sin(t * 0.5)  * 10.0 + random(-2, 2);           // Throttle 8-28%
    case 8:  return 45.0  + sin(t * 0.25) * 20.0 + random(-1, 1);           // VSS 25-65
    case 9:  return sin(t * 0.6)  * 4.0           + random(-1, 1) * 0.5;    // Short Fuel trim
    case 10: return sin(t * 0.2)  * 3.0           + random(-1, 1) * 0.3;    // Long Fuel trim
    case 11: return 0.0;                                                      // MAF - no sensor
    case 12: return 0.45 + sin(t * 1.2)        * 0.35 + random(-1,1)*0.02;  // O2 B1S1
    case 13: return 0.45 + sin(t * 1.2 + 1.0)  * 0.35 + random(-1,1)*0.02; // O2 B1S2
    case 14: return 12.0 + sin(t * 0.4)  * 8.0  + random(-1, 1);            // Timing 4-20 deg
    case 15: return 14.5 + sin(t * 0.02) * 0.3  + random(-1, 1) * 0.1;     // Baro
    case 16: return sin(t * 0.3)  * 5.0          + random(-1, 1);            // EGR Err
    case 17: return 62.0 + sin(t * 0.01) * 3.0;                              // Fuel Level
    case 18: return 40.0 + sin(t * 0.3)  * 15.0 + random(-2, 2);            // Abs Load
    case 19: return 18.0 + sin(t * 0.5)  * 10.0 + random(-2, 2);            // Rel Throt
    case 20: return 15.0 + sin(t * 0.3)  * 8.0  + random(-1, 1);            // Cmd EGR
    case 21: return 48.0 + sin(t * 0.4)  * 6.0  + random(-1, 1);            // Fuel Pres
    case 22: return 2.1  + sin(t * 0.3)  * 0.8  + random(-1, 1) * 0.1;     // Fuel Rate
    case 23: return 87432.0;                                                  // Odometer static
    case 24: return 68.0 + sin(t * 0.08) * 5.0  + random(-1, 1);            // Amb Tmp
    case 25: return t;                                                        // Run Time counts up
    default: return 0.0;
  }
}
#endif





//---- Include Header File --------------------
#include "bluetooth.h"
#include "layouts.h"
#include "meter.h"
#include "touchscreen.h"
#include "config.h"

//----SET UP -----------------------------------
void setup() {
  //pin configuration
  analogSetAttenuation(ADC_0db);        // 0dB(1.0 ครั้ง) 0~800mV   for LDR
  pinMode(LDR_PIN, ANALOG);             //ldr analog input read brightness
  pinMode(BUZZER_PIN, OUTPUT);          //speaker
  pinMode(SELECTOR_PIN, INPUT_PULLUP);  //button
  pinMode(LED_RED_PIN, OUTPUT);         //red
  pinMode(LED_GREEN_PIN, OUTPUT);       //green
  pinMode(LED_BLUE_PIN, OUTPUT);        //blue
  digitalWrite(LED_RED_PIN, LOW);       //on
  digitalWrite(LED_GREEN_PIN, HIGH);    //off
  digitalWrite(LED_BLUE_PIN, HIGH);     //off
  //pwm setup
  ledcSetup(buzzerChannel, 1500, 10);        //buzzer 10 bit
  ledcSetup(backlightChannel, 12000, 8);     //backlight 8 bit
  ledcAttachPin(BUZZER_PIN, buzzerChannel);  //attach buzzer

  //init communication
  Serial.begin(115200);
  //memory check
  psramInit();
  Serial.println(F("\n---------------------------------------\n"));
  Serial.printf("Free Internal Heap %d/%d\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("Free SPIRam Heap %d/%d\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("ChipRevision %d, Cpu Freq %d, SDK Version %s\n", ESP.getChipRevision(), ESP.getCpuFreqMHz(), ESP.getSdkVersion());
  Serial.printf("Flash Size %d, Flash Speed %d\n", ESP.getFlashChipSize(), ESP.getFlashChipSpeed());
  Serial.println(F("\n---------------------------------------\n"));
  Serial.println(F("<<  OBDII Gauge  >>"));
  Serial.println(compile_date);



  //init TFT display
  tft.init();
  tft.invertDisplay(true);
  tft.setRotation(1);  //landcape
                       // Start the SPI for the touchscreen and init the touchscreen
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchscreenSPI);
  ts.setRotation(1);

  touch_calibrate();  //hold button at start to calibrate touch
  //testTouch();//test touchscreen

  tft.setSwapBytes(true);  //to display correct image color
  // show_spiffs_jpeg_image("/vaandcob.jpg", 0, 0);// display logo image
  // delay(3000);

  //backlight ledcAttachPin must be set after tft.init()
  ledcAttachPin(TFT_BL, backlightChannel);  //attach backlight
  for (uint8_t i = 255; i > 0; i--) {       //fading effect
    ledcWrite(backlightChannel, i);         //full bright
    delay(5);
  }
  digitalWrite(LED_RED_PIN, HIGH);  //red off
  tft.fillScreen(TFT_BLACK);        //show start page
  tft.fillRectVGradient(0, 0, 320, 26, TFT_YELLOW, 0x8400);
  tft.setTextColor(TFT_BLACK);
  tft.pushImage(0, 0, 320, 25, obd2gauge);  //show logo
  ledcWrite(backlightChannel, 255);         //full bright
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.drawRightString("   Config button ->", 319, 26, 2);
  String txt = " BUILD : " + compile_date;
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString(txt, 0, 26, 2);

  // checkGenuine();//check guniune
  //init variable
  pref.begin("setting", false);
  /* Create a namespace called "setting" with read/write mode
{ pref.get...(key,default)
layout : UShort 
showsystem: bool
tempOffset : uint16_t
warning value: uint16_t
recent_client_addr : {0x00,0x00,0x00,0x00,0x00,0x00} array of bytes[6]
}*/
  layout = pref.getUShort("layout", 0);  //load layout setting from NVR
  if (layout > max_layout) layout = 0;
  showsystem = pref.getBool("showsystem", false);             //load showsytem
  tempOffset = pref.getInt("tempOffset", factoryTempOffset);  //load defaultTempoffset
#ifdef SERIAL_DEBUG
  Serial.printf("Show System: %d\n", showsystem);
  Serial.printf("Temp Offset: %d\n", tempOffset);
#endif
  for (uint8_t i = 0; i < maxpidIndex; i++) {  //read warning value from pid name if not found load from default
    warningValue[i] = pref.getString(pidConfig[i][0].c_str(), pidConfig[i][8]);
  }
  pref.getBytes("recent_client", recent_client_addr, pref.getBytesLength("recent_client"));  //read last bt address
  ecu_off_volt = pref.getFloat("ecu_off_volt", factoryECUOff);                               //read ecu 0ff voltage


  //start bluetooth
  if (!BTSerial.begin("Va&Cob OBDII Gauge", true)) {
    Serial.println(F("Bluetooth..error!"));
    Terminal("Bluetooth..error!", 0, 48, 320, 191);
    abort();
  } else {
    Serial.println(F("Bluetooth..OK"));
    Terminal("Bluetooth..OK", 0, 48, 320, 191);
  }
  runtime = millis();
#ifdef SKIP_CONNECTION
  foundOBD2 = true;
#endif

  //Connect to ELM327
  connectLastOBDII();   //try connect last BT
  while (!foundOBD2) {  //not success try scan and connect another OBD2
    scanBTdevice();
    autoDim();  //auto backlight handle
    //checkCPUTemp(); //check temp never work if BT or WIFI not connected
    if (digitalRead(SELECTOR_PIN) == LOW) {  //button pressed
      if (!press) {
        press = true;                          //set flag
        holdtime = millis();                   //set timer
      } else if (holdtime - millis() > 500) {  //press once and longer
        configMenu();                          //open config menu
        press = false;                         //reset flag
      }                                        //else if holdtimer > 30000
      delay(200);                              //delay avoid bounce
    }
  }

  //Initialize ELM327
  Terminal("Initializing...", 0, 48, 320, 191);
  bt_message = "";
  for (uint8_t initIndex = 0; initIndex < elm327InitCount; initIndex++) {  //send init ELM327 AT command
    BTSerial.print(elm327Init[initIndex] + "\r");
    if (initIndex == elm327InitCount - 1) delay(2000);  //wait for 0100 Searching..protocal
    else delay(100);                                    //must add delay , unless will skip BTSerial.avaialble>0
    while (BTSerial.available() > 0) {
      char incomingChar = BTSerial.read();
      if (incomingChar == '>') {  //check prompt
        BTSerial.flush();         //clear buffer
#ifdef SERIAL_DEBUG
        Serial.println(bt_message);
#endif
#ifdef TERMINAL
        Terminal(bt_message, 0, 48, 320, 191);
#endif
        if (initIndex == 1) BTSerial.print("\r");  //ATBRD23 response
        bt_message = "";
      } else {
        bt_message.concat(incomingChar);  //keep elm327 respond
      }
    }  //while
  }    //for

  initScreen();
  beepbeep();
  prompt = true;
  lastResponseTime = millis();
  //----------------------
}  //setup

/*###################################*/
void loop() {

  autoDim();
  handleTouch();


  //SCAN BUTON (button press HOLD to config menu)
  if (digitalRead(SELECTOR_PIN) == LOW) {  //button pressed
    if (!press) {
      press = true;                           //set flag
      holdtime = millis();                    //set timer
    } else if (holdtime - millis() > 3000) {  //press once and longer than 3 sec
      configMenu();                           //open config menu
      press = false;                          //reset flag
      if (foundOBD2) initScreen();            //open new layout screen
    }                                         //else if holdtimer > 30000
    delay(200);                               //delay avoid bounce
  } else {                                    //button release
    if (press) {                              //change layout next page
      if (graphActive) { graphExit(); return; }// exit graph if in graph page
      layout++;                               //change to next layout page
      if (layout == max_layout) layout = 0;
      pref.putUShort("layout", layout);
      ledcWriteTone(buzzerChannel, 5000);  //play click sound
      delay(5);
      ledcWriteTone(buzzerChannel, 0);
      initScreen();  //open meter screen
                     //reset variable
      for (uint8_t i = 0; i < maxpidIndex - 1; i++) old_data[i] = 0.0;
      engine_off_count = 0;
      BTSerial.flush();                   //clear tx
      while (BTSerial.available() > 0) {  //clear rx buffer
        BTSerial.read();
      }
      bt_message = "";
      skip = false;
      pidIndex = maxpidIndex - 1;  //will be +1 to be 0
      press = false;               //reset flag
      prompt = true;               //to trig reading elm again
      lastResponseTime = millis();
      delay(200);                  //delay avoid bounce

    }  //if press

  }  //else digitalRead
     //----------------------

#ifndef SKIP_CONNECTION
  // --- WATCHDOG BLOCK ---
  // only start counting after first successful connection
  if (foundOBD2 && lastResponseTime > 0 &&
      millis() - lastResponseTime > BT_TIMEOUT) {
    #ifdef SERIAL_DEBUG
    Serial.println("BT watchdog timeout - reconnecting...");
    #endif
    tft.fillScreen(TFT_BLACK);
    Terminal("Connection lost! Reconnecting...", 0, 48, 320, 191);
    digitalWrite(LED_GREEN_PIN, HIGH);  // green off
    digitalWrite(LED_RED_PIN,   LOW);   // red on
    foundOBD2    = false;
    prompt       = false;
    bt_message   = "";
    BTSerial.disconnect();
    delay(2000);  // give adapter time to fully drop
    connectLastOBDII();
    if (!foundOBD2) scanBTdevice();
    if (foundOBD2) {
      lastResponseTime = millis();
      initScreen();
      digitalWrite(LED_RED_PIN,   HIGH);  // red off
      digitalWrite(LED_GREEN_PIN, LOW);   // green on
    }
  }
  // --- END WATCHDOG BLOCK ---
#endif


     //BLUETOOTH read
  while (BTSerial.available() > 0) {
    char incomingChar = BTSerial.read();
    if (incomingChar == '>') {  //check prompt
      BTSerial.flush();         //clear tx
#ifdef SERIAL_DEBUG
      Serial.println(bt_message);
#endif
#ifdef TERMINAL  //for degbugging
      getAB(bt_message);
      Terminal(bt_message + "->" + String(A) + "," + String(B), 0, 48, 320, 191);
#endif
      prompt = true;  //pid response ready to calculate & display
      lastResponseTime = millis();  // reset watchdog
    } else {
      bt_message.concat(incomingChar);  //keep elm327 respond
    }
  }
  //----------------------
  //SERIAL PORT read
  if (Serial.available() > 0) {
    char incomingChar = Serial.read();
    //check CR/NL
    if ((incomingChar != '\r') && (incomingChar != '\n')) {
      se_message.concat(incomingChar);
      //Serial.print(incomingChar,HEX);
    } else {

      if (se_message != "") {
#ifdef TERMINAL
        Terminal(se_message, 0, 48, 320, 191);  //display terminal
#endif
        BTSerial.print(se_message + "\r");
        se_message = "";
        Serial.flush();
      }  //if se_message

    }  //else if incomingChar
  }    //if Serial.available


  //Send another PID to elm327
#ifdef LoadTestData
  static unsigned long lastTestUpdate = 0;
  if (millis() - lastTestUpdate > 100) {
    lastTestUpdate = millis();
    const LayoutDef &ld = layoutDefs[layout];
    if (graphActive) {
      uint8_t pidIdx = ld.cells[graphPid].pid;
      float   val    = getTestData(pidIdx);
      graphUpdate(graphPid, val);
    } else {
      for (uint8_t slot = 0; slot < ld.cellCount; slot++) {
        uint8_t pidIdx = ld.cells[slot].pid;
        float   val    = getTestData(pidIdx);
        updateMeter(slot, val);
      }
    }
  }
#else
  // normal OBD prompt block — unchanged
  if (prompt) {
    prompt = false;
    checkCPUTemp();
    if (!skip) {
      updateMeter(pidIndex, bt_message);
      pidRead++;
    }
    pidIndex++;
    if (pidIndex >= maxpidIndex) {
      pidIndex = 0;
      autoDim();
      if (showsystem) {
        String status = String(pidRead * 1000.0 / (millis() - runtime), 0) + " p/s " + String(tempRead, 1) + "`F ";
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawCentreString(status.c_str(), 159, 0, 2);
        runtime = millis();
      }
      pidRead = 0;
    }
    if (pidCurrentSkip[pidIndex] <= 0) {
      pidCurrentSkip[pidIndex] = pidReadSkip[pidIndex];
      skip = false;
      BTSerial.print(pidList[pidIndex] + "\r");
    } else {
      pidCurrentSkip[pidIndex] = pidCurrentSkip[pidIndex] - 1;
      prompt = true;
      lastResponseTime = millis();
      skip = true;
    }
  }
#endif

  /*------------------*/
}  // loop
