 
# M5Cardputer WebRadio FR version

M5Cardputer_WebRadio needs these Librarys: 
M5Unified : https://github.com/m5stack/M5Unified 
ESP8266Audio: https://github.com/earlephilhower/ESP8266Audio

![image](https://github.com/rolandbreedveld/M5Cardputer_WebRadio_Dutch/blob/main/M5Cardputer_WebRadio_NL.jpeg)

----
WiFi Settings will be stored in EEPROM

PlatformIO IDE used. (VS code)

Difficile d'utiliser une carte SD pour le stockage des URL 
Le dilemme classique sur ESP32 :
- Wi-Fi après SPI/SD_card = Wi-Fi KO
- SPI/SD après Wi-Fi = SD KO

Pourquoi ?
Sur certaines cartes comme la M5Cardputer, l’accès SD se fait via HSPI, et le Wi-Fi utilise les DMA/interruptions du bus principal.
→ Quand SD est initialisé après Wi-Fi, le SPI bus ou les GPIO peuvent être en conflit.

Solution implémentée : 
Utilisation d'un GET HTTP pour recupérer la liste des chaines : 
web-streams comes from http://philae.synology.me/~admin/arduino/radio_dico.php

