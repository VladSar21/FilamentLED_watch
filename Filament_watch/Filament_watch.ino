//  Arduino IDE 2.3.2
#include <WiFi.h>  // In-built
#include <SPI.h>   // In-built
#include <time.h>  // In-built
#include <EEPROM.h>
#include <WiFiManager.h>  //2.0.12
#include <GyverNTP.h>  // 1.3.1

GyverNTP ntp(4);

#define CS_595 5
#define DAT_595 23  // MOSI
#define CLK_595 18  // CLK

void TaskTimeToScreen(void *Parameters);

byte tmData[4] = {0b00000010, 0b00000010, 0b00000010, 0b00000010};

hw_timer_t *HalfSec_timer = NULL;

byte halfSec = 0;
byte oldhalfSec;
bool night = false;
String str = "";
bool newCommand = false;
byte ONOFF = 1, TOff = 23, TOn = 6;

const byte digit_stamp[18] = {
  // Q0 Q1 Q2 Q3 Q4 Q5 Q6 Q7
  // a  b  c  d  e  f  g  DP
  0b11111100,  // 0
  0b01100000,  // 1
  0b11011010,  // 2
  0b11110010,  // 3
  0b01100110,  // 4
  0b10110110,  // 5
  0b10111110,  // 6
  0b11100000,  // 7
  0b11111110,  // 8
  0b11110110,  // 9
  0b11101110,  // A
  0b00111110,  // b
  0b00011010,  // C
  0b01111010,  // d
  0b10011110,  // E
  0b10001110,  // F
  0b00000001,  // .
  0b00000000   //
};

void IRAM_ATTR onTimer() {
  halfSec = halfSec + 1;
  if (halfSec > 119) halfSec = 0;
}


void setup() {
  Serial.begin(115200);
  pinMode(CS_595, OUTPUT);
  pinMode(DAT_595, OUTPUT);
  pinMode(CLK_595, OUTPUT);
  EEPROM.begin(4);
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.autoConnect("FilamentWatch");
  ntp.begin();
  ntp.updateNow();
  WiFi.disconnect();
  EEPROM.get(1, ONOFF);
  EEPROM.get(2, TOff);
  EEPROM.get(3, TOn);

  HalfSec_timer = timerBegin(0, 80, true);
  timerAttachInterrupt(HalfSec_timer, &onTimer, true);
  timerAlarmWrite(HalfSec_timer, 500000, true);
  timerAlarmEnable(HalfSec_timer);

  xTaskCreate(TaskTimeToScreen, "TimeToScreen", 1000, (void *)&tmData, 2, NULL);
}

void loop() {
  if (oldhalfSec != halfSec & halfSec % 2 == 0) {
    prepareTime(true);
  } else if (oldhalfSec != halfSec & halfSec % 2 != 0) {
    prepareTime(false);
  }
  updateTime();
  checkSleepTime();
  if (Serial.available() > 0) {
    str = Serial.readString();
    Serial.println("# " + str.substring(0, str.length() - 1));
    newCommand = true;
  }
  if (newCommand) {
    commandStirng();
  }
}

