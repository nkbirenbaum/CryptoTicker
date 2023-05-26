// bitmaps.cpp
#include "bitmaps.h"

// Constructor
Bitmaps::Bitmaps() {
  
}

// Dumb c++11 workaround, see https://stackoverflow.com/questions/8016780/undefined-reference-to-static-constexpr-char
constexpr bool Bitmaps::wifi4[wifiRows][wifiCols];
constexpr bool Bitmaps::wifi3[wifiRows][wifiCols];
constexpr bool Bitmaps::wifi2[wifiRows][wifiCols];
constexpr bool Bitmaps::wifi1[wifiRows][wifiCols];
constexpr bool Bitmaps::wifi0[wifiRows][wifiCols];
