#include "jimlib.h"
#ifdef UBUNTU
#include "ESP32sim_ubuntu.h"
#define ADC1_CHANNEL_1 0
int adc1_get_raw(int) { return 0; }
#else // #ifndef UBUNTU
#include <Adafruit_NeoPixel.h> 
#include "driver/adc.h"
#endif

#include "RollingLeastSquares.h"
//Adafruit_NeoPixel pixels(1, 21, NEO_GRB + NEO_KHZ800);

unsigned long lastTimePrinted;
unsigned long loopTime = 0;

#ifdef I2S
#include <driver/i2s.h>
#define I2S_SAMPLE_RATE (3000)
#define ADC_INPUT (ADC1_CHANNEL_4) //pin 32
#define I2S_DMA_BUF_LEN (8)

// The 4 high bits are the channel, and the data is inverted
size_t bytes_read;
uint16_t buffer[I2S_DMA_BUF_LEN] = {0};


void i2sInit() {
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate =  I2S_SAMPLE_RATE,              // The format of the signal using ADC_BUILT_IN
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 2,
    .dma_buf_len = I2S_DMA_BUF_LEN,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_adc_mode(ADC_UNIT_1, ADC_INPUT);
  i2s_adc_enable(I2S_NUM_0);
  adc1_config_channel_atten(ADC_INPUT, ADC_ATTEN_DB_11);
}

void setup() {
  i2sInit();
}

void loop() {

  unsigned long startMicros = ESP.getCycleCount();

  i2s_read(I2S_NUM_0, &buffer, sizeof(buffer), &bytes_read, 0);

  unsigned long stopMicros = ESP.getCycleCount();

  loopTime = stopMicros - startMicros;

  if (millis() - lastTimePrinted >= 100) {
    Serial.println("------------------");
    Serial.println(buffer[0] & 0x0FFF);
    Serial.println(loopTime);
    lastTimePrinted = millis();
  }
}

#else

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

CLI_VARIABLE_HEXINT(cmd, 0xa3243);
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
void loop() {
  j.run();
  int before, after;

  if (cmd == 1) ESP.restart();
  if (millis() == 30000 || j.jw.updateInProgress || cmd == 0)
    return;

  int repeat = 8;
  int cmdResp = doCmd(cmd, repeat);
  int inTemp =  doCmd(0x90152, repeat);
  int outTemp = doCmd(0x90350, repeat);
  int flow =    doCmd(0x902d3, repeat);

  int c = cmd;
  int in = bitReverse((inTemp & 0xff0) >> 4, 8);
  int out = bitReverse((outTemp & 0xff0) >> 4, 8);
  int f = bitReverse((flow & 0xff0) >> 4, 8);
  int setT = bitReverse((cmdResp & 0xf00) >> 8, 4);
  OUT("CMD: %05x %05x %05x %05x %05x %05x (pl=%02d in=%02d out=%02d fl=%02d)",
    c, cmdResp, auxResp, inTemp, outTemp, flow, setT, in, out, f); 
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
#endif //I2S