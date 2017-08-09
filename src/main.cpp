// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <Arduino.h>
#include "avr_util.h"
#include "custom_defs.h"
#include "hardware_clock.h"
#include "io_pins.h"
#include "lin_processor.h"
#include "system_clock.h"
#include <EEPROM.h>


uint16_t lastPosition = 0;

uint16_t currentTarget = 0;
uint8_t initializedTarget = false;
uint8_t targetThreshold = 0;
uint8_t currentTableMovement = 0;


const int moveTableUpPin = PD4;
const int moveTableDownPin = PD7;


void printValues() {
  Serial.println("======= VALUES =======");
  Serial.print("Threshold is: ");
  Serial.println(targetThreshold);
  Serial.print("Current Position: ");
  Serial.println(lastPosition);
  Serial.println("======================");
}

void printHelp() {
  Serial.println("======= Serial Commands =======");
  Serial.println("Send 'STOP' to stop");
  Serial.println("Send 'HELP' to show this view");
  Serial.println("Send 'VALUES' to show the current values");
  Serial.println("Send 'T123' to set the threshold to 123 (255 max!)");
  Serial.println("Send '1580' to move to position 1580.");
  Serial.println("===============================");
}

void storeThreshold(uint8_t value) {
  if (value > 50 && value < 254) {
    targetThreshold = value;
    Serial.print("New Threshold: ");
    Serial.println(value);
    EEPROM.put(0, value);
  } else {
    Serial.println("Not stored. Keep your value between 50 and 254");
  }
}


// direction == 0 => Table stops
// direction == 1 => Table goes upwards
// direction == 2 => Table goes downwards
//
void moveTable(uint8_t direction) {
  if (direction != currentTableMovement) {
    currentTableMovement = direction;
    if (direction == 0) {
      Serial.println("Table stops");
      digitalWrite(moveTableUpPin, HIGH);
      digitalWrite(moveTableDownPin, HIGH);
    } else if (direction == 1) {
      Serial.println("Table goes up");
      digitalWrite(moveTableDownPin, HIGH);
      digitalWrite(moveTableUpPin, LOW);
    } else {
      Serial.println("Table goes down");
      digitalWrite(moveTableUpPin, HIGH);
      digitalWrite(moveTableDownPin, LOW);
    }
  }
}

// direction == 0 => Table is levelled
// direction == 1 => Target is above table
// direction == 2 => Target is below table
uint8_t desiredTableDirection() {

  int distance = lastPosition - currentTarget;
  uint16_t absDistance = abs(distance);

  if (absDistance > targetThreshold) {
    if (distance <= 0) { // table has to move up
      return 1;
    }
    return 2;
  }
  return 0;

}


void processLINFrame(LinFrame frame) {
  // Get the first byte which is the LIN ID
  uint8_t id = frame.get_byte(0);

  // 0x92 is the ID of the LIN node that sends the table position
  if (id == 0x92) {

    // the table position is a two byte value. LSB is sent first.
    uint8_t varA = frame.get_byte(2); //1st byte of the value (LSB)
    uint8_t varB = frame.get_byte(1); //2nd byte (MSB)
    uint16_t temp = 0;

    temp = varA;
    temp <<= 8;
    temp = temp | varB;

    if (temp != lastPosition) {
      lastPosition = temp;
      String myString = String(temp);
      char buffer[5];
      myString.toCharArray(buffer, 5);
      Serial.print("Current Position: ");
      Serial.println(buffer);

      if (initializedTarget == false) {
        currentTarget = temp;
        initializedTarget = true;
      }
    }

  }
}

void setup() {


  Serial.begin(115200);
  while (!Serial) {;};

  Serial.println("IKEA Hackant v1.0");
  Serial.println("Type 'HELP' to display all commands.");

  pinMode(moveTableUpPin, OUTPUT);
  pinMode(moveTableDownPin, OUTPUT);

  digitalWrite(moveTableUpPin, HIGH);
  digitalWrite(moveTableDownPin, HIGH);

  // setup everything that the LIN library needs.
  hardware_clock::setup();
  lin_processor::setup();

  // Enable global interrupts.
  sei();




  EEPROM.get(0, targetThreshold);
  if (targetThreshold == 255) {
    storeThreshold(120);
  }

  printValues();

}



void loop() {


  // Periodic updates.
  system_clock::loop();


  // Handle recieved LIN frames.
  LinFrame frame;

  // if there is a LIN frame
  if (lin_processor::readNextFrame(&frame)) {
    processLINFrame(frame);
  }

  // direction == 0 => Table is levelled
  // direction == 1 => Target is above table
  // direction == 2 => Target is below table
  uint8_t direction = desiredTableDirection();
  moveTable(direction);


  if (Serial.available() > 0) {

    // read the incoming byte:
    String val = Serial.readString();

    if (val.indexOf("HELP") != -1 || val.indexOf("help") != -1) {
      printHelp();
    } else if (val.indexOf("VALUES") != -1 || val.indexOf("values") != -1) {
      printValues();
    } else if (val.indexOf("STOP") != -1 || val.indexOf("stop") != -1) {

      if (direction == 1)
        currentTarget = lastPosition + (targetThreshold * 2);
      else if (direction == 2)
        currentTarget = lastPosition - (targetThreshold * 2);

      Serial.print("STOP at ");
      Serial.println(currentTarget);


    } else if (val.indexOf('T') != -1 || val.indexOf("t") != -1) {
      uint8_t threshold = (uint8_t)val.substring(1).toInt();
      storeThreshold(threshold);

    } else {
      if (val.toInt() > 150 && val.toInt() < 6400) {
        Serial.print("New Target ");
        Serial.println(val);
        currentTarget = val.toInt();
      } else {
        Serial.println("Not stored. Keep your value between 150 and 6400");
      }
    }
  }
}
