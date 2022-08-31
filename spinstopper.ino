#include <Adafruit_NeoPixel.h>
#include <Adafruit_Thermal.h>
#include <SoftwareSerial.h>
#include <TM1637.h>
#include <EEPROM.h>

#define VELOCITY 10
#define CREDITS_2_EUR 3
#define CREDITS_1_EUR 1
#define PULSE_THRESHOLD 3
#define BRIGHTNESS 16
#define MYSTERY_MIN 5 // * 10
#define MYSTERY_MAX 100 // * 10

Adafruit_NeoPixel strip(72, 3, NEO_GRB + NEO_KHZ800);
TM1637 displayA(A2, A3);
TM1637 displayB(A4, A5);
SoftwareSerial swSerial(0, 1);
Adafruit_Thermal printer(&swSerial);

int plus250s[16] = {0, 8, 9, 17, 18, 26, 27, 35, 36, 44, 45, 53, 54, 62, 63, 71};
int plus100s[16] = {1, 7, 10, 16, 19, 25, 28, 34, 37, 43, 46, 52, 55, 61, 64, 70};
int plus50s[16] = {2, 6, 11, 15, 20, 24, 29, 33, 38, 42, 47, 51, 56, 60, 65, 69};
long score = 0;
long romscore = (EEPROM.read(0) * pow(256, 2)) + (EEPROM.read(1) * 256) + EEPROM.read(2);
long highscore = romscore;
int credits = 0;
int pixel = 0;
int flashes = 0;
int pulses = 0;
int game_totalCredits = 0;
int game_coinInserted = 0;
long firstpulse = 0;
bool oldButtonState = HIGH;
bool bonusCredit = false;
bool bonusCreditRolled = false;
bool pricesVisible = false;
bool processingGameOver = false;
bool pressed = LOW;

void setCredits(int to, bool dot = false) {
  bool creditOverflow = false;
  credits = max(0, to);
  if (credits > 9) {
    credits = 9;
    creditOverflow = true;
    bonusCredit = true;
  }
  bool pinModes[8] = {
    (credits == 0 || credits == 2 || credits == 6 || credits == 8),
    (credits == 0 || credits == 2 || credits == 3 || credits == 5 || credits == 6 || credits >= 8),
    (credits != 2),
    (dot || creditOverflow),
    (credits == 2 || credits == 3 || credits == 4 || credits == 5 || credits == 6 || credits >= 8),
    (credits == 0 || credits == 4 || credits == 5 || credits == 6 || credits >= 8),
    (credits == 0 || credits == 2 || credits == 3 || credits >= 5),
    (credits <= 4 || credits >= 7)
  };
  
  for (int i=0; i<8; i++) digitalWrite(i + 4, pinModes[i]);
}

void setHighscore(long score, int colonpoint = 0) {
  if (score > highscore && millis() > 5500) {
    int byte1 = min(255, (score / 256 / 256));
    int byte2 = (score / 256) % 256;
    int byte3 = score % 256;
    EEPROM.write(0, byte1);
    EEPROM.write(1, byte2);
    EEPROM.write(2, byte3);
  }
  highscore = score;
  if (highscore > 16777215) highscore = 16777215;
  displayB.point(colonpoint);
  displayB.clearDisplay();
  if (highscore <= 9999) {
    displayB.display(3, highscore % 10);
    if (score >= 10) displayB.display(2, highscore / 10 % 10);
    if (score >= 100) displayB.display(1, highscore / 100 % 10); 
    if (score >= 1000) displayB.display(0, highscore / 1000 % 10);
  } else {
    int power = log10(highscore) - 1;
    int digits = highscore / pow(10, power);
    displayB.display(3, power);
    displayB.display(2, 14);
    displayB.display(1, digits % 10);
    displayB.display(0, digits / 10 % 10);
  }
}

void setScore(long to, int colonpoint = 0) {
  if (to > highscore) setHighscore(to, colonpoint);
  score = to;
  if (score > 16777215) score = 16777215;
  displayA.point(colonpoint);
  displayA.clearDisplay();
  if (score <= 9999) {
    displayA.display(3, score % 10);   
    if (score >= 10) displayA.display(2, score / 10 % 10);   
    if (score >= 100) displayA.display(1, score / 100 % 10);   
    if (score >= 1000) displayA.display(0, score / 1000 % 10);
  } else {
    int power = log10(score) - 1;
    int digits = score / pow(10, power);
    displayA.display(3, power);
    displayA.display(2, 14);
    displayA.display(1, digits % 10);
    displayA.display(0, digits / 10 % 10);
  }
}

void coinAlert() {
  if (credits > 0 || bonusCredit || processingGameOver) return;
  if (pulses == 0) firstpulse = millis();
  pulses++;
}

