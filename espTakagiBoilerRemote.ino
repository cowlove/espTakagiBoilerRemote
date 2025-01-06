#include "jimlib.h"
#ifndef CSIM
#include <Adafruit_NeoPixel.h> 
#include "driver/adc.h"
#else // #ifndef UBUNTU
#include "ESP32sim_ubuntu.h"
#define ADC1_CHANNEL_1 0
int adc1_get_raw(int) { return 0; }
#define delayUs(x) delayMicroseconds(x)
#endif

#include "RollingLeastSquares.h"
//Adafruit_NeoPixel pixels(1, 21, NEO_GRB + NEO_KHZ800);

unsigned long lastTimePrinted;
unsigned long loopTime = 0;


JStuff j;

void setup() {
    Serial.begin(921600);
    analogRead(1);
    j.mqtt.active = true;
    j.jw.debug = true;
    pinMode(1, OUTPUT);
}

//RollingLeastSquaresStatic<int, float, 4> avg;

uint32_t lastMicro = 0;
int reads = 0;


const int threshold = 2000;
const int deadband = 20;
const int histSize = 6;

uint32_t lastChangeTime = 0;

struct Delay {
  static const int size = histSize;
  int data[size];
  int index = 0;
  int delay(int val) { 
    int rval = data[index];
    data[index] = val;
    if (++index >= size)
      index = 0;
    return rval;
  }
} dly;

struct Averager {
  static const int size = histSize;
  int data[size];
  int index = 0;
  void add(uint16_t d) {
    data[index] = d;
    if (++index >= size)
      index = 0;
  }
  uint16_t average() { 
    int sum = 0;
    for(int i = 0; i < size; i++)
      sum += data[i];
    return sum / size;
  }
  int error() {
    int sum = 0;
    for(int i = 0; i < size - 1; i++)
      sum += (((int)data[(index + i) % size]) - ((int)(data[(index + i + 1) % size])));
    return abs(sum) / size;
  }
} avg1, avg2;

Averager avg;
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

uint32_t readPacket(uint32_t leadin, int pulsewidth) { 
  uint32_t startMicros = micros();
  uint32_t lastLow = startMicros;
  int tmo = 500000;
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
  uint32_t rval = 0;
  int bits = 0;
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
  //OUT("RECV: %08x %d", rval, bits);
  return rval;
}

bool toggle = 0;

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
  //OUT("SEND:          %08x", data);
}

int readAdcAverage(int count) { 
  int sum = 0;
  for(int i = 0; i < count; i++) {
    sum += adc1_get_raw(ADC1_CHANNEL_1);//analogRead(1);
  }
  return sum / count;
}

CLI_VARIABLE_HEXINT(cmd, 2);
int auxResp;
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

int setTempCmd = 0xa0243; // lowest setting

void loop() {
  j.run();
  int before, after;

  if (cmd == 1) ESP.restart();
  if (millis() == 30000 || j.jw.updateInProgress || cmd == 0)
    return;
  if (cmd > 0x10) 
    setTempCmd = cmd;


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
      j.run(); 
      doCmd(0xa3641, 20);
      j.run();
      doCmd(0xa3a41, 20);
      j.run();
      doCmd(0xa0243, 20);
    }

    if (cmd == 2) {
      // MODE 2 automatic temperature control based on inlet temperature 
      setTempCmd = 0xa0243; // 37 degC
      if (in > 15) setTempCmd = 0xad247; // 50
      if (in > 20) setTempCmd = 0xa3243; // 55
      if (in > 25) setTempCmd = 0xab247; // 60
      if (in > 40) setTempCmd = 0xa7247; // 70
      if (in > 50) setTempCmd = 0xaf243; // 75
    }
  }
}

void loop2() {
  j.run();
  if (j.jw.updateInProgress || millis() > 30000)
    return;
  uint32_t now = micros();
  uint16_t x = adc1_get_raw(ADC1_CHANNEL_1);//analogRead(1);

  avg1.add(dly.delay(x));
  avg2.add(x);
  reads++;

  if (lastMicro / 1000000 != now / 1000000) {  
    //OUT("0 0 poop %04d %04d %d", avg1.average(), x, reads);
    reads = 0;
  }

  //printf("%08d 0 %04d\n", micros(), x);
  if ((toggle && (avg1.average() - deadband > threshold) && (avg2.average() < threshold)) ||
      (!toggle && (avg1.average() + deadband < threshold) && (avg2.average() > threshold))) {
    toggle = !toggle; 
    //OUT("%d %d", avg1.average() / 1, (int)(now - lastChangeTime) / 1);
    lastChangeTime = now;
  }
  lastMicro = now;
}
