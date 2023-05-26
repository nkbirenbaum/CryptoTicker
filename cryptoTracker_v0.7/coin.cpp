// coin.cpp
#include "coin.h"

Coin::Coin(char* urlPriceIn, char* urlPingIn, char* coinNameIn) {
  urlPrice = urlPriceIn;
  urlPing = urlPingIn;
  coinName = coinNameIn;
}

// Dumb c++11 workaround, see https://stackoverflow.com/questions/8016780/undefined-reference-to-static-constexpr-char
//constexpr int bufferSize;