void togglePrices() {
  if (firstpulse > 0 && millis() - firstpulse >= 500) return;
  if (pricesVisible) {
    setScore(score);
    setHighscore(highscore);
  } else {
    displayA.clearDisplay();
    displayB.clearDisplay();
    displayA.point(1);
    displayB.point(1);
    displayA.display(0, 2);
    displayA.display(1, 14);
    displayA.display(3, CREDITS_2_EUR);
    displayB.display(0, 1);
    displayB.display(1, 14);
    displayB.display(3, CREDITS_1_EUR);
  }
  pricesVisible = !pricesVisible;
}

int pixelInBounds(int pixel) {
  int num = pixel;
  if (num >= 72) num -= 72;
  else if (num < 0) num += 72;
  return num;
}

void gameOver() {
  processingGameOver = true;
  strip.clear();
    for (int i=0; i<4; i++) strip.fill(strip.Color(255, 0, 0), i * 18 + 9, 9);
    strip.show();
  if (score == 0) {
    for (int i=0; i<3; i++) {
      delay(300);
      strip.clear();
      strip.show();
      delay(250);
      for (int j=0; j<4; j++) strip.fill(strip.Color(255, 0, 0), j * 18 + 9, 9);
      strip.show();
    }
    delay(300);
    strip.clear();
    strip.show();
    delay(250);
  } else {
    printer.doubleHeightOff();
    printer.setLineHeight(0);
    printer.setFont('A');
    printer.justify('C');
    printer.setSize('L');
    printer.println("Spin Stopper");
    printer.setSize('M');
    printer.println("Game Overview Receipt");
    printer.setSize('S');
    printer.justify('L');
    printer.println("Inserted coin: " + String(game_coinInserted) + " EUR");
    printer.println("Credits used: " + String(game_totalCredits));
    printer.boldOn();
    printer.println("Earned score: " + String(score));
    printer.boldOff();
    printer.println("Current highscore: " + String(highscore));
    printer.justify('C');
    printer.setSize('M');
    printer.println("Well done!");
    printer.println("Play again soon!");
    printer.feed(3);
    delay(750);
  
    strip.clear();
    for (int i=0; i<4; i++) strip.fill(strip.Color(0, 255, 0), i * 18 + 9, 9);
    strip.show();
  
    printer.sleep();
    delay(3000);
    printer.wake();
    printer.setDefault();
  }
  processingGameOver = false;
}

void setup() {
  for (int i=3; i<=12; i++) pinMode(i, OUTPUT);
  pinMode(A0, INPUT_PULLUP);
  randomSeed(analogRead(13));
  swSerial.begin(9600);
  printer.begin();
  displayA.init();
  displayA.set(5);
  displayB.init();
  displayB.set(5);
  setCredits(8, HIGH);
  setScore(8888, 1);
  setHighscore(8888, 1);
  strip.begin();
  strip.setBrightness(max(10, BRIGHTNESS - 4));
  strip.fill(strip.Color(255, 255, 255));
  strip.show();
  delay(4000);
  strip.clear();
  strip.show();
  displayA.point(0);
  displayA.clearDisplay();
  displayB.point(0);
  displayB.clearDisplay();
  for (int i=4; i<=12; i++) digitalWrite(i, LOW);
  delay(1000);
  strip.setBrightness(BRIGHTNESS);
  setCredits(0);
  setScore(0, 0);
  setHighscore(romscore, 0);
  attachInterrupt(digitalPinToInterrupt(2), coinAlert, FALLING);
}

