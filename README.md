# CryptoTracker
Cryptocurrency tracker using ESP32 and OLED display

# To-do list
- Update reconnectWiFi() function (change to disconnected state) 
- Allow for changing credentials 
- Shade missing Wifi status pieces 
- Adjust for daylight savings 
- Improve w/ events (tutorial): https://randomnerdtutorials.com/esp32-useful-wi-fi-functions-arduino/ 
- Improve w/ events (video): https://www.youtube.com/watch?v=qwmLCWhfn9s 
- Add server ping if price is not correct 
- Add buttons for further functionality (ping server, input new credentials, etc) 
- Improve security: remove "client.setInsecure();" and use long-lasting certificate 

# Parts
+ ESP32: [HiLetgo ESP-WROOM-32 ESP32 ESP-32S Development Board 2.4GHz Dual-Mode WiFi + Bluetooth Dual Cores Microcontroller Processor Integrated with Antenna RF AMP Filter AP STA for Arduino IDE](https://www.amazon.com/gp/product/B0718T232Z/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
+ OLED screen: [1.5inch RGB OLED Display Module 128x128 16-bit High Color SPI Interface SSD1351 Driver Raspberry Pi/Jetson Nano Examples Provided](https://www.amazon.com/gp/product/B07D9NVJPZ/ref=ppx_yo_dt_b_search_asin_title?ie=UTF8&psc=1) 
