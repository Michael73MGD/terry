/*
 * This ESP32 code is created by esp32io.com
 *
 * This ESP32 code is released in the public domain
 *
 * For more detail (instruction and wiring diagram), visit https://esp32io.com/tutorials/esp32-led-matrix
 */

#include <MD_Parola.h>
#include <MD_MAX72xx.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4 // 4 blocks
#define CS_PIN 17

// create an instance of the MD_Parola class
MD_Parola ledMatrix = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

void setup() {
  ledMatrix.begin();         // initialize the LED Matrix
  ledMatrix.setIntensity(5); // set the brightness of the LED matrix display (from 0 to 15)
  ledMatrix.displayClear();  // clear LED matrix display
}

void loop() {
  ledMatrix.setTextAlignment(PA_LEFT);
  ledMatrix.print("Left"); // display text
  delay(2000);

  ledMatrix.setTextAlignment(PA_CENTER);
  ledMatrix.print("Center"); // display text
  delay(2000);

  ledMatrix.setTextAlignment(PA_RIGHT);
  ledMatrix.print("Right"); // display text
  delay(2000);

  ledMatrix.setTextAlignment(PA_CENTER);
  ledMatrix.setInvert(true);
  ledMatrix.print("Invert"); // display text inverted
  delay(2000);

  ledMatrix.setInvert(false);
  ledMatrix.print(1234); // display number
  delay(2000);
}
