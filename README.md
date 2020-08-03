# magnatic-esp
ESP8266 / ESP32 library

Turn your ESP into a light-weight server with the following functionalities:
* HTTP-server with optional basic-authentication
* FTP-server
* NTP-client
* Separate configuration pages for users, super-users and administrators, all protected by basic-authentication
* OTA (web-based and ArduinoOTA)
* mDNS-publication of name and OTA-service
* Light-weight template-engine that allows you to use configuration-items as variables and easily expose c++-variables for usage in templates

For deployment on an ESP8266 you'll need the following libraries:
* ESP8266WiFi
* ESP8266HTTPClient
* ESP8266WebServer
* ESP8266mDNS
* ESP8266HTTPUpdateServer
* ESP8266httpUpdate

For an ESP32 you'll need these:
* WiFi
* HTTPClient
* WebServer
* ESPmDNS
* SPIFFS
* HTTPUpdate

Both need:
* WiFiUDP
* ArduinoOTA
* EEPROM
* ESP8266FtpServer
* ezTime

