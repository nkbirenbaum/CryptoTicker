# CryptoTracker
Cryptocurrency tracker using ESP32 and OLED display

# To-do list
- Adjust for daylight savings
- Shade missing Wifi status pieces
- Add server ping if price is not correct
- Display available networks: https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
- Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/
- Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s
- Improve security: remove "client.setInsecure();" and use long-lasting certificate
- Encrypt flash storage of ssid/password: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html#nvs-encryption

# Parts
+ ESP32: [HiLetgo ESP-WROOM-32 ESP32 ESP-32S Development Board 2.4GHz Dual-Mode WiFi + Bluetooth Dual Cores Microcontroller Processor Integrated with Antenna RF AMP Filter AP STA for Arduino IDE](https://www.amazon.com/gp/product/B0718T232Z/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
+ OLED screen: [1.5inch RGB OLED Display Module 128x128 16-bit High Color SPI Interface SSD1351 Driver Raspberry Pi/Jetson Nano Examples Provided](https://www.amazon.com/gp/product/B07D9NVJPZ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
