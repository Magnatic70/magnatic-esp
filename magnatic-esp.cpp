#include "magnatic-esp.h"

configParameter* firstConfigParameterPointer=0;
exposedVariable* firstExposedVariablePointer=0;

// Wifi definition
#ifdef ESP8266
//ESP8266WiFiMulti wiFiMulti;
ESP8266WebServer server(80);
#endif

#ifdef ESP32
//WiFiMulti wiFiMulti;
WebServer server(80);
#endif

HTTPClient http;

// OTA webpage
#ifdef ESP8266
ESP8266WebServer httpServer(8266);
ESP8266HTTPUpdateServer httpUpdater;
#endif

// FTP server
FtpServer ftpSrv;

// Variables needed for library functionality
char ssidAP[32];
String configLevelNames[3]={"User","Superuser","Administrator"};
String magnaticDeviceType="undefined";
String magnaticDeviceDescription="undefined";
int magnaticDeviceRelease=999999;
String magnaticDeviceServerURL;
int magnaticDeviceTypeIndexFileSize=0;
String wifiIPAddress;
unsigned long nextFirmwareCheckTime=0;
String libraryTimeESP=__TIMESTAMP__;

#define RebootUser 1
#define RebootReconnect 10000
#define RebootUndefined 3
#define RebootOTA 4
//#define RebootDomoticzNotFound 5
#define RebootWOTA 6
#define RebootUOTA 7
//#define RebootDomoticaTimeout 8

#define WifiModeStartup 0
#define WifiModeSTA 1
#define WifiModeAP 2
#define WifiModeBoth 3
int wifiMode=WifiModeStartup;

// NTP and time definition
Timezone tzNTP;
int ntpMode=timeNotSet;

#define DebugToServer 1

bool OTAInProgress=false;

String getPayloadFromHttpRequest(String url){
	String payload=F("--==error==--");
	if(WiFi.status()==WL_CONNECTED){
		//Serial.println(url);
		http.setReuse(false);
		http.begin(url);
		int httpCode = http.GET();
		if(httpCode == HTTP_CODE_OK) {
			payload = http.getString();
		}
		else{
			payload=String(httpCode)+http.getString();
			//Serial.println(payload);
		}
		http.end();
	}
	return payload;
}

void debugToServer(String line){
	if(DebugToServer==1) {
		getPayloadFromHttpRequest(readStringFromSettingFile("httpDebugServerURL")+URLEncode(line));
	}
}

void writeToLog(String line){
	File lf=SPIFFS.open("/esp.log","a");
	if(lf.size()>16384){
		lf.close();
		SPIFFS.remove("/esp.log.1");
		SPIFFS.rename("/esp.log","/esp.log.1");
		lf=SPIFFS.open("/esp.log","a");
	}
	String curTime=tzNTP.dateTime("Y-m-d H:i:s");
	lf.println(curTime+" - "+line);
	lf.close();
}

String URLEncode(String str){
    String encodedString="";
    char c;
    char code0;
    char code1;
    char code2;
    for (int i =0; i < str.length(); i++){
      c=str.charAt(i);
      if (c == ' '){
        encodedString+= '+';
      } else if (isalnum(c)){
        encodedString+=c;
      } else{
        code1=(c & 0xf)+'0';
        if ((c & 0xf) >9){
            code1=(c & 0xf) - 10 + 'A';
        }
        c=(c>>4)&0xf;
        code0=c+'0';
        if (c > 9){
            code0=c - 10 + 'A';
        }
        code2='\0';
        encodedString+='%';
        encodedString+=code0;
        encodedString+=code1;
        //encodedString+=code2;
      }
      yield();
    }
    return encodedString;
}

void addConfigParameter(String name, String description, String defaultValue, int allowedFrom, bool rebootNeeded, int configLevel){
	configParameter* pointerToNew;
	pointerToNew=(configParameter*) malloc(sizeof(configParameter));
	pointerToNew=new configParameter();
	pointerToNew->name=name;
	pointerToNew->defaultValue=defaultValue;
	pointerToNew->allowedFrom=allowedFrom;
	pointerToNew->description=description;
	pointerToNew->rebootNeeded=rebootNeeded;
	pointerToNew->configLevel=configLevel;
	pointerToNew->currentValue=F("-=Undefined=-");
	pointerToNew->pointerToNext=0;
	if(firstConfigParameterPointer==0){
		firstConfigParameterPointer=pointerToNew;
	}
	else{
		configParameter* traversePointer=firstConfigParameterPointer;
		while(traversePointer->pointerToNext>0){
			traversePointer=traversePointer->pointerToNext;
		}
		traversePointer->pointerToNext=pointerToNew;
	}
}

void addExposedVariable(String nameOfVariable, String typeOfVariable, void* pointerToVariable){
	exposedVariable* pointerToNew;
	pointerToNew=(exposedVariable*) malloc(sizeof(exposedVariable));
	pointerToNew=new exposedVariable();
	pointerToNew->pointer=pointerToVariable;
	pointerToNew->name=nameOfVariable;
	pointerToNew->type=typeOfVariable;
	pointerToNew->pointerToNext=0;
	if(firstExposedVariablePointer==0){
		firstExposedVariablePointer=pointerToNew;
	}
	else{
		exposedVariable* traversePointer=firstExposedVariablePointer;
		while(traversePointer->pointerToNext>0){
			traversePointer=traversePointer->pointerToNext;
		}
		traversePointer->pointerToNext=pointerToNew;
	}
}

