// v001: 	* Eerste versie. Don't forget use 4M with 2M SPIFFS during initial upload. This is guaranteed to work. More than 512K without SPIFFS might also be sufficient.

// v002: 	* FTP-server-ondersteuning. Dit vereist dat SPIFFS beschikbaar is.
//			* Ondersteuning voor opvragen willekeurige pagina's die in SPIFFS zijn opgeslagen. Gebruik FTP-server om pagina's te beheren
//			* Configuratie-parameters zijn nu configureerbaar in defineConfiguration
//			* Configuratiepagina vanaf SPIFFS
//			* Ondersteuning voor template-configuratie-variabelen

// v003:	* Update van mDNS iedere 5 minuten ipv ieder uur.
//			* mDNS devicenaam instelbaar
//			* Device-configuratie wordt nu automatisch gegenereerd ipv template-pagina vanaf SPIFFS
//			* Gebruik template-configuratie-variabelen nu met &&config.<configParameterName>&&
//			* Device-configuratie nu op /config ipv op /
//			* http-request voor / wordt nu omgezet naar /index.html
//			* NTP toegevoegd
//				- Het duurt even voordat NTP de juiste tijd levert. Tot dat moment wordt 1970-01-01 00:00 (in UTC) als tijd gehanteerd. In timezone Amsterdam is dat 1970-01-01 01:00.
//			* Mogelijkheid om variabelen te exposen voor template-engine.
// 				Voorbeeld: Gebruik &&S.currentTime&& om de huidige tijd op te vragen.
//				Ondersteunde types:
//				- &&S.<varname>&& voor string
//				- &&I.<varname>&& voor integer
//				- &&B.<varname>&& voor byte
//				- &&C.<varname>&& voor char
//				- &&L.<varname>&& voor long
//				- &&U.<varname>&& voor unsigned long
//				- &&F.<varname>&& voor float
//				- &&D.<varname>&& voor double
// 				Definieer de te exposen variabelen in exposeVariables conform het voorbeeld

// v004:	* Change to .h and .cpp file

// v005:	* Linked list for storage of configParameters and exposedVariables

// v006:	* Exposure of bool-type using &&BO.<varname>&&
//			* Auto commit to EEPROM after writeStringToEEPROM
//			* Debug warning if a configParameter cannot be found
// 			* Prevent writeStringToEEPROM to write to EEPROM-address 0. This prevents unwanted factory resets when an unknown configParameter is used.

// v007:	* getUnsignedLongFromCommand added
//			* Renamed addStringtoURL to addStringToURL

// v008:	* Moved settings from EEPROM to settings file /settings.txt
//			* Ondersteuning voor !OPTION &&B.<exposedboolean>&& en !ENDOPTION>. 
//				- Deze moeten zelfstandig op een regel staan
// 				- Alles daartussen wordt alleen getoond als de boolean true is.
//				- Nesten van <!OPTION is niet mogelijk.
//			* debugToServer toegevoegd
//			* replaceTemplateVarsWithExposedVars en replaceTemplateVarsWithConfigValues werken nu ook als de template var aan het begin van een regel staat
//			* Output van .hti-parsing heeft nu geen extra lege regels meer
//			* getGenericFilesFromServer toegevoegd. saveSetting.js wordt nu automatisch gedownload als deze niet op SPIFFS staat.
// 			* Clean-up of all EEPROM-configuation related functions

// v009:	* length of configParameter is no longer needed and thus removed from the struct
//			* NTP, FTP and webOTA can be disabled
//			* Username and password configurable for /config, /saveSetting, webOTA and FTP
//			* Password configurable for ArduinoOTA
//			* Timezone configurable for NTP
// 			* Introduced configLevels. Each with its own set of username and password. 1=User, 2=Superuser, 3=Administrator
//			* Improved usability of configuration page by disabling buttons during saving of new values and reboot of device
//			* Introduced optional basic authentication for handleNotFound (webserver)
//			* Implemented caching of configParameter values in memory
//			* magnaticDeviceType and magnaticDeviceVersion implemented
//			* configuration page now contains link to webserver with firmware releases
// 			* Device now tries to download a devicetype-specific index.hti
//			* Wifi boot order changed. First try to connect using STA. If that fails, then enable the device AP, disable STA and disable all services except webserver.
//			* Fixed serious bugs where device AP would be overloaded with 65 bytes where a maximum of 32 bytes are allowed. This resulted in non-working or unprotected device APs.
//			* Option to enable automatic OTA-updates
//			* Release number is derived from source filename
//			* Automatic check for new version in configuration page
//			* Added register-magnatic-device

// v010:	* register-magnatic-device now also sends type and release
//			* Optimized loops searching in configParameters, settings and exposedVariables
//			* Removed EEPROMLocation from configParameter-struct
// 			* ATTENTION! Use version 2.5.1 of ESP8266HTTPClient. Version 2.6.3 seems to be incompatible with ESP8266Webserver. It crashes the device after receiving small payloads from the webserver.
//			* Added getPayloadFromHttpRequest

