# CryptoTracker
Cryptocurrency tracker using ESP32 and OLED display

![splash](https://github.com/nkbirenbaum/CryptoTicker/assets/97985843/f92da880-fd6b-416f-827e-4ddb1026d687)

![setup mode](https://github.com/nkbirenbaum/CryptoTicker/assets/97985843/7394e2b0-bdfe-4cac-81a4-d55808ce97fe)

![price display](https://github.com/nkbirenbaum/CryptoTicker/assets/97985843/45e72159-f8fd-4e66-a02b-62bec23e11af)

# To-do list
- Add ability to change coin
- Shade missing Wifi status pieces
- Add server ping if price is not retrieved correctly
- Display available networks: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/ & https://randomnerdtutorials.com/esp32-wifimulti/
- Improve security: remove "client.setInsecure();" and use long-lasting certificate
- Encrypt flash storage of ssid/password: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-encryption
- Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
- Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s

# Parts
+ ESP32: [HiLetgo ESP-WROOM-32 ESP32 ESP-32S Development Board 2.4GHz Dual-Mode WiFi + Bluetooth Dual Cores Microcontroller Processor Integrated with Antenna RF AMP Filter AP STA for Arduino IDE](https://www.amazon.com/gp/product/B0718T232Z/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
+ OLED screen: [1.5inch RGB OLED Display Module 128x128 16-bit High Color SPI Interface SSD1351 Driver Raspberry Pi/Jetson Nano Examples Provided](https://www.amazon.com/gp/product/B07D9NVJPZ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 

# Code upload instructions
- Install [Arduino Core for ESP32](https://github.com/espressif/arduino-esp32)
- Download all neccessary libraries (see code)
- Select NodeMCU-32S from ESP32 Arduino boards manager
- After clicking upload, hold reset button to right of USB once dubgger says "Connecting..." to begin upload