String readStringFromSettingCache(String configName){
	String value;
	configParameter* traversePointer;
	traversePointer=firstConfigParameterPointer;
	while(traversePointer>0){
		if(traversePointer->name==configName){
			value=traversePointer->currentValue;
			break;
		}
		traversePointer=traversePointer->pointerToNext;
	}
	return value;
}

String readDefaultStringFromSettingCache(String configName){
	String value;
	configParameter* traversePointer;
	traversePointer=firstConfigParameterPointer;
	while(traversePointer>0){
		if(traversePointer->name==configName){
			value=traversePointer->defaultValue;
			break;
		}
		traversePointer=traversePointer->pointerToNext;
	}
	return value;
}

void saveStringToSettingCache(String configName, String currentValue){
	configParameter* traversePointer;
	traversePointer=firstConfigParameterPointer;
	while(traversePointer>0){
		if(traversePointer->name==configName){
			traversePointer->currentValue=currentValue;
			break;
		}
		traversePointer=traversePointer->pointerToNext;
	}
}

String readStringFromSettingFile(String configName){
	String value=readStringFromSettingCache(configName);
	if(value==F("-=Undefined=-")){
		File sf=SPIFFS.open(F("/settings.txt"),"r");
		while(sf.available()){
			String line=sf.readStringUntil('\r');
			int posOfEqual=line.indexOf("=");
			if(line.substring(0,posOfEqual)==configName){
				value=line.substring(posOfEqual+1);
				saveStringToSettingCache(configName,value);
				break;
			}
		}
		sf.close();
		if(value==F("-=Undefined=-")){ // No value found in setting file, set to default
			configParameter* traversePointer;
			traversePointer=firstConfigParameterPointer;
			while(traversePointer>0){
				if(traversePointer->name==configName){
					value=traversePointer->defaultValue;
					saveStringToSettingCache(configName,value);
					break;
				}
				traversePointer=traversePointer->pointerToNext;
			}
		}
	}
	return value;
}		

void readCharArrayFromSettingFile(char (&array)[32], String configName){
	String value=readStringFromSettingFile(configName).substring(0,32);
	value.toCharArray(array,value.length()+1);
}

float readFloatFromSettingFile(String configName){
	return readStringFromSettingFile(configName).toFloat();
}

int readIntFromSettingFile(String configName){
	return readStringFromSettingFile(configName).toInt();
}

int getIntFromCommand(String command, int startPos){
	int value=0;
	int pos=startPos;
	if(command[pos]=='-'){
		pos++;
	}
	while(pos<command.length()){
		value=value*10;
		value=value+command[pos]-48;
		pos++;
	}
	if(command[startPos]=='-'){
		value=value*-1;
	}
	return value;
}

unsigned long getUnsignedLongFromCommand(String command, int startPos){
	unsigned long value=0;
	int pos=startPos;
	while(pos<command.length()){
		value=value*10;
		value=value+command[pos]-48;
		pos++;
	}
	return value;
}

String getContentType(String filename) {
	if (server.hasArg("download")) {
		return F("application/octet-stream");
	}
	else if (filename.endsWith(".htm")) {
		return F("text/html");
	}
	else if (filename.endsWith(".html")) {
		return F("text/html");
	}
	else if (filename.endsWith(".css")) {
		return F("text/css");
	}
	else if (filename.endsWith(".js")) {
		return F("application/javascript");
	}
	else if (filename.endsWith(".png")) {
		return F("image/png");
	}
	else if (filename.endsWith(".gif")) {
		return F("image/gif");
	}
	else if (filename.endsWith(".jpg")) {
		return F("image/jpeg");
	}
	else if (filename.endsWith(".ico")) {
		return F("image/x-icon");
	}
	else if (filename.endsWith(".xml")) {
		return F("text/xml");
	}
	else if (filename.endsWith(".pdf")) {
		return F("application/x-pdf");
	}
	else if (filename.endsWith(".zip")) {
		return F("application/x-zip");
	}
	else if (filename.endsWith(".gz")) {
		return F("application/x-gzip");
	}
	else{
		return F("text/plain");
	}
}

String replaceTemplateVarsWithConfigValues(String line){
	if(line.indexOf("&&")>=0){
		configParameter* traversePointer;
		traversePointer=firstConfigParameterPointer;
		while(traversePointer>0){
			if(line.indexOf("&&config."+traversePointer->name+"&&")>=0){
				line.replace("&&config."+traversePointer->name+"&&",readStringFromSettingFile(traversePointer->name));
			}
			traversePointer=traversePointer->pointerToNext;
		}
	}
	return line;
}

String convertExposedVariableToString(exposedVariable* pointer){
	String result;
	if(pointer->type=="U"){
		result=String(*(unsigned long *)pointer->pointer);
	}
	if(pointer->type=="L"){
		result=String(*(long *)pointer->pointer);
	}
	if(pointer->type=="I"){
		result=String(*(int *)pointer->pointer);
	}
	if(pointer->type=="F"){
		result=String(*(float *)pointer->pointer);
	}
	if(pointer->type=="D"){
		result=String(*(double *)pointer->pointer);
	}
	if(pointer->type=="C"){
		result=String(*(char *)pointer->pointer);
	}
	if(pointer->type=="B"){
		result=String(*(byte *)pointer->pointer);
	}
	if(pointer->type=="BO"){
		result=String(*(bool *)pointer->pointer);
	}
	if(pointer->type=="S"){
		result=*(String *)pointer->pointer;
	}
	return result;
}