// v011:	* Used F(...) to save dynamic memory (about 2000 bytes saved). Could be further optimized by sacrificing readability and maintainability of code in serveConfigurationPage
//			* Optimized serveConfigurationPage. Now "streaming" lines in stead of creating a large String. Lowers dynamic memory usage.
//			* esp8266Loop now checks if wifi-link to SSID is working. When not, it attempts to reconnect.
//			* fuzzy function added
//			* delay(1) added to esp8266Loop to give CPU room to handle wifi-related tasks.
//			* esp8266Setup does a better job of starting a clean Wifi-connection
//			* wifiConnect function created. Moved all relevant code from esp8266Setup into this function.
//			* ArduinoOTA-update progress displayed with dots every 5%
//			* HTI-pages are now streamed line by line.
//			* Removed old functions and variables for http-response and url-construction
//			* Published getFileFromServer
//			* Rudimentary reboot/reconnect tracking added
//			* Added writeToLog
//			* Fixed bug where device would boot-loop after entering Wifi AP-mode

// v012:	* Fixed a bug where the settings.txt could be downloaded/viewed. That should not be possible as this file contains usernames and passwords
//			* Minimalistic logrotate for writeToLog. Logfiles are rotated when the primary logfile grows to over 16 kB. One historic version is retained.
//			* Added enableOTA-switch on level 4
//			* Fixed a bug where the OPTION-tag would not work. Using a BO.<exposedvarname> is now mandatory

// v013:	* Compatibility with ESP32

// v014:	* Renamed to magnatic-esp
// 			* Register device is now a configuration option
// 			* Fixed a bug where wifiIPAddress did not reflect the actual ip-address when using the deviceAP

// Wifi libraries
#ifdef ESP8266
#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h> // ATTENTION! Use version 2.5.1 of ESP8266HTTPClient. Version 2.6.3 seems to be incompatible with ESP8266Webserver. It crashes the device after receiving small payloads from the webserver.
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#endif

#ifdef ESP32
#include <WiFi.h>
//#include <WiFiMulti.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#ifdef ESP32
#include <SPIFFS.h>
#endif

// Used to store configuration of settings
struct configParameter{
	String name;
	String defaultValue;
	int allowedFrom;
	String description;
	bool rebootNeeded;
	int configLevel;
	String currentValue;
	configParameter* pointerToNext;
};

// Used to store configuration of exposed variables
struct exposedVariable{
	void* pointer;
	String name;
	String type;
	exposedVariable* pointerToNext;
};

extern configParameter* firstConfigParameterPointer;
extern exposedVariable* firstExposedVariablePointer;

// EEPROM library and settings to be stored in EEPROM
#include <EEPROM.h>

// Some friendly names to use in defineConfiguration and serveConfigurationPage
#define AccessPointConfigPage 0
#define MainConfigPage 1
#define AllConfigPages 2

// Wifi definition
#define maxHttpResponseLength 2048
#define maxUrlLength 128

#ifdef ESP8266
//extern ESP8266WiFiMulti wiFiMulti;
extern ESP8266WebServer server;
#endif

#ifdef ESP32
//extern WiFiMulti wiFiMulti;
extern WebServer server;
#endif
extern HTTPClient http;

// OTA webpage
#ifdef ESP8266
#include <ESP8266HTTPUpdateServer.h>
extern ESP8266WebServer httpServer;
extern ESP8266HTTPUpdateServer httpUpdater;
#endif

// FTP server
#include <ESP8266FtpServer.h>
extern FtpServer ftpSrv;

// NTP and time definition
#include <ezTime.h>
extern Timezone tzNTP;

// Automatic update over http
#ifdef ESP8266
#include <ESP8266httpUpdate.h>
#endif

#ifdef ESP32
#include <HTTPUpdate.h>
#endif

// Variables needed for library functionality
extern char ssidAP[32];
extern String magnaticDeviceType;
extern String magnaticDeviceDescription;
extern int magnaticDeviceRelease;
extern int magnaticDeviceTypeIndexFileSize;
extern bool OTAInProgress;
extern String wifiIPAddress;

// Functions
void getDeviceReleaseFromSourceFile(String sourceFilename);
void debugToServer(String line);
String URLEncode(String str);

void addConfigParameter(String name, String description, String defaultValue, int allowedFrom, bool rebootNeeded, int configLevel);

void addExposedVariable(String name, String type, void* pointer);

String readStringFromSettingFile(String configName);
void readCharArrayFromSettingFile(char (&array)[32], String configName);
float readFloatFromSettingFile(String configName);
int readIntFromSettingFile(String configName);

int getIntFromCommand(String command, int startPos);
unsigned long getUnsignedLongFromCommand(String command, int startPos);

String getContentType(String filename) ;

String replaceTemplateVarsWithConfigValues(String line);
String convertExposedVariableToString(int index);
String replaceTemplateVarsWithExposedVars(String line);

void serveURI(String path);

void handleNotFound();
void handleConfig() ;
void handleSaveSetting();

void serveConfigurationPage(int pageType, int level);

void writeStringToSettingFile(String settingName, String settingValue);

int serverArgumentIndexByName(String argumentName);
String serverArgument(String argumentName);

void espSetup();
void espLoop();

int daysInMonth(int month, int year);
bool leapYear(int year);
String formatStringWithLeadingZeros(String input, int desiredLength);
void writeToLog(String line);

String getPayloadFromHttpRequest(String url);

unsigned long fuzzy(unsigned long duration, unsigned long fuzz);

void getFileFromServer(String fileName, String serverPath, int fileSizeExpected);
