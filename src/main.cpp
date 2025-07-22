#include <Arduino.h>
// Tremolo firmware
// ATtiny85 @ 16MHz internal

#define lfopin 0 // pin 0 on attiny PB0
#define ritpin 1 // modified PCB, ADC1
#define tappin 1 // PB1
#define shapepin 3 // ADC3
#define ratepin 2 // pin 4 is analog pin 2... ADC2

//TCCR0A = TCCR0A & 0b11111000 | 0b001 ; // PB0

#define DEBUG
// waveforms
size_t wave_size = 128;

const uint8_t sinWave_8bit[] = {0, 1, 1, 1, 1, 1, 2, 2, 3, 4, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 16, 17, 19, 20, 22, 24, 26, 27, 29, 31, 34, 36, 38, 40, 42, 45, 47, 50, 52, 55, 57, 60, 62, 65, 68, 71, 73, 76, 79, 82, 85, 88, 91, 94, 97, 100, 103, 106, 109, 112, 116, 119, 122, 125, 128, 131, 134, 137, 140, 144, 147, 150, 153, 156, 159, 162, 165, 168, 171, 174, 177, 180, 183, 185, 188, 191, 194, 196, 199, 201, 204, 206, 209, 211, 214, 216, 218, 220, 222, 225, 227, 229, 230, 232, 234, 236, 237, 239, 240, 242, 243, 245, 246, 247, 248, 249, 250, 251, 252, 252, 253, 254, 254, 255, 255, 255, 255, 255};

const uint8_t triangleWave_8bit[] = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 96, 98, 100, 102, 104, 106, 108, 110, 112, 114, 116, 118, 120, 122, 124, 126, 128, 129, 131, 133, 135, 137, 139, 141, 143, 145, 147, 149, 151, 153, 155, 157, 159, 161, 163, 165, 167, 169, 171, 173, 175, 177, 179, 181, 183, 185, 187, 189, 191, 193, 195, 197, 199, 201, 203, 205, 207, 209, 211, 213, 215, 217, 219, 221, 223, 225, 227, 229, 231, 233, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253};