String replaceTemplateVarsWithExposedVars(String line){
	if(line.indexOf("&&")>=0){
		exposedVariable* traversePointer;
		traversePointer=firstExposedVariablePointer;
		while(traversePointer>0){
			if(line.indexOf("&&"+traversePointer->type+"."+traversePointer->name+"&&")>=0){
				line.replace("&&"+traversePointer->type+"."+traversePointer->name+"&&",convertExposedVariableToString(traversePointer));
			}
			traversePointer=traversePointer->pointerToNext;
		}
	}
	return line;
}

bool getBooleanValueOfExposedVar(String var){
	return replaceTemplateVarsWithExposedVars(var)=="1";
}

void parseAndServeTemplate(String path){
	WiFiClient thisClient=server.client();

	bool showLines=true;

	// Send header
	thisClient.println(F("HTTP/1.1 200 OK"));
	thisClient.println(F("Content-type:text/html"));
	thisClient.println(F("Connection: close"));
	thisClient.println();

	File f=SPIFFS.open(path,"r");
	while(f.available()){
		String line=f.readStringUntil('\n');
		if(line.indexOf("<!OPTION &&BO.")>=0){
			int firstIndex=line.indexOf("&&");
			int lastIndex=line.lastIndexOf("&&");
			showLines=getBooleanValueOfExposedVar(line.substring(firstIndex,lastIndex+2));
		}
		else if(line.indexOf("<!ENDOPTION>")>=0){
			showLines=true;
		}
		else{
			if(showLines){
				line=replaceTemplateVarsWithConfigValues(line);
				line=replaceTemplateVarsWithExposedVars(line);
			}
		}
		if(showLines){
			thisClient.println(line);
		}
	}
	thisClient.println();
	thisClient.stop();
}

void serveURI(String path){
	if(path.endsWith(".hti")){
		parseAndServeTemplate(path);
	}
	else{
		File f=SPIFFS.open(path,"r");
		server.streamFile(f,getContentType(path));
		f.close();
	}
}

void handleNotFound(){
	char username[32];
	char password[32];
	if(server.uri()!="/saveSetting.js" && server.uri()!="/default.css" && readStringFromSettingFile("enableWebRootAuthentication")=="y"){
		readCharArrayFromSettingFile(username,"webRootUser");
		readCharArrayFromSettingFile(password,"webRootPassword");
	}
	if (server.uri()!="/saveSetting.js" && server.uri()!="/default.css" && readStringFromSettingFile("enableWebRootAuthentication")=="y" && !server.authenticate(username,password)) {
		return server.requestAuthentication();
	}
	else{
		String uri=server.uri();
		if(uri=="/"){
			if(SPIFFS.exists(F("/index.hti"))){
				uri=F("/index.hti");
			}
			else{
				uri=F("/index.html");
			}
		}
		if(SPIFFS.exists(uri) && uri!="/settings.txt"){ // Block settings.txt from being downloaded/viewed. This file contains usernames and passwords.
			serveURI(uri);
		}
		else{
			String message = F("File Not Found\n\n");
			message += F("URI: ");
			message += uri;
			message += F("\nMethod: ");
			message += (server.method() == HTTP_GET)?F("GET"):F("POST");
			message += F("\nArguments: ");
			message += server.args();
			message += F("\n");
			for (uint8_t i=0; i<server.args(); i++){
				message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
			}
			server.send(404, F("text/plain"), message);
		}
	}
}

String formatStringWithLeadingZeros(String input, int desiredLength){
	while(input.length()<desiredLength){
		input="0"+input;
	}
	return input;
}

String getCurrentTimeString(){
	return tzNTP.dateTime("YmdHi");
}

bool leapYear(int year){
	bool leap=false;
	if(year%4==0){
		leap=true;
		if(year%100==0){
			leap=false;
			if(year%400==0){
				leap=true;
			}
		}
	}
	return leap;
}

int daysInMonth(int month, int year){
	int days=31;
	if(month==2){
		if(leapYear(year)){
			days=29;
		}
		else{
			days=28;
		}
	}
	else if(month==4 || month==6 || month==9 || month==11){
		days=30;
	}
	return days;
}

String addHoursToCurrentTime(int addHours){
	if(addHours>24){ // TBD: Adding more hours might break this function!
		addHours=24;
	}
	int year=tzNTP.dateTime("Y").toInt();
	int month=tzNTP.dateTime("m").toInt();
	int day=tzNTP.dateTime("d").toInt();
	int hour=tzNTP.dateTime("H").toInt();
	int minute=tzNTP.dateTime("i").toInt();
	hour+=addHours;
	while(hour>23){
		hour-=24;
		day++;
	}
	while(hour<0){
		hour=23;
		day--;
	}
	while(day>daysInMonth(month,year)){
		day=1;
		month++;
	}
	while(day<1){
		month--;
		if(month<1){
			month=12;
			year--;
		}
		day=daysInMonth(month,year);
	}
	while(month>12){
		month=1;
		year++;
	}
	while(month<1){
		month=12;
		year--;
	}
	if(year>9999){
		year=9999;
	}
	if(year<1900){
		year=1900;
	}
	return String(year)+formatStringWithLeadingZeros(String(month),2)+formatStringWithLeadingZeros(String(day),2)+formatStringWithLeadingZeros(String(hour),2)+formatStringWithLeadingZeros(String(minute),2);
}

