#include "magnatic-esp.h"

void setup(){
	magnaticDeviceDescription="emptyESP";
	magnaticDeviceType="EE";
	magnaticDeviceTypeIndexFileSize=706;
	getDeviceReleaseFromSourceFile(__FILE__);

	espSetup();
}

void loop(){
	espLoop();
}
