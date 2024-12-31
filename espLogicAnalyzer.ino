#include <Adafruit_NeoPixel.h> 
Adafruit_NeoPixel pixels(1, 21, NEO_GRB + NEO_KHZ800);

void setup() {
  pixels.begin();
}

unsigned char red = 0;
void loop() {
  const int maxLum = 50;
  pixels.clear();
  if (digitalRead(0)) { 
	pixels.setPixelColor(0, pixels.Color(random(maxLum), random(maxLum), random(maxLum)));
  } else { 
	pixels.setPixelColor(0, 0,0,red+=20);
  }
  pixels.show();
  delay(50);
  printf("poop %d\n", (int)red);
}
