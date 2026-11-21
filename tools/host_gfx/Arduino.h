#pragma once
// Adaptacion minima para renderizar Adafruit GFX real en el host, sin hardware.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
using String = std::string;
class __FlashStringHelper;
#define PROGMEM
#define pgm_read_byte(p) (*(const uint8_t*)(p))
#define pgm_read_word(p) (*(const uint16_t*)(p))
#define pgm_read_dword(p) (*(const uint32_t*)(p))
inline float radians(float grados) { return grados * 0.017453292519943295f; }
