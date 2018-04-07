#include <SPI.h>
#include <mcp_can.h>
#include "shift.h"



// pin for overheat light
int overheatPin = A0;

void setup() {
  pinMode(overheatPin, OUTPUT);
  initShift();
}

void loop() {
  // put your main code here, to run repeatedly:
  shiftUpdate();
}
