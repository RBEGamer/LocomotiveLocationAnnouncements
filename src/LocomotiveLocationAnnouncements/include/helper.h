#ifndef __helper__
#define __helper__



#include "Arduino.h"
#include "SD.h"
#include "Wire.h"


void printDirectory(File dir, int numTabs);
void i2c_scanner();
#endif