void checkForNewFirmware(bool immediately){
	if(immediately || getCurrentTimeString().compareTo(readStringFromSettingFile("nextFirmwareCheckTime"))>=0){
		if(immediately){
			EEPROM.write(1,RebootUOTA);
			EEPROM.commit();
			writeToLog("User initiated OTA.");
		}
		else{
			EEPROM.write(1,RebootWOTA);
			EEPROM.commit();
			//writeToLog("Check due to automatic web-based OTA.");
		}
		Serial.println("Current time "+getCurrentTimeString()+" is after scheduled time "+readStringFromSettingFile("nextFirmwareCheckTime"));
		writeStringToSettingFile("nextFirmwareCheckTime",addHoursToCurrentTime(readStringFromSettingFile("automaticUpdateCheckInterval").toInt()));
		int nextReleaseNumber=magnaticDeviceRelease+1;
		String nextFirmwareURL=magnaticDeviceServerURL+"/"+magnaticDeviceType+"/"+magnaticDeviceType+formatStringWithLeadingZeros(String(nextReleaseNumber),3)+".bin";
		Serial.println("Trying to find "+nextFirmwareURL);
		WiFiClient client;
#ifdef ESP8266
		t_httpUpdate_return ret = ESPhttpUpdate.update(client, nextFirmwareURL);
#endif
#ifdef ESP32
		t_httpUpdate_return ret = httpUpdate.update(client, nextFirmwareURL);
#endif
		switch (ret) {
			case HTTP_UPDATE_FAILED:
#ifdef ESP8266
				Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\r\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
#endif
#ifdef ESP32
				Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\r\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
#endif
				break;

			case HTTP_UPDATE_NO_UPDATES:
				Serial.println(F("HTTP_UPDATE_NO_UPDATES"));
				break;

			case HTTP_UPDATE_OK:
				Serial.println(F("HTTP_UPDATE_OK"));
				break;
		}
		EEPROM.write(1,RebootUndefined);
		EEPROM.commit();
	}
}

void serveConfigurationPage(int pageType, int level){
	int nextReleaseNumber=magnaticDeviceRelease+1;
	String nextFirmwareURL=magnaticDeviceServerURL+"/"+magnaticDeviceType+"/"+magnaticDeviceType+formatStringWithLeadingZeros(String(nextReleaseNumber),3)+".bin";
	
	WiFiClient thisClient=server.client();

	// Send header
	thisClient.println(F("HTTP/1.1 200 OK"));
	thisClient.println(F("Content-type:text/html"));
	thisClient.println(F("Connection: close"));
	thisClient.println();


	thisClient.println(F("<html>"));
	thisClient.println(F("<head>"));
	thisClient.println(		"<title>"+readStringFromSettingFile("DeviceName")+" configuration</title>");
	thisClient.println(F("<script src=\"saveSetting.js\"></script>"));
	thisClient.println(F("<script src=\"checkFirmware.js\"></script>"));
	thisClient.println(F("</head>"));
	thisClient.println("<body onload=\"checkForNewFirmware('"+nextFirmwareURL+"')\">"
							"Device type: <a href=\""+magnaticDeviceServerURL+"/"+magnaticDeviceType+"\" target=\"firmwareDownload\">"+magnaticDeviceDescription+", release "+magnaticDeviceType+formatStringWithLeadingZeros(String(magnaticDeviceRelease),3)+"</a><p>"
							"Device IP: "+WiFi.localIP().toString()+"<p>");
	thisClient.println(F("<table>"));
	configParameter* traversePointer;
	traversePointer=firstConfigParameterPointer;
	while(traversePointer>0){
		if((traversePointer->allowedFrom==pageType || traversePointer->allowedFrom==AllConfigPages) && traversePointer->configLevel==level){
			thisClient.println(F("<tr>"));
			thisClient.println(		"<td>"+traversePointer->description+": </td>");
			thisClient.println(F(	"<td>"));
			thisClient.println(			"<input id=\""+traversePointer->name+"\" type=\"text\" value=\""+readStringFromSettingFile(traversePointer->name)+"\">"
										"<input type=\"button\" value=\"Apply\" onclick=\"saveSetting('"+traversePointer->name+"',"+String(level)+")\">"
							);
			if(traversePointer->rebootNeeded){
				thisClient.println	(F("&nbsp;Device will reboot after applying this setting"));
			}
			thisClient.println(F("</td>"));
			thisClient.println(F("</tr>"));
		}
		traversePointer=traversePointer->pointerToNext;
	}
	thisClient.println(			F("</table><p>"));
	if(level==1){
		thisClient.println(				configLevelNames[0]+" - ");
		thisClient.println(				"<a href=\"/config?level=2\">"+configLevelNames[1]+"</a> - ");
		thisClient.println(				"<a href=\"/config?level=3\">"+configLevelNames[2]+"</a>");
	}
	if(level==2){
		thisClient.println(				"<a href=\"/config?level=1\">"+configLevelNames[0]+"</a> - ");
		thisClient.println(				configLevelNames[1]+" - ");
		thisClient.println(				"<a href=\"/config?level=3\">"+configLevelNames[2]+"</a>");
	}
	if(level==3){
		thisClient.println(				"<a href=\"/config?level=1\">"+configLevelNames[0]+"</a> - ");
		thisClient.println(				"<a href=\"/config?level=2\">"+configLevelNames[1]+"</a> - ");
		thisClient.println(				configLevelNames[2]);
	}
	thisClient.println(					F("<div id=\"updateButton\" style=\"visibility:hidden\"><input type=\"button\" value=\"New firmware available. Update now.\" onclick=\"updateNow()\"></div>"));
	thisClient.println(					F("<div id=\"noUpdateAvailable\" style=\"visibility:hidden\">Your device has been updated to the latest firmware.</div>"));
	thisClient.println(		F("</body>"));
	thisClient.println(	F("</html>"));
	thisClient.println();
	thisClient.stop();
}

