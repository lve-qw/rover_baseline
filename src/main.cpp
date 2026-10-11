#include <Arduino.h>
#include <FastLED.h>
#include <math.h>

// --- Светодиоды ---
#define LED_PIN 27
#define NUM_LEDS 8
CRGB leds[NUM_LEDS];

uint8_t wait_pos = 0;
int8_t wait_dir = 1;
uint32_t wait_prevMs = 0;
const uint16_t wait_interval = 50;
uint32_t stop_startMs = 0;
const uint16_t stop_period = 1000;

// --- Моторы и драйвер ---
#define PIN_AIN1 2
#define PIN_AIN2 5
#define PIN_BIN1 12
#define PIN_BIN2 15
#define PIN_PWMA 32
#define PIN_PWMB 13

// --- Энкодеры ---
#define ENC_A1 39
#define ENC_B1 36
#define ENC_A2 34
#define ENC_B2 35

volatile long encoderTicks1 = 0;
volatile int lastState1 = 0;
volatile long encoderTicks2 = 0;
volatile int lastState2 = 0;

// --- Параметры колес ---
const float WHEEL_DIAMETER = 4.1;
const float TICKS_PER_WHEEL_REV = 4300.0; // Откалиброванное значение

// --- Прерывания энкодеров ---
void IRAM_ATTR handleEncoder1()
{
  int currentState = (digitalRead(ENC_A1) << 1) | digitalRead(ENC_B1);
  if (lastState1 != currentState)
  {
    if ((lastState1 == 0b00 && currentState == 0b01) || (lastState1 == 0b01 && currentState == 0b11) ||
        (lastState1 == 0b11 && currentState == 0b10) || (lastState1 == 0b10 && currentState == 0b00))
      encoderTicks1++;
    else
      encoderTicks1--;
    lastState1 = currentState;
  }
}

void IRAM_ATTR handleEncoder2()
{
  int currentState = (digitalRead(ENC_A2) << 1) | digitalRead(ENC_B2);
  if (lastState2 != currentState)
  {
    if ((lastState2 == 0b00 && currentState == 0b01) || (lastState2 == 0b01 && currentState == 0b11) ||
        (lastState2 == 0b11 && currentState == 0b10) || (lastState2 == 0b10 && currentState == 0b00))
      encoderTicks2++;
    else
      encoderTicks2--;
    lastState2 = currentState;
  }
}

// --- Светодиоды ---
void wait_led()
{
  if (millis() - wait_prevMs >= wait_interval)
  {
    wait_prevMs = millis();
    FastLED.clear();
    leds[wait_pos] = CRGB::Orange;
    FastLED.show();
    if (wait_pos == 0)
      wait_dir = 1;
    if (wait_pos == NUM_LEDS - 1)
      wait_dir = -1;
    wait_pos += wait_dir;
  }
}
void stop_led()
{
  uint32_t elapsed = millis() - stop_startMs;
  float phase = (elapsed % stop_period) * (2.0 * M_PI / stop_period);
  float sinValue = sin(phase);
  uint8_t brightness = (uint8_t)(100 + 100 * sinValue); // диапазон 0-200 вместо 0-255

  FastLED.clear();

  for (int i = 2; i < 6; i++)
  {
    leds[i] = CRGB(brightness, 0, 0);
  }

  for (int i = 0; i < NUM_LEDS; i++)
  {
    if (i < 2 || i >= 6)
    {
      leds[i] = CRGB(0, 0, 0);
    }
  }

  FastLED.show();
}
void fill_led(CRGB color)
{
  fill_solid(leds, NUM_LEDS, color);
  FastLED.show();
}
void clear_led()
{
  FastLED.clear();
  FastLED.show();
}

// --- Моторы ---
void stop_motors()
{
  analogWrite(PIN_PWMA, 0);
  analogWrite(PIN_PWMB, 0);
  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, LOW);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, LOW);
}

// --- Плавная езда ---
void drive_cm(float cm)
{
  encoderTicks1 = 0;
  encoderTicks2 = 0;

  digitalWrite(PIN_AIN1, LOW);
  digitalWrite(PIN_AIN2, HIGH);
  digitalWrite(PIN_BIN1, LOW);
  digitalWrite(PIN_BIN2, HIGH);

  float circumference = PI * WHEEL_DIAMETER;
  long target_ticks = (long)round((cm / circumference) * TICKS_PER_WHEEL_REV);

  const uint8_t MAX_SPEED = 120;
  const uint8_t MIN_SPEED = 30; // Минимальная скорость, чтобы моторы не заглохли раньше времени

  // Расстояния для разгона и торможения (по 20% от пути)
  long accel_dist = target_ticks * 0.2;
  long decel_dist = target_ticks * 0.3;

  // Защита от наложения фаз на коротких дистанциях
  if (accel_dist + decel_dist > target_ticks)
  {
    accel_dist = target_ticks / 2;
    decel_dist = target_ticks - accel_dist;
  }

  uint32_t startTime = millis();

  while (encoderTicks1 < target_ticks || encoderTicks2 < target_ticks)
  {
    if (millis() - startTime > 5000)
      break; // Таймаут 5 сек

    // Берем среднее значение тиков для более плавного регулирования
    long current_ticks = (encoderTicks1 + encoderTicks2) / 2;
    uint8_t current_speed = MAX_SPEED;

    if (current_ticks < accel_dist)
    {
      // Фаза разгона
      current_speed = map(current_ticks, 0, accel_dist, MIN_SPEED, MAX_SPEED);
    }
    else if (current_ticks > (target_ticks - decel_dist))
    {
      // Фаза торможения
      current_speed = map(current_ticks, target_ticks - decel_dist, target_ticks, MAX_SPEED, MIN_SPEED);
    }

    // Не даем скорости упасть ниже MIN_SPEED до самой остановки
    if (current_speed < MIN_SPEED && current_ticks < target_ticks)
    {
      current_speed = MIN_SPEED;
    }

    analogWrite(PIN_PWMA, current_speed);
    analogWrite(PIN_PWMB, current_speed);
    delay(10);
  }

  stop_motors();
}

// --- Основная логика ---
bool has_driven = false;
uint8_t current_mode = 0;
uint32_t mode_prevMs = 0;

void setup()
{
  Serial.begin(115200);

  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
  clear_led();

  pinMode(PIN_AIN1, OUTPUT);
  pinMode(PIN_AIN2, OUTPUT);
  pinMode(PIN_BIN1, OUTPUT);
  pinMode(PIN_BIN2, OUTPUT);
  pinMode(PIN_PWMA, OUTPUT);
  pinMode(PIN_PWMB, OUTPUT);
  stop_motors();

  pinMode(ENC_A1, INPUT);
  pinMode(ENC_B1, INPUT);
  pinMode(ENC_A2, INPUT);
  pinMode(ENC_B2, INPUT);
  attachInterrupt(digitalPinToInterrupt(ENC_A1), handleEncoder1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B1), handleEncoder1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_A2), handleEncoder2, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B2), handleEncoder2, CHANGE);
}

void loop()
{
  if (!has_driven)
  {
    fill_led(CRGB::Green);
    delay(1000);
    drive_cm(20.0);
    has_driven = true;
    current_mode = 0;
  }

  if (millis() - mode_prevMs >= 3000)
  {
    mode_prevMs = millis();
    current_mode = (current_mode + 1) % 4;
  }

  switch (current_mode)
  {
  case 0:
    wait_led();
    break;
  case 1:
    fill_led(CRGB::Green);
    break;
  case 2:
    clear_led();
    break;
  case 3:
    stop_led();
    break;
  }
}