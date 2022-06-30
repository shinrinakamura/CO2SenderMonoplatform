
#include "MeasureCO2Sensor.h"

//CO2センサーのオブジェクト
Adafruit_SCD30  scd30;

void SensorSet(){
  
  initLcd();    //ディスプレイの初期化
  Serial.println("Adafruit SCD30 test!");

  if (!scd30.begin()) { // CO2センサーの初期化
    Serial.println("Failed to find SCD30 chip");
    while (1) { delay(10); }
  }
  Serial.println("SCD30 Found!");
  
  Serial.print("Measurement Interval: ");   //CO2センサーのインターバルタイム
  Serial.print(scd30.getMeasurementInterval()); 
  Serial.println(" seconds");
}


//CO2センサの値を読みだしてディスプレイに表示する
//第一引数：温度を読み出す領域のポインタ
//第二引数：湿度を読み出す領域のポインタ
//第三引数：二酸化炭素を読み出す領域のポインタ
bool MesureCO2(float *temp, float *hum, float *co2){
    
    //CO2センサーを読み込めたか確認する
    if (scd30.dataReady()){
    Serial.println("Data available!");
    if (!scd30.read()){ Serial.println("Error reading sensor data"); return false; }

    //センサーの値の読み取り
    *temp = scd30.temperature;
    *hum = scd30.relative_humidity;
    *co2 = scd30.CO2;
        
    {
    
      char Buffer[17];

      // 上段　温度　湿度
      memset(Buffer, 0x0, 17);
      setLcdCursor(0, 0);
      writeLine(Buffer);
      sprintf( Buffer, "%3.2f C, %2.2f %%", *temp, *hum); // 表示
      setLcdCursor(0, 0);
      writeLine(Buffer);

      // 下段　CO2
      memset(Buffer, 0x0, 17);
      setLcdCursor(0, 1);
      writeLine(Buffer);
      sprintf( Buffer, "CO2:%5.1f ppm", *co2);
      setLcdCursor(0, 1);
      writeLine(Buffer);     

      
    }

    return true;
    
  } else {
    Serial.println("No data");

    //ここは必要か確認する
    return false;
  }
}


//IEEE 単精度浮動小数点を4バイトのバイナリにする関数
//第一引数　ペイロードで送信したい値(単精度浮動小数点数)
//第二引数　ペイロードの保存先(4byteで8文字＋のnullで9バイト確保)
void makePayLoad(float value, char *buff){

  // IEEE 754形式の浮動小数点のメモリイメージを32bit整数にコピー
  // メモリ上のバイト順は浮動小数点、整数それぞれ共通
  uint32_t SendValue;
  
  memcpy( &SendValue, &value, sizeof(SendValue));   //
  sprintf(buff, "%08X", SendValue);
    
}

// for LCD
#define AQM1602Y_ADDR 0x3e

int writeCommand(byte command) {
  Wire.beginTransmission(AQM1602Y_ADDR);
  Wire.write(0x0);
  Wire.write(command);
  return Wire.endTransmission();
}

int writeData(byte data) {
  Wire.beginTransmission(AQM1602Y_ADDR);
  Wire.write(0b01000000);
  Wire.write(data);
  return Wire.endTransmission();
}

void initLcd() {
  delay(100);
  writeCommand(0x38); // Function Set
  delay(1);
  writeCommand(0x39); // Unction Set
  delay(1);
  writeCommand(0x14); // BS=0:1/5 BIAS; F2 F1 F0: 100
  delay(1);
  writeCommand(0x73); // Contrast set
  delay(1);
  writeCommand(0x5e); // Icon on, booster circuit on
  delay(1);
  writeCommand(0x6c); // Follower circuit on
  delay(1);
  writeCommand(0x0c); // Entire display on
  delay(1);
  writeCommand(0x01); // Clear display
  delay(1);
  writeCommand(0x06); // Entry Mode Set, increment
  delay(1);
}

void setLcdCursor(int x, int y) {
  y *= 0x40;
  y += x;
  y |= 0b10000000;

  writeCommand(y);
}

int writeChar(byte c) {
  if (c < 0x20) {
    return -1; // error
  }

  if (0x7f < c) {
    return -1; // error
  }
  return writeData((byte)c);
}

void writeLine(char *s) {
  int len = strlen(s);
  int ret = 0;
  
  for (int i = 0;i < len; i++) {
    writeChar(s[i]);
  }
}