void handleConfig(){
	char username[32];
	char password[32];
	String level=serverArgument("level");
	if(level.toInt()<1 || level.toInt()>3){
		level="1";
	}
	readCharArrayFromSettingFile(username,"config"+level+"User");
	readCharArrayFromSettingFile(password,"config"+level+"Password");
	if (!server.authenticate(username,password)) {
		return server.requestAuthentication();
	}
	else{
		if(server.client().localIP()==WiFi.localIP()){ // Called from private access point
			serveConfigurationPage(MainConfigPage,level.toInt());
		}
		else{ // Called from device access point
			serveConfigurationPage(AccessPointConfigPage,level.toInt());
		}
	}
}

void handleUpdateNow(){
	char username[32];
	char password[32];
	readCharArrayFromSettingFile(username,"config2User");
	readCharArrayFromSettingFile(password,"config2Password");
	if (!server.authenticate(username,password)) {
		return server.requestAuthentication();
	}
	else{
		if(server.client().localIP()==WiFi.localIP()){ // Called from private access point
			server.send(200,F("text/html"),F("OK"));
			checkForNewFirmware(true);
		}
		else{ // Called from device access point
			server.send(200,F("text/html"),F("NOK"));
		}
	}
}

int serverArgumentIndexByName(String argumentName){
	int index=0;
	for(int i=0;i<server.args();i++){
		if(server.argName(i)==argumentName){
			index=i;
			break;
		}
	}
	return index;
}

String serverArgument(String argumentName){
	return server.arg(serverArgumentIndexByName(argumentName));
}

void handleSaveSetting(){
	char username[32];
	char password[32];
	readCharArrayFromSettingFile(username,"config"+serverArgument("level")+"User");
	readCharArrayFromSettingFile(password,"config"+serverArgument("level")+"Password");
	if (!server.authenticate(username,password)) {
		return server.requestAuthentication();
	}
	else{
		int settingIndex=serverArgumentIndexByName("setting");
		bool done=false;
		configParameter* traversePointer;
		traversePointer=firstConfigParameterPointer;
		while(traversePointer>0){
			if(server.arg(settingIndex)==traversePointer->name){
				if(server.client().localIP()==WiFi.localIP() && (traversePointer->allowedFrom==MainConfigPage || traversePointer->allowedFrom==AllConfigPages)){
					Serial.println(F("From network of SSID"));
					Serial.println("Setting "+traversePointer->name+" to "+server.arg(serverArgumentIndexByName("value")));
					writeStringToSettingFile(traversePointer->name,server.arg(serverArgumentIndexByName("value")));
					done=true;
				}
				if(server.client().localIP()!=WiFi.localIP() && (traversePointer->allowedFrom==AccessPointConfigPage || traversePointer->allowedFrom==AllConfigPages)){
					Serial.println(F("From network of device access point"));
					Serial.println("Setting "+traversePointer->name+" to "+server.arg(serverArgumentIndexByName("value")));
					writeStringToSettingFile(traversePointer->name,server.arg(serverArgumentIndexByName("value")));
					done=true;
				}
				break;
			}
			traversePointer=traversePointer->pointerToNext;
		}
		if(done){
			server.send(200,F("text/plain"),F("OK"));
			// For some settings a reboot is needed
			traversePointer=firstConfigParameterPointer;
			while(traversePointer>0){
				if(server.arg(settingIndex)==traversePointer->name && traversePointer->rebootNeeded){
					server.close();
					delay(500);
					EEPROM.write(1,RebootUser);
					EEPROM.commit();
					writeToLog("Configuration change by user.");
					ESP.restart();
				}
				traversePointer=traversePointer->pointerToNext;
			}
		}
		else{
			server.send(200,F("text/plain"),F("NOK. Unknown setting or not allowed from this network."));
		}
	}
}

void getFileFromServer(String fileName, String serverPath, int fileSizeExpected){
	int fileSize;
	if(!SPIFFS.exists(fileName)){
		fileSize=0;
	}
	else{
		File f=SPIFFS.open(fileName,"r");
		fileSize=f.size();
		f.close();
	}
	if(fileSize!=fileSizeExpected){
		// TBD: 404 error handling needed!
		String payload=getPayloadFromHttpRequest(readStringFromSettingFile("httpFileServerURL")+serverPath+fileName);
		if(payload!="--==error==--"){
			SPIFFS.remove(fileName);
			File nf=SPIFFS.open(fileName,"w");
			nf.print(payload);
			nf.close();
		}
	}
}

void getGenericFilesFromServer(){
	getFileFromServer(F("/saveSetting.js"),"",1132);
	getFileFromServer(F("/checkFirmware.js"),"",1093);
	getFileFromServer(F("/index.hti"),"/"+magnaticDeviceType,magnaticDeviceTypeIndexFileSize);
}

void getDeviceReleaseFromSourceFile(String sourceFilename){
	int dotIndex=sourceFilename.lastIndexOf(".");
	magnaticDeviceRelease=sourceFilename.substring(dotIndex-3,dotIndex).toInt();
}

