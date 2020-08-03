# magnatic-esp

Turn your ESP into a light-weight server with the following functionalities:
* HTTP-server with optional basic-authentication
* FTP-server
* NTP-client
* Separate configuration pages for users, super-users and administrators, all protected by basic-authentication
* OTA (automatic, web-based and/or ArduinoOTA)
* mDNS-publication of name and OTA-service
* Light-weight template-engine that allows you to use configuration-items as variables, easily expose c++-variables for usage in templates and make HTML-lines optional

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

## First deployment
Connect the ESP to your machine and use the Arduino-IDE to compile and upload using the serial connection. After that you can use Arduino-OTA.

After the first deployment to your ESP, the ESP will reboot and search for a known SSID. When the default SSID "YourSSID" is not found, the ESP will revert to SoftAP-mode. The default name of that SSID will be "Magnatic-unconfigured-[MAC-ADDRESS]" with a default password "espadmin".

Use WinSCP to upload checkFirmware.js and saveSetting.js to the root of the ESP. Session settings for WinSCP:
* File protocol: FTP
* Encryption: No encryption
* Host name: 192.168.4.1
* Port number: 21
* User name: esp
* Password: esp

## Initial configuration
Connect your laptop, phone or tablet to the initial softAP SSID and go to the configuration page "http://192.168.4.1/config". The default username and password are "esp" and "esp".

Switch to the Superuser-page and change the configuration to match your SSID and the correct password. The ESP will reboot after setting the password.

Reconnect your laptop, phone or tablet to your own SSID.

Use the Arduino-IDE Serial monitor to find the IP-address assigned to the ESP. Baudrate is 115200.

Connect to `http://<esp-IP>/config` and go to the Superuser-page again. You'll find much more to configure. It is highly recommended to change the Device name and all passwords!
Go to the Administrator-page to disable all services you don't need.
  
## Default configuration items
### Superuser
*	[SSID] SSID of your wifi network
*	[SSIDPassword] Password of your wifi network
*	[APPassword] Password for access point of this device: This is the password needed to access the softAP of the ESP. This softAP will be enabled automatically when the ESP can't connect to the "SSID of your wifi network"
*	[DeviceName] Name of this device: This name will be used in mDNS. Default=Magnatic-unconfigured
*	[config1User] User for configuration page level 1: Username needed to access the user-configuration page. Default=esp
*	[config1Password] Password for configuration page level 1: Password needed to access the user-configuration page. Default=esp
*	[webOTAUser] User for OTA-webpage: Username needed to access the OTA-webpage (`http://<esp-IP>:8266/update`). Default=OTAota
*	[webOTAPassword] Password for OTA-webpage: Password needed to access the OTA-webpage. Default=OTAota
*	[arduinoOTAPassword] Password for Arduino-OTA: Password needed to Arduino OTA. Default=OTAota
*	[timeZoneNTP] Timezone for NTP: Timezone used for NTP. Default=Europe/Amsterdam

### Administrator
*	[config2User] User for configuration page level 2: Username needed to access the user-configuration page. Default=esp
*	[config2Password] Password for configuration page level 2: Password needed to access the user-configuration page. Default=esp
*	[config3User] User for configuration page level 3: Username needed to access the user-configuration page. Default=esp
*	[config3Password] Password for configuration page level 3: Password needed to access the user-configuration page. Default=esp
* [enableWebOTA] Enable OTA-webpage (y=yes, n=no). Default=y
* [enableFTP] Enable FTP-server (y=yes, n=no). Default=y
* [FTPUser] User for FTP-server: Username for FTP access. Default=esp
*	[FTPPassword] Password for FTP-server: Password for FTP access. Default=esp
*	[enableNTP] Enable NTP-client (y=yes, n=no). Default=y
*	[enableWebRootAuthentication] Enable basic authentication on webserver (y=yes, n=no). Default=n
*	[webRootUser] User for webserver: Only applicable if "Enable basic authentication on webserver" was set to y. Default=esp
*	[webRootPassword] Password for webserver: Only applicable if "Enable basic authentication on webserver" was set to y. Default=esp
*	[enableAutomaticUpdates] Enable automatic OTA updates (y=yes, n=no). Default=n. You'll need a webserver that provides these updates. See the chapter on "Automatic updates" for more information.
*	[magnaticDeviceServerURL] URL of server providing firmware updates: The base-URL of the webserver that provides OTA-updates. Default=http://192.168.1.180/magnaticDevices
*	[automaticUpdateCheckInterval] Interval for check of new firmware in hours. Default=24
*	[registerDevice] Register device on boot (n=no, other=url): If enabled the device will send an http-request with some device information to this URL. Default=http://192.168.1.180:65023/domoticz-events/register-magnatic-device.pl

### Hidden
Some configuration items are hidden from the configuration pages and can only be changed in the code or in the settings.txt
* [enableOTA] Enable Arduino OTA (y=yes, n=no). Default=y
* [nextFirmwareCheckTime] Next time to check for new firmware. Default=197101010000

## Creating your own configuration items
Use the function addConfigParameter in your code. Be sure to use the espSetup()-function first to initalize the library!
Syntax: addConfigParameter("configParameterName","Description for configuration page","Default value", allowedFrom, rebootNeeded, configLevel)
* allowFrom
  * AccessPointConfigPage: This configuration item will only be shown when the device is in softAP-mode
  * MainConfigPage: This configuration item will only be shown when the device is in normal mode
  * AllConfigPages: This configuration item will be shown in both softAP and normal mode
* rebootNeeded
  * true: The device will reboot after changing this configuration item
  * false: The device will not reboot after changing this configuration item
* configLevel
  * 1: User
  * 2: Superuser
  * 3: Administrator
  * 4: Hidden

## Using template-files
Template-files have the extension .hti and are for the most part normal HTML-files. Using the .hti-extension triggers the ESP-webserver in interpreting HTML-lines before serving them.

### Configuration item values
The current value of configuration items can be shown by using `&&config.<configItemName>&&`. `&&config.DeviceName&&` will show the name of the device.

### Exposed variables
Variables can be exposed to the template-engine be using the function addExposedVariable.
Syntax: addExposedVariable("variable name in template",type,pointer)
* type
  * "U": Unsigned long
  * "L": Long
  * "I": Int
  * "F": Float
  * "D": Double
  * "C": Char
  * "B": Byte
  * "BO": bool
  * "S": String

Example: addExposedVariables("wifiIPAddress", "S" , (void*)&wifiIPAddress)

The value of the exposed variable can be shown by using `&&<type>.<variableName>&&`. `&&S.wifiIPAddress&&` will show the IP-address of the ESP.

### Optional HTML-lines
One or more HTML-lines can be made optional based on the value of an exposed bool
```html
<!OPTION &&BO.locked&&>
This is optional HTML.
....
<!ENDOPTION>
```
If the value of the exposed variable is true then the optional HTML-lines are shown. Otherwise they are not sent to the http-client.

# Automatic updates
<TBD>
  
