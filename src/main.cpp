#include <Arduino.h>
#include <FastLED.h>
#include <math.h>

#define LED_PIN 27
#define NUM_LEDS 8

CRGB leds[NUM_LEDS];

// --- Переменные для эффектов ---
uint8_t wait_pos = 0;
int8_t wait_dir = 1;
uint32_t wait_prevMs = 0;
const uint16_t wait_interval = 50;

uint32_t stop_startMs = 0;
const uint16_t stop_period = 1000;

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

void setup()
{
  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, LED_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(60);
  clear_led();
  stop_startMs = millis();
}

uint8_t current_mode = 0;
uint32_t mode_prevMs = 0;

void loop()
{
  if (millis() - mode_prevMs >= 3000)
  {
    mode_prevMs = millis();
    current_mode = (current_mode + 1) % 4;
    Serial.print("Mode: ");
    Serial.println(current_mode);
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