void registerMagnaticDevice(int reason){
	Serial.println(getPayloadFromHttpRequest(readStringFromSettingFile("registerDevice")+"?name="+URLEncode(readStringFromSettingFile("DeviceName"))+"&ip="+WiFi.localIP().toString()+"&type="+magnaticDeviceType+"&version="+magnaticDeviceRelease+"&reason="+String(reason)));
}

void wifiConnect(int reason){
	// Wifi setup
	byte mac[6];
	WiFi.macAddress(mac);
	
	String ssidString=readStringFromSettingFile("DeviceName")+"-"+String(mac[5],HEX)+String(mac[4],HEX)+String(mac[3],HEX)+String(mac[2],HEX)+String(mac[1],HEX)+String(mac[0],HEX);
	if(ssidString.length()>32){
		ssidString=ssidString.substring(0,32);
	}
	ssidString.toCharArray(ssidAP,ssidString.length());
	char aPPassword[32];
	readCharArrayFromSettingFile(aPPassword,"APPassword");
	
	// Proper reset of Wifi
	WiFi.mode(WIFI_OFF);
	WiFi.persistent(false);   // Solve possible wifi init errors (re-add at 6.2.1.16 #4044, #4083)
	WiFi.disconnect(true);    // Delete SDK wifi config
	delay(200);

	wifiMode=WifiModeSTA;
	WiFi.mode(WIFI_STA);
#ifdef ESP8266
	WiFi.setSleepMode(WIFI_MODEM_SLEEP);
	WiFi.setOutputPower(20.5);
#endif
	delay(200);
	
	Serial.print(F("Connecting to: "));
	Serial.println(readStringFromSettingFile("SSID"));
	char ssid[32];
	char ssidPassword[32];
	//writeStringToSettingFile("SSID","ThuisDN");
	readCharArrayFromSettingFile(ssid,"SSID");
	readCharArrayFromSettingFile(ssidPassword,"SSIDPassword");
	WiFi.begin(ssid,ssidPassword); // Set ssid and password for internet-access
	int tries=0;
	while (WiFi.status() != WL_CONNECTED && tries++<25) {
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print(F("IP address: "));
	Serial.println(WiFi.localIP());
	wifiIPAddress=WiFi.localIP().toString();
	
	if(WiFi.status() == WL_CONNECTED){
		Serial.println(F("Connected to STA"));
		if(readStringFromSettingFile("registerDevice").startsWith("http://")){
			registerMagnaticDevice(reason);
		}
	}
	else{
		Serial.println(F("STA not found, disabling STA-mode, enabling AP-mode"));
		writeToLog("STA not found. Entering Wifi AP-mode");
		WiFi.mode(WIFI_OFF);
		WiFi.disconnect();
		delay(200);
		wifiMode=WifiModeAP;
		WiFi.mode(WIFI_AP);
		WiFi.softAP(ssidAP, aPPassword);
		IPAddress myIPAP = WiFi.softAPIP(); // Set ssid and password for local access
		wifiIPAddress=WiFi.softAPIP().toString();
		Serial.print(F("Device AP name: "));
		Serial.println(ssidAP);
		Serial.print(F("AP Password: "));
		Serial.println(aPPassword);
		Serial.print(F("AP IP address: "));
		Serial.println(myIPAP);
	}
}

void espSetup(){
	Serial.begin(115200);
	Serial.println(F("Program started"));
	
#ifdef ESP8266
	SPIFFS.begin();
#endif
#ifdef ESP32
	SPIFFS.begin(true); // true=format SPIFFS if failed
#endif
	writeToLog("Start of bootsequence at "+String(millis())+"ms");
	
	// EEPROM setup
	EEPROM.begin(512);
	
	addConfigParameter(F("SSID"),F("SSID of your wifi network"),F("YourSSID"),AllConfigPages,false,2);
	addConfigParameter(F("SSIDPassword"),F("Password of your wifi network"),F("YourPassword"),AllConfigPages,true,2);
	addConfigParameter(F("APPassword"),F("Password for access point of this device"),F("espadmin"),MainConfigPage,true,2);
	addConfigParameter(F("DeviceName"),F("Name of this device"),F("Magnatic-unconfigured"),MainConfigPage,true,2);
	addConfigParameter(F("config1User"),F("User for configuration page level 1"),F("esp"),MainConfigPage,false,2);	
	addConfigParameter(F("config1Password"),F("Password for configuration page level 1"),F("esp"),MainConfigPage,false,2);
	addConfigParameter(F("config2User"),F("User for configuration page level 2"),F("esp"),MainConfigPage,false,3);	
	addConfigParameter(F("config2Password"),F("Password for configuration page level 2"),F("esp"),MainConfigPage,false,3);
	addConfigParameter(F("config3User"),F("User for configuration page level 3"),F("esp"),MainConfigPage,false,3);	
	addConfigParameter(F("config3Password"),F("Password for configuration page level 3"),F("esp"),MainConfigPage,false,3);
	addConfigParameter(F("enableWebOTA"),F("Enable OTA-webpage (y=yes, n=no)"),F("y"),MainConfigPage,true,3);
	addConfigParameter(F("enableOTA"),F("Enable Arduino OTA (y=yes, n=no)"),F("y"),MainConfigPage,true,4);
	addConfigParameter(F("webOTAUser"),F("User for OTA-webpage"),F("OTAota"),MainConfigPage,true,2);
	addConfigParameter(F("webOTAPassword"),F("Password for OTA-webpage"),F("OTAota"),MainConfigPage,true,2);
	addConfigParameter(F("arduinoOTAPassword"),F("Password for Arduino-OTA"),F("OTAota"),MainConfigPage,true,2);
	addConfigParameter(F("enableFTP"),F("Enable FTP-server (y=yes, n=no)"),F("y"),MainConfigPage,true,3);
	addConfigParameter(F("FTPUser"),F("User for FTP-server"),F("esp"),MainConfigPage,true,3);
	addConfigParameter(F("FTPPassword"),F("Password for FTP-server"),F("esp"),MainConfigPage,true,3);
	addConfigParameter(F("enableNTP"),F("Enable NTP-client (y=yes, n=no)"),F("y"),MainConfigPage,true,3);
	addConfigParameter(F("timeZoneNTP"),F("Timezone for NTP"),F("Europe/Amsterdam"),MainConfigPage,true,2);
	addConfigParameter(F("enableWebRootAuthentication"),F("Enable basic authentication on webserver (y=yes, n=no)"),F("n"),MainConfigPage,false,3);
	addConfigParameter(F("webRootUser"),F("User for webserver"),F("esp"),MainConfigPage,false,3);
	addConfigParameter(F("webRootPassword"),F("Password for webserver"),F("esp"),MainConfigPage,false,3);
	addConfigParameter(F("enableAutomaticUpdates"),F("Enable automatic OTA updates (y=yes, n=no)"),F("n"),MainConfigPage,false,3);
	addConfigParameter(F("magnaticDeviceServerURL"),F("URL of server providing firmware updates"),F("http://192.168.1.180/magnaticDevices"),MainConfigPage,false,3);
	addConfigParameter(F("automaticUpdateCheckInterval"),F("Interval for check of new firmware in hours"),F("24"),MainConfigPage,false,3);
	addConfigParameter(F("nextFirmwareCheckTime"),F("Next time to check for new firmware"),F("197101010000"),MainConfigPage,false,4);
	addConfigParameter(F("registerDevice"),F("Register device on boot (n=no, other=url)"),F("http://192.168.1.180:65023/domoticz-events/register-magnatic-device.pl"),MainConfigPage,false,3);
	addConfigParameter(F("httpFileServerURL"),F("URL of http file server"),F("http://192.168.1.180/magnaticDevices"),MainConfigPage,false,3);
	addConfigParameter(F("httpDebugServerURL"),F("URL of http debug server"),F("http://192.168.1.180:65023/rpss/rpss_service.pl?what=espdebug&line="),MainConfigPage,false,3);
	
	magnaticDeviceServerURL=readStringFromSettingFile("httpFileServerURL");
	magnaticDeviceServerURL=readStringFromSettingFile("magnaticDeviceServerURL");
	
	addExposedVariable(F("magnaticDeviceType"),F("S"),(void*)&magnaticDeviceType);
	addExposedVariable(F("magnaticDeviceDescription"),F("S"),(void*)&magnaticDeviceDescription);
	addExposedVariable(F("magnaticDeviceRelease"),F("I"),(void*)&magnaticDeviceRelease);
	addExposedVariable(F("wifiIPAddress"),F("S"),(void*)&wifiIPAddress);
	addExposedVariable(F("firmwareWebserver"),F("S"),(void*)&magnaticDeviceServerURL);
	
	wifiConnect(EEPROM.read(1));
	
	getGenericFilesFromServer();

	// Webserver and configuration page
	server.on("/config", handleConfig);
	server.on("/saveSetting",handleSaveSetting);
	server.on("/updateNow",handleUpdateNow);
	server.on("/sourceinfo/esp",[](){server.send(200,"text/plain",libraryTimeESP);});
	server.onNotFound(handleNotFound);
	server.begin();
	Serial.println(F("HTTP server started"));
	
	// mDNS
	if(WiFi.status() == WL_CONNECTED){
#ifdef ESP8266
		MDNS.begin(readStringFromSettingFile("DeviceName"),WiFi.localIP(),5);
#endif
#ifdef ESP32
		MDNS.begin(readStringFromSettingFile("DeviceName").c_str());
#endif
	}
	
	// OTA
	if(WiFi.status() == WL_CONNECTED){
#ifdef ESP8266
		ArduinoOTA.setPort(8266);
#endif
#ifdef ESP32
		ArduinoOTA.setPort(3232);
#endif
		char arduinoOTAPassword[32];
		readCharArrayFromSettingFile(arduinoOTAPassword,"arduinoOTAPassword");
		ArduinoOTA.setPassword(arduinoOTAPassword);
		
		ArduinoOTA.onStart([]() {
			String type;
			if (ArduinoOTA.getCommand() == U_FLASH) {
				type = F("sketch");
			} else { // U_SPIFFS
				type = F("filesystem");
			}

			// NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
			Serial.println("Start updating " + type);
			OTAInProgress=true;
		});
		ArduinoOTA.onEnd([]() {
			Serial.println(F("\r\nEnd"));
			EEPROM.write(0,0);
			EEPROM.write(1,RebootOTA);
			EEPROM.commit();
			writeToLog("Arduino OTA");
		});
		ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
			int perc=progress / (total / 100);
			if(perc%5==0){
				Serial.print('.');
			}
		});
		ArduinoOTA.onError([](ota_error_t error) {
			Serial.printf("Error[%u]: ", error);
			if (error == OTA_AUTH_ERROR) {
				Serial.println(F("Auth Failed"));
			} else if (error == OTA_BEGIN_ERROR) {
				Serial.println(F("Begin Failed"));
			} else if (error == OTA_CONNECT_ERROR) {
				Serial.println(F("Connect Failed"));
			} else if (error == OTA_RECEIVE_ERROR) {
				Serial.println(F("Receive Failed"));
			} else if (error == OTA_END_ERROR) {
				Serial.println(F("End Failed"));
			}
		});
		ArduinoOTA.begin();
	}
	
	// OTA-webpage