const uint8_t squareWave_8bit[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

size_t counter = 0;

// Global variables
unsigned long maxRate = 300000;
unsigned long minRate = 1000;
uint8_t lfoResolution = wave_size / 2; //how many descrete values the LFO waveform has for each half period
int rate_hysteresis = 5; // how much analog value from ratePot can change before tap is overruled

float maxRateFromPot = 600; // actually sets minimum rate
float minRateFromPot = 1;   // actually sets maximum rate

auto tapTempoTimer = millis();
float tapRate = 50;
float masterRate;
bool tapReleased = true;
bool tapRateSet = false;
unsigned long tapHoldStart = 0;
unsigned long holdDuration = 100000;
bool doubleTime = false;
float doubleTimeDividend = 2.0; // sets how much the masterRate is divided by on long press

float lastRateFromPot = 0;

int currentPWMValue = 0; // will count to 255 and back again;

bool risingEdge = true; //selects which direction counter is counting

auto lastTick = millis();



int getRateFromPot() {
  float a = analogRead(ratepin);
  float scaledAndInverted = map(a, 0, 1024, maxRateFromPot, minRateFromPot);
  return scaledAndInverted;
}

void quick_blink() {
#ifdef DEBUG
  digitalWrite(lfopin, HIGH);
  delay(10);
  digitalWrite(lfopin, LOW);
#endif
}

void set_next_counter() {
  uint8_t sample_step = 2;

  if (counter >= wave_size - sample_step) { risingEdge = false;}
  else if (counter <= 0) { risingEdge = true;}
  if (risingEdge == true) { counter += sample_step; }
  else if (risingEdge == false ) { counter -= sample_step;}
}

void setup() {
  pinMode(lfopin, OUTPUT);
  pinMode(ratepin, INPUT);
  pinMode(tappin, INPUT);
  pinMode(shapepin, INPUT);
  pinMode(ritpin, INPUT);

  // @16MHz
  // TCCR0B = TCCR0B & 0b11111000 | 0b001 -> 65.5kHz
  // TCCR0B = TCCR0B & 0b11111000 | 0b010 -> 8.15kHz
  // TCCR0B = TCCR0B & 0b11111000 | 0b011 -> 1.02kHz
  // TCCR0B = TCCR0B & 0b11111000 | 0b100 -> 255Hz
  TCCR0B = (TCCR0B & 0b11111000) | 0b001 ; // PB1, set to divide-by-1 prescale  
  


  // check how long a second is
  for (int i = 0; i < 3; i++) {
    digitalWrite(lfopin, HIGH);
    delay(100*65.5);
    digitalWrite(lfopin, LOW);
    delay(100*65.5);
  }
}

size_t i = 0;

uint8_t next_lfo_value = 0;

void loop() {
//   if(digitalRead(tappin) == LOW && tapReleased == true) { // tappin is active LOW
//     unsigned long proposedTapRate = (millis() - tapTempoTimer);
//     if (proposedTapRate > maxRate) { // slow tremolo
//       // do nothing
//       tapRateSet = false;      
//     }
//     else if (proposedTapRate < minRate) { // fast tremolo
// //      Serial.println("Under minRate");
//       tapRate = minRate;
//       tapRateSet = true;
//       doubleTime = false;
//     }
//     else {
//       tapRate = proposedTapRate;
//       tapRateSet = true;
//       doubleTime = false;
//       tapHoldStart = millis();
//     }
//     tapTempoTimer = millis();
//     tapReleased = false;
//   }

//   if(digitalRead(tappin) == LOW && tapReleased == false && doubleTime == false && millis() > (tapHoldStart + holdDuration)) {
//       masterRate = masterRate/doubleTimeDividend; // double the speed
//       doubleTime = true;
//       tapRateSet = false;
//   }
  
//   if (digitalRead(tappin) == HIGH) {
//     tapReleased = true;
//   }
  
  if(getRateFromPot() != lastRateFromPot) {
    if((tapRateSet == false && doubleTime == false) || getRateFromPot() < (lastRateFromPot - rate_hysteresis) || getRateFromPot() > (lastRateFromPot + rate_hysteresis)) {
      masterRate = getRateFromPot();
      lastRateFromPot = getRateFromPot();
      tapRateSet = false;
      doubleTime = false;
    }
  }
  // else if(tapRateSet == true) {
  //   masterRate = tapRate/(lfoResolution*2.0); //need lfoResolution*2 tick for one LFO period
  // }

  if (millis() > (lastTick + masterRate)) {
    set_next_counter();
    lastTick = millis();
  
    // select shape of the signal
    int shape = analogRead(shapepin);
    if (shape < 512) {
      // linear combination of triangle and sine
      float sine = static_cast<float>(sinWave_8bit[counter]);
      float triangle = static_cast<float>(triangleWave_8bit[counter]);

      float sine_ratio = (1 / static_cast<float>(512)) * (shape);

      next_lfo_value = static_cast<uint8_t>(round( sine_ratio * sine + (1-sine_ratio) * triangle  ));
    }
    else {
      // linear combination of square and sine
      float sine = static_cast<float>(sinWave_8bit[counter]);
      float square = static_cast<float>(squareWave_8bit[counter]);

      float square_ratio = (1 / static_cast<float>(1024 - 512)) * (shape - 512);

      next_lfo_value = static_cast<uint8_t>(round( square_ratio * square + (1-square_ratio) * sine  ));
    }


    analogWrite(lfopin, next_lfo_value);
  }


  // delay(5000);

  // setNextPWMValue();

  // analogWrite(lfopin, currentWave[i]);
  // i++;
  // if (i >= currentWaveSize ) { i=0; }

}



  
