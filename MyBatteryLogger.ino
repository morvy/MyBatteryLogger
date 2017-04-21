#include <RTClib.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SPI.h>
#include "SdFat.h"

const int8_t DISABLE_CHIP_SELECT = -1;
const uint8_t spiSpeed = SPI_HALF_SPEED;

SdFat sd;
SdFile dataFile;

#define LED 1 // notification LED

// RTC init
RTC_DS1307 rtc;
char rDate[11] = "";
char rTime[9] = "";

// SD init
#define chipSelect 10
char filename[13] = "";

// INA219 init
Adafruit_INA219 ina219;

// notification blinking
void notify(int duration) {
  for(int i=0; i<5; i++) {
    digitalWrite(LED, LOW);
    delay(duration);
    digitalWrite(LED, HIGH);
    delay(duration/2);
  }
}

// Nick's watchdog timer functions
ISR(WDT_vect) {
  wdt_disable();  // disable watchdog
}

void myWatchdogEnable(const byte interval) {  
  MCUSR = 0;                          // reset various flags
  WDTCSR |= 0b00011000;               // see docs, set WDCE, WDE
  WDTCSR =  0b01000000 | interval;    // set WDIE, and appropriate delay

  wdt_reset();
  set_sleep_mode (SLEEP_MODE_PWR_DOWN);  
  sleep_mode();            // now goes to Sleep and waits for the interrupt
}

void setup() {
  pinMode(LED, OUTPUT); // notification LED
  
  // Start RTC, short notify
  if (!rtc.begin()) {
    notify(400);
    return;
  }
  
  // Start SD, long notify
  if (!sd.begin(chipSelect, spiSpeed)) {
    notify(1200);
    sd.initErrorHalt();
    return;
  }
  // Start INA219;
  ina219.begin();
  ina219.setCalibration_32V_1A();

  // Create filename from current date YYYYMMDD.csv
  DateTime now = rtc.now();
  sprintf(filename, "%04d%02d%02d.csv", now.year(), now.month(), now.day());

  // Set header for readings
  if(dataFile.open(filename, O_WRITE)) {
    dataFile.println("\"DATE\";\"TIME\";\"SHUNT V\";\"BUS V\";\"CURRENT mA\";\"LOAD V\"");
    dataFile.close();
  }
}

void loop() {
  // let me know if you are alive
  notify(100);
  
  // prepare RTC strings
  DateTime now = rtc.now();
  sprintf(rDate, "%04d/%02d/%02d", now.year(), now.month(), now.day());
  sprintf(rTime, "%02d:%02d:%02d", now.hour(), now.minute(), now.second());

  // prepare INA219 variables
  float shuntvoltage = 0;
  float busvoltage = 0;
  float current_mA = 0;
  float loadvoltage = 0;

  // read sensor data
  shuntvoltage = ina219.getShuntVoltage_mV();
  busvoltage = ina219.getBusVoltage_V();
  current_mA = ina219.getCurrent_mA();
  loadvoltage = busvoltage + (shuntvoltage / 1000);
  
  // Write all to SD card
  if (dataFile.open(filename, O_WRITE)) {
    dataFile.print('"');
    dataFile.print(rDate);
    dataFile.print("\";\"");
    dataFile.print(rTime);
    dataFile.print("\";\"");
    dataFile.print(shuntvoltage);
    dataFile.print("\";\"");
    dataFile.print(busvoltage);
    dataFile.print("\";\"");
    dataFile.print(current_mA);
    dataFile.print("\";\"");
    dataFile.print(loadvoltage);
    dataFile.println('"');
    dataFile.close();
  }
  for(int i = 0; i<15; i++) {
    myWatchdogEnable (0b100001);  // 8 seconds * 15 = 2 minutes
  }
}

