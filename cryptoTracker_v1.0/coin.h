// coin.h
#ifndef COIN_H
#define COIN_H

class Coin {
  private:
    
  public:
    char* urlPrice;
    char* urlPing;
    char* coinName;
    double price;
    double priceChange;
    double oldPrice;
    double percentChange;
    static constexpr int bufferSize = 64;
    char priceBuffer1[bufferSize];
    char priceBuffer2[bufferSize];
    char priceBuffer3[bufferSize];
    char priceBuffer4[bufferSize];
    char timeBuffer1[bufferSize];
    char timeBuffer2[bufferSize];
    bool flagPositiveChange;
    Coin(char urlPriceIn[], char urlPingIn[], char coinNameIn[]);
};

#endif