#ifdef ESP8266
	if(WiFi.status() == WL_CONNECTED && readStringFromSettingFile("enableWebOTA")=="y"){
		Serial.println(F("Enabling webOTA"));
		httpUpdater.setup(&httpServer,"/update",readStringFromSettingFile("webOTAUser"),readStringFromSettingFile("webOTAPassword"));
		httpServer.begin();
	}
#endif

	// FTP-server
	if(WiFi.status() == WL_CONNECTED && readStringFromSettingFile("enableFTP")=="y"){
		Serial.println(F("Enabling FTP"));
		if (SPIFFS.begin()) {
			Serial.println(F("SPIFFS opened!"));
			ftpSrv.begin(readStringFromSettingFile("FTPUser"),readStringFromSettingFile("FTPPassword")); // username, password for ftp. Set ports in ESP8266FtpServer.h (default 21, 50009 for PASV)
		}
	}
	
	// NTP init
	if(WiFi.status() == WL_CONNECTED && readStringFromSettingFile("enableNTP")=="y"){
		Serial.println(F("Enabling NTP"));
		tzNTP.setLocation(readStringFromSettingFile("timeZoneNTP"));
	}
	
	randomSeed(millis()); // This should be fairly random due to the time needed to get to this point. Mainly because of the connection to the SSID, but also other small factors
	EEPROM.write(1,RebootUndefined);
	EEPROM.commit();
	writeToLog("End of bootsequence at "+String(millis())+"ms");

}