void TaskTimeToScreen(void *Parameters)  
{
  byte *tData;
  tData = (byte *)Parameters;
  for (;;)  
  {
    digitalWrite(CS_595, LOW);
    shiftOut(DAT_595, CLK_595, LSBFIRST, tData[0]);
    digitalWrite(CS_595, HIGH);
    digitalWrite(CS_595, LOW);
    shiftOut(DAT_595, CLK_595, LSBFIRST, tData[1]);
    digitalWrite(CS_595, HIGH);
    digitalWrite(CS_595, LOW);
    shiftOut(DAT_595, CLK_595, LSBFIRST, tData[2]);
    digitalWrite(CS_595, HIGH);
    digitalWrite(CS_595, LOW);
    shiftOut(DAT_595, CLK_595, LSBFIRST, tData[3]);
    digitalWrite(CS_595, HIGH);
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}


void prepareTime(bool dp) {
  if (night == false && ONOFF == 1) {
    tmData[0] = digit_stamp[ntp.hour() / 10];
    tmData[1] = digit_stamp[ntp.hour() % 10];
    tmData[2] = digit_stamp[ntp.minute() / 10];
    if (dp == false) tmData[3] = digit_stamp[ntp.minute() % 10];
    else tmData[3] = digit_stamp[ntp.minute() % 10] | digit_stamp[16];
  } else if (night == true && ONOFF == 1) {
    tmData[0] = digit_stamp[17];
    tmData[1] = digit_stamp[17];
    tmData[2] = digit_stamp[17];
    if (dp == false) tmData[3] = digit_stamp[17];
    else tmData[3] = digit_stamp[17] | digit_stamp[16];
  } else if (ONOFF == 0) {
    tmData[0] = digit_stamp[17];
    tmData[1] = digit_stamp[17];
    tmData[2] = digit_stamp[17];
    tmData[3] = digit_stamp[17];
  }
}

void checkSleepTime() {
  if (ntp.hour() >= TOff || ntp.hour() <= TOn && ntp.second() == 0) {
    night = true;
  }
  if (ntp.hour() < TOff && ntp.hour() > TOn && ntp.second() == 0) {
    night = false;
  }
}

void updateTime(){
    if (ntp.minute() == 30 & ntp.second() == 0) {
    WiFi.reconnect();
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      if (attempt > 5) break;
      attempt = attempt + 1;
    }
    ntp.updateNow();
    WiFi.disconnect();
  }
}

void commandStirng() {
  if (str.indexOf("help") >= 0) {
    help();
    Serial.println("> Ok");
  } else if (str.indexOf("TOff?") >= 0) {
    Serial.println("> Time Off = " + String(int(TOff)));
    Serial.println("> Ok");
  } else if (str.indexOf("TOff=") >= 0) {
    TOff = str.substring(str.indexOf("TOff=") + 5, str.length()).toInt();
    if (TOff >= 0 && TOff <= 23) EEPROM.put(2, TOff);
    EEPROM.commit();
    delay(200);
    Serial.println("> Time Off set = " + String(int(TOff)));
    Serial.println("> Ok");
  } else if (str.indexOf("TOn?") >= 0) {
    Serial.println("> Time On = " + String(int(TOn)));
    Serial.println("> Ok");
  } else if (str.indexOf("TOn=") >= 0) {
    TOn = str.substring(str.indexOf("TOn=") + 4, str.length()).toInt();
    if (TOn >= 0 && TOn <= 23) EEPROM.put(3, TOn);
    EEPROM.commit();
    delay(200);
    Serial.println("> Time On set = " + String(int(TOn)));
    Serial.println("> Ok");
  } else if (str.indexOf("ONOFF?") >= 0) {
    Serial.println("> Time On = " + String(int(TOn)));
    Serial.println("> Ok");
  } else if (str.indexOf("ONOFF=") >= 0) {
    ONOFF = str.substring(str.indexOf("ONOFF=") + 6, str.length()).toInt();
    if (ONOFF >= 0 && ONOFF <= 1) EEPROM.put(1, ONOFF);
    EEPROM.commit();
    delay(200);
    Serial.println("> ONOFF set = " + String(int(ONOFF)));
    Serial.println("> Ok");
  } else if (str.indexOf("reset") >= 0) {
    ESP.restart();
  } else Serial.println("> Bad command or operator!");
  newCommand = false;
  str = "";
}

void help() {
  Serial.println("====================================================");
  Serial.println("   help, TOn?, TOn=, TOff?, TOff=, ONOFF=, reset  ");
  Serial.println("help - справка");
  Serial.println("TOn? - установленное значение времени включения");
  Serial.println("TOn= - установить значение времени включения");
  Serial.println("TOff? - установленное значение времени выключения");
  Serial.println("TOff= - установить значение времени выключения");
  Serial.println("ONOFF= - включить или выключить индикацию 1/0");
  Serial.println("reset - рестарт системы");
  Serial.println("====================================================");
}
