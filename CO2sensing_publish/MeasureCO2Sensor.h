#ifndef __MEASURE_SENSOR_H__
#define __MEASURE_SENSOR_H__

#include <Wire.h>

#include <Adafruit_SCD30.h>

//このライブラリは必要か確認する必要がある
#include <stdio.h>

bool MesureCO2(float *temp, float *hum, float *co2);
void SensorSet();
void initLcd();
void setLcdCursor(int x, int y);
int writeChar(byte c);
void writeLine(char *s);

#endif