void loop() {
  if (firstpulse > 0 && millis() - firstpulse >= 500 && pulses > 0 && credits == 0 && !bonusCredit) {
    int incoming = (pulses < PULSE_THRESHOLD) ? CREDITS_1_EUR : CREDITS_2_EUR;
    game_coinInserted = (pulses < PULSE_THRESHOLD) ? 1 : 2;
    firstpulse = pulses = game_totalCredits = 0;
    setCredits(incoming);
    setScore(0);
    setHighscore(highscore);
  }
  digitalWrite(12, (credits > 0 || bonusCredit));
  bool newButtonState = digitalRead(A0);
  if (!newButtonState && oldButtonState && (credits > 0 || bonusCredit) && !pressed) {
    delay(ceil(VELOCITY / 2));
    newButtonState = digitalRead(A0);
    if (!newButtonState) pressed = HIGH;
  }
  oldButtonState = newButtonState;
  if (pressed) {
    if (flashes == 0) strip.clear();
    flashes++;
    strip.setPixelColor(pixel, strip.Color(255, 255, 255));
    strip.show();
    delay(200);
    strip.clear();
    strip.show();
    delay(150);
    if (flashes == 1 && bonusCredit) {
        bonusCreditRolled = true;
        digitalWrite(7, LOW);
    } else if (flashes == 2) {
      game_totalCredits++;
      for (int i=0; i<16; i++) {
        if (pixel == plus250s[i]) setScore(score + 250); // +250 score
        if (pixel == plus100s[i]) setScore(score + 100); // +100 score
        if (pixel == plus50s[i]) setScore(score + 50); // +50 score
      }
      if (pixel == 22 || pixel == 58) setScore(score + random(MYSTERY_MIN, MYSTERY_MAX + 1) * 10); // mystery value
      else if (pixel == 4) setScore(score * 2); // double score
      else if (pixel % 18 == 13) { // +1 credit (bonus indicated by dot)
        bonusCredit = true;
        bonusCreditRolled = false;
        setCredits(credits, true);
      } else if (pixel == 40) setCredits(credits + 3); // +3 credits
    } else if (flashes > 8) {
      pressed = LOW;
      if (bonusCreditRolled) {
        bonusCreditRolled = false;
        bonusCredit = false;
        if (credits == 0) gameOver();
      } else {
        setCredits(credits - 1);
        if (credits == 0 && !bonusCredit) gameOver();
      }
      flashes = 0;
    }
  } else if (credits > 0 || bonusCredit) {
    if (++pixel >= 72) pixel = 0;
    strip.clear();
    strip.setPixelColor(pixel, strip.Color(255, 255, 255));
    for (int i=1; i<6; i++) strip.setPixelColor(pixelInBounds(pixel - i), strip.ColorHSV(0, 0, 255 - i * 40));
    strip.show();
    if (bonusCredit) digitalWrite(7, (pixel % 18 < 9));
    delay(VELOCITY);
  } else {
    // theater chase
    uint32_t color = strip.gamma32(strip.ColorHSV(random(65536)));
    for(int a=0; a<40; a++) {
      for(int b=0; b<3; b++) {
        if (firstpulse > 0 && millis() - firstpulse >= 500) break;
        strip.clear();
        for (int c=b; c<72; c += 3) strip.setPixelColor(c, color);
        strip.show();
        delay(40);
      }
    }
    // 8 block cycle
    togglePrices();
    color = strip.gamma32(strip.ColorHSV(random(65536)));
    for (int a=0; a<20; a++) {
      for (int i=0; i<4; i++) {
        if (firstpulse > 0 && millis() - firstpulse >= 500) break;
        strip.clear();
        strip.fill(color, i * 9, 9);
        strip.fill(color, i * 9 + 36, 9);
        strip.show();
        delay(70);
      }
    }
    // rainbow
    togglePrices();
    for (long firstPixelHue = 0; firstPixelHue < 3*65536; firstPixelHue += 256) {
      if (firstpulse > 0 && millis() - firstpulse >= 500) break;
      for (int i=0; i<72; i++) {
        int pixelHue = firstPixelHue + (i * 65536L / 72);
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
      strip.show();
      delay(6);
    }
    strip.clear();
    strip.show();
    // fill up
    togglePrices();
    color = strip.gamma32(strip.ColorHSV(random(65536)));
    for (int a=0; a<6; a++) {
      for (int i=0; i<36; i++) {
        if (firstpulse > 0 && millis() - firstpulse >= 500) break;
        strip.setPixelColor(4 + i, color);
        int rightSide = 4 - i;
        if (rightSide < 0) rightSide += 72;
        strip.setPixelColor(rightSide, color);
        strip.show();
        delay(20);
      }
      strip.fill(color);
      for (int i=0; i<36; i++) {
        if (firstpulse > 0 && millis() - firstpulse >= 500) break;
        strip.setPixelColor(4 + i, strip.Color(0, 0, 0));
        int rightSide = 4 - i;
        if (rightSide < 0) rightSide += 72;
        strip.setPixelColor(rightSide, strip.Color(0, 0, 0));
        strip.show();
        delay(20);
      }
      strip.fill(strip.Color(0, 0, 0)); 
    }
    // random on/off
    togglePrices();
    color = strip.gamma32(strip.ColorHSV(random(65536)));
    for (int a=0; a<200; a++) {
      if (firstpulse > 0 && millis() - firstpulse >= 500) break;
      for (int i=0; i<6; i++) strip.setPixelColor(random(strip.numPixels() + 1), color);
      for (int i=0; i<6; i++) strip.setPixelColor(random(strip.numPixels() + 1), strip.Color(0, 0, 0));
      strip.show();
      delay(70);
    }
    togglePrices();
  }
}
