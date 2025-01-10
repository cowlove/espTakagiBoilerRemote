#include "jimlib.h"
#ifndef CSIM
#include <Adafruit_NeoPixel.h> 
#include "driver/adc.h"
#else // #ifndef UBUNTU
#include "ESP32sim_ubuntu.h"
#define ADC1_CHANNEL_1 0
int adc1_get_raw(int) { return 0; }
#define delayUs(x) delayMicroseconds(x)
#define NEO_GRB 0
#define NEO_KHZ800 0
struct Adafruit_NeoPixel {
  Adafruit_NeoPixel(int, int, int) {}
  static int Color(int, int, int) { return 0; }
  void begin() {}
  void setPixelColor(int, int a = 0, int b = 0, int c = 0) {}
  void show() {}
  void clear() {}
};
#endif

#include "RollingLeastSquares.h"

struct ESP32S3miniNeoPixel {
  Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, 21, NEO_GRB + NEO_KHZ800);
  bool firstRun = true;
  void set(int) {
    if (firstRun)
      pixels.begin();
    firstRun = false;
    int maxLum = millis() % 150;
    pixels.clear();
    pixels.setPixelColor(0, 0, maxLum, 0);
    pixels.show();    
  }
};

bool waitFor(bool level, uint32_t tmo);
uint32_t readPacket(uint32_t leadin, int pulsewidth);
void sendPacket(uint32_t data, int bytes);
uint16_t bitReverse(uint16_t x, int bits); 
uint32_t doCmd(uint32_t cmd, int repeat);

JStuff j;
ESP32S3miniNeoPixel led;
CLI_VARIABLE_HEXINT(cmd, 0);
int auxResp;

void setup() {
    Serial.begin(921600);
    analogRead(1);
    pinMode(1, OUTPUT);
    j.mqtt.active = true;
    j.jw.debug = true;
}

//////////////////////////////
// wait for either a high or low level, wait up to tmo microseconds
RollingAverage<int,6> avg;
int vlevel = 2100;
bool waitFor(bool level, uint32_t tmo) { 
  uint32_t startMicros = micros();
  while(true) { 
    uint32_t now = micros();
    uint16_t x = adc1_get_raw(ADC1_CHANNEL_1);//analogRead(1);
    avg.add(x);
    if (level && avg.average() > vlevel) return true;
    if (!level && avg.average() < vlevel) return true;
    if (now - startMicros > tmo) return false;
  }
}

/////////////////////////////////////
// read a packet, waiting for at least a leadin high period of at least leadin ms
uint32_t readPacket(uint32_t leadin, int pulsewidth) { 
  uint32_t startMicros = micros();
  uint32_t lastLow = startMicros;
  int tmo = 500000;
  j.run();

  while(true) { 
    uint32_t now = micros();
    uint16_t x = adc1_get_raw(ADC1_CHANNEL_1);//analogRead(1);
    avg.add(x);
    if (avg.average() < vlevel) 
      lastLow = now;

    if (now - startMicros > tmo) {
      OUT("timeout avg %d, sinceLow %d", avg.average(), now - lastLow); 
      return -1;
    }

    if (now - lastLow > leadin)
      break;
  }

  int rval = 0, bits = 0;
  while(1) { 
    if (!waitFor(0, (bits == 0) ? tmo : 2000))
      break;
    uint32_t startPulse = micros();
    if (!waitFor(1, 2500))
      break;
    uint32_t endPulse = micros();
    rval = rval << 1;
    if (endPulse - startPulse > pulsewidth) /*680*/
      rval = rval | 0x1;
    bits++;
  }
  LOG(2, "RECV: %08x %d", rval, bits);
  return rval;
}

/////////////////////////////
// send a packet
void sendPacket(uint32_t data, int bytes) { 
  int longWidth = 850;
  int shortWidth = 300;
  int period = 1980;
  for(int i = 0; i < bytes; i++) {
    bool longPulse = data & (0x1 << (bytes - 1 - i));
    uint32_t startPulse = micros();
    if (longPulse) {
      digitalWrite(1, 1);
      delayUs(longWidth);
      digitalWrite(1, 0);
      delayUs(period - longWidth);
    } else { 
      digitalWrite(1, 1);
      delayUs(shortWidth);
      digitalWrite(1, 0);
      delayUs(period - shortWidth);
    }
  }
  LOG(2, "SEND:          %08x", data);
}

uint32_t doCmd(uint32_t cmd, int repeat) { 
  uint32_t rval = -1;
  while(repeat-- > 0) {
    uint32_t pkt = readPacket(10000, 900);
    if ((pkt & 0x80000) == 0x80000) {
        rval = pkt;
        delay(3);
        sendPacket(cmd, 20);
    } else {
      auxResp = pkt;
    }
  } 
  return rval;
}

uint16_t bitReverse(uint16_t x, int bits) { 
  int rval = 0;
  while(bits-- > 0) { 
    rval = rval << 1;
    rval |= x & 0x1;
    x = x >> 1;
  }
  return rval;
}

int setTempCmd;

// CMD modes:
// 0 pause and wait for OTA 
// 1 reboot 
// 2 automatic temperature control based on inlet temp
// > 0x10: interpret cmd as panel packet and repeatedly send it

void loop() {
  j.run();

  if (cmd == 1) ESP.restart();
  if (j.jw.updateInProgress || cmd == 2) return;
  if (cmd > 0x10) setTempCmd = cmd;
  
  if (cmd == 3) { 
    // MODE 3 - don't send packets, infer timing and read control panel packet 
    // in addition to furnace packets 
    int cpkt = -1;
    int f1pkt = readPacket(150000, 900);
    if ((f1pkt != -1) && ((f1pkt & 0x80000) == 0x80000)) {
        cpkt = readPacket(1000, 500);
    } 
    int f2pkt = readPacket(2000, 900);
    if (cpkt != -1) 
      OUT("PAN: %05x %05x %05x", cpkt, f1pkt, f2pkt);
  } else { 
    int repeat = 8;
    int cmdResp = doCmd(setTempCmd, repeat);
    int inTemp =  doCmd(0x90152, repeat);
    int outTemp = doCmd(0x90350, repeat);
    int flow =    doCmd(0x902d3, repeat);

    int in = bitReverse((inTemp & 0xff0) >> 4, 8);
    int out = bitReverse((outTemp & 0xff0) >> 4, 8);
    int f = bitReverse((flow & 0xff0) >> 4, 8);
    int setT = bitReverse((cmdResp & 0xf00) >> 8, 4);
    OUT("CMD: %05x %05x %05x %05x %05x %05x (pl=%02d in=%02d out=%02d fl=%02d)",
      setTempCmd, cmdResp, auxResp, inTemp, outTemp, flow, setT, in, out, f); 

    if (cmdResp == 0xc8856) { 
      OUT("FURNACE RESET");
      doCmd(0xa3641, 20);
      doCmd(0xa3a41, 20);
      doCmd(0xa0243, 20);
    }
    if (cmd == 0) {
      // MODE 0 automatic temperature control based on inlet temperature 
      setTempCmd = 0xa0243; // 37 degC
      if (in > 15) setTempCmd = 0xad247; // 50
      if (in > 20) setTempCmd = 0xa3243; // 55
      if (in > 25) setTempCmd = 0xab247; // 60
      if (in > 40) setTempCmd = 0xa7247; // 70
      if (in > 50) setTempCmd = 0xaf243; // 75
    }
  }
}