void writeStringToSettingFile(String settingName, String settingValue){
	saveStringToSettingCache(settingName,settingValue);
	File sfi=SPIFFS.open(F("/settings.txt"),"r");
	File sfo=SPIFFS.open(F("/settings.tmp"),"w");
	bool done=false;
	while(sfi.available()){
		String line=sfi.readStringUntil('\r');
		int posOfEqual=line.indexOf("=");
		if(line.substring(0,posOfEqual)==settingName){
			sfo.print(settingName+"="+settingValue+"\r");
			done=true;
		}
		else{
			sfo.print(line+"\r");
		}
	}
	if(!done){
		sfo.print(settingName+"="+settingValue+"\r");
	}
	sfi.close();
	sfo.close();
	SPIFFS.remove(F("/settings.txt"));
	SPIFFS.rename(F("/settings.tmp"),F("/settings.txt"));
}

unsigned long fuzzy(unsigned long duration, unsigned long fuzz){
	duration-=fuzz;
	duration+=random(fuzz*2);
	return duration;
}

int prevWifiStatus=WL_CONNECTED;
int curWifiStatus;
unsigned long lastChangeToWifiDisconnectTime=0;

void espLoop(){
	delay(1); // Give the processor some time for Wifi-handling
	if(readStringFromSettingFile("enableNTP")=="y" && ntpMode==timeNotSet && timeStatus()==timeSet){
		writeToLog("NTP synced "+String(millis())+"ms after boot");
		ntpMode=timeSet;
	}
	curWifiStatus=WiFi.status();
	if(wifiMode==WifiModeBoth || wifiMode==WifiModeSTA){
		if(prevWifiStatus!=curWifiStatus){ 
			if(curWifiStatus != WL_CONNECTED){
				lastChangeToWifiDisconnectTime=millis();
			}
			prevWifiStatus=curWifiStatus;
		}
		if(curWifiStatus != WL_CONNECTED && curWifiStatus!=WL_IDLE_STATUS && millis()>lastChangeToWifiDisconnectTime+20000){ // Disconnected for more than 20 seconds for some reason. Try to reconnect.
			Serial.println(F("Reconnecting to access point"));
			writeToLog("Wifi-status changed to "+String(curWifiStatus)+". Restarting.");
			EEPROM.write(1,RebootReconnect+curWifiStatus);
			EEPROM.commit();
			ESP.restart();
			//wifiConnect(RebootReconnect+curWifiStatus);
			//writeToLog("Reconnection complete. Current Wifi status is "+String(WiFi.status()));
			//curWifiStatus=WiFi.status();
		}
	}
	if(curWifiStatus == WL_CONNECTED && readStringFromSettingFile("enableNTP")=="y"){
		events(); 									// NTP: Updates date and time with NTP every 30 minutes
	}
	server.handleClient(); 							// Webserver and Configuration
	if(curWifiStatus == WL_CONNECTED && readStringFromSettingFile("enableOTA")=="y"){
		ArduinoOTA.handle(); 							// Arduino-OTA
	}
#ifdef ESP8266
	if(curWifiStatus == WL_CONNECTED && readStringFromSettingFile("enableWebOTA")=="y"){
		httpServer.handleClient(); 					// OTA-webpage
	}
#endif
	if(curWifiStatus == WL_CONNECTED && readStringFromSettingFile("enableFTP")=="y"){
		ftpSrv.handleFTP(); 						// FTP-server
	}
	if(curWifiStatus == WL_CONNECTED && readStringFromSettingFile("enableNTP")=="y" && year()>1970 && readStringFromSettingFile("enableAutomaticUpdates")=="y"){ // Extra check for nextFirmwareCheckTime to prevent excessive checking during rollover of unsigned long.
		checkForNewFirmware(false);
	}
}
