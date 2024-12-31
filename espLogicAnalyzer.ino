#ifdef UBUNTU
#include "ESP32sim_ubuntu.h"
#else // #ifndef UBUNTU
#include <Adafruit_NeoPixel.h> 
#endif

#include "RollingLeastSquares.h"
//Adafruit_NeoPixel pixels(1, 21, NEO_GRB + NEO_KHZ800);
#include "driver/adc.h"

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
void setup() {
    Serial.begin(921600);
    analogRead(1);
}

//RollingLeastSquaresStatic<int, float, 4> avg;

uint32_t lastMicro = 0;
int reads = 0;


const int rmsStableThreshold = 50;
const int minDifference = 600;
const int histSize = 16;

uint32_t lastStableTime = 0;
int lastStableValue = 0;
int lastRms = 0;
int lastValue = 0;

struct Delay {
  static const int size = histSize;
  int data[size];
  int index = 0;
  int delay(int val) { 
    int rval = data[index];
    data[index] = val;
    if (++index >= size)
      index = 0;
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
} avg;

void loop() {
  uint16_t x = adc1_get_raw(ADC1_CHANNEL_1);//analogRead(1);
  //avg.add(delay.delay(x));
  avg.add(x);
  reads++;
  int rms = avg.error() + 1;
  int currentAvg = avg.average();
  uint32_t now = micros();
  if (lastMicro / 1000000 != now / 1000000) {  
    printf("0 0 poop %04d %04d %04d %d %d %d\n", currentAvg, rms, x, reads, avg.index, (int)avg.data[0]);
    reads = 0;
  }

  //printf("%08d 0 %04d\n", micros(), x);
  if (0 && rms < rmsStableThreshold && abs(lastStableValue - currentAvg) > minDifference) { 
    printf("%d %d\n", currentAvg / 10, (int)(now - lastStableTime));
    lastStableTime = now;
    lastStableValue = currentAvg;

  }
  if (rms > rmsStableThreshold && lastRms < rmsStableThreshold) {
    printf("%d %d\n", lastValue / 10, (int)(now - lastStableTime));
    lastStableTime = now;
  }
  lastMicro = now;
  lastValue = currentAvg;
  lastRms = rms;
}
#endif //I2S