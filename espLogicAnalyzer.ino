#ifdef UBUNTU
#include "ESP32sim_ubuntu.h"
#else // #ifndef UBUNTU
#include <Adafruit_NeoPixel.h> 
#endif

#include "RollingLeastSquares.h"
//Adafruit_NeoPixel pixels(1, 21, NEO_GRB + NEO_KHZ800);

void setup() {
}

RollingLeastSquaresStatic<int, float, 4> avg;

uint32_t lastMicro = 0;
int reads = 0;


float lastStableValue = 0;
float rmsStableThreshold = 3;
float minDifference = 100;
uint32_t lastStableTime = 0;

void loop() {
  int x = analogRead(1);
  avg.add(0, x);
  float rms = avg.rmsError();
  float currentAvg = avg.averageY();
  reads++;
  uint32_t now = micros();
  if (lastMicro / 1000000 != now / 1000000) {  
    printf("poop %08.2f %08.2f %04d %d \n", currentAvg, rms, x, reads);
    reads = 0;
  }

  if (rms < rmsStableThreshold && abs(lastStableValue - currentAvg) > minDifference) { 
    printf("%08.2f %08.2f %d\n", currentAvg, rms, (int)(now - lastStableTime));
    lastStableTime = now;
    lastStableValue = currentAvg;

  }
  lastMicro = now;
}
