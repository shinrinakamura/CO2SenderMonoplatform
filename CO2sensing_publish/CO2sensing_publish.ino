/*
CO2センサを利用して、CO２濃度、温度、湿度を取得してさくらのモノのプラットフォームに送信するプログラム

M5stack basicの場合はarduinoのボードマネージャはM5Stack-Core2を使用する
ボードとボードマネージャの組み合わせがあっていないとうまく動きません
SCD30の情報　https://wiki.seeedstudio.com/jp/Grove-CO2_Temperature_Humidity_Sensor-SCD30/
i2cのアドレスは0x61

センサーのキャリブレーション等について
https://tomoto335.hatenablog.com/entry/co2-sensors  
*/


#include "MeasureCO2Sensor.h"
#include <M5Stack.h>
#include "sipf_client.h"  //さくらの通信ライブラリ

//タイマー設定
//割り込み通知用のカウンター
volatile int timeCounter1 = 1 ;    //最初に一度送信したいので
//uint8_t timer_flg_1 =0;   //最初に一度送信したいので

portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

//ハードウェアタイマーの準備
hw_timer_t *timer1 = NULL;

//割り込み時に呼ばれる割り込み関数
//フラグを立てるためにカウンターをインクリメントする
void IRAM_ATTR onTimer1(){
  // Increment the counter and set the time of ISR
  portENTER_CRITICAL_ISR(&timerMux);
  timeCounter1++;
  portEXIT_CRITICAL_ISR(&timerMux);
}

void TimerSet(int time){
  
  timer1 = timerBegin(0, 80, true);
 
  // Attach onTimer function.
  timerAttachInterrupt(timer1, &onTimer1, true);
  
  //タイマの動作間隔の指定（トリガー条件）
  //第一引数：タイマーへのポインタ
  //第二引数：割り込み発生までの時間（マイクロ秒で指定）
  //第三引数：カウンターのリロード指定（true:定期実行、false:1ショットの実行）
  timerAlarmWrite(timer1, (time* 1000000*60), true);   //1分毎
  
  
  // Start an alarm
  //タイマーの有効化　timerAlarmDisable関数でストップさせることもできる
  //引数：タイマー変数
  timerAlarmEnable(timer1);
 
}

/**
 * SIPF接続情報
 */
static uint8_t buff[256];

static uint32_t cnt_btn1, cnt_btn2, cnt_btn3; //M5stackのボタンの設定

//モジュールをリセットする
static int resetSipfModule()
{
  digitalWrite(5, LOW);
  pinMode(5, OUTPUT);
  // Reset要求
  digitalWrite(5, HIGH);
  delay(10);
  digitalWrite(5, LOW);
  Serial2.begin(115200, SERIAL_8N1, 16, 17);  // モデムとの通信を初期化

  // 起動完了メッセージ待ち　
  //返り値は読み込んだ長さかタイムアウト判定
  Serial.println("### MODULE OUTPUT ###");
  int len, is_echo = 0;
  for (;;) {
    
    len = SipfUtilReadLine(buff, sizeof(buff), 300000); //タイムアウト300秒(5分)
    if (len < 0) {
      return -1;  //Serialのエラーかタイムアウト
    }
    if (len == 1) {
      //空行なら次の行へ
      continue;
    }
    if (len >= 13) {  //モジュールから返ってきたメッセージを確認
      if (memcmp(buff, "*** SIPF Client", 15) == 0) {
        is_echo = 1;
      }
      if (memcmp(buff, "+++ Ready +++", 13) == 0) {
        Serial.println("#####################");
        break;
      }
      if (memcmp(buff, "ERR:Faild", 9) == 0) {  //接続リトライオーバー
        Serial.println((char*)buff);
        Serial.println("#####################");
        return -1;
      }
    }
    if (is_echo) {  //おそらくローカルエコーを見ている
      //ここは何をしているかよくわからない
      Serial.printf("%s\r\n", (char*)buff);
    }
  }
  return 0;
}

//画面のタイトルを表示
static void drawTitle(void)
{
  M5.Lcd.setTextSize(2);  
  M5.Lcd.fillRect(0, 0, 320, 20, 0x07FF);
  M5.Lcd.setCursor(2, 2);
  M5.Lcd.setTextColor(TFT_BLACK, 0x07FF);
  M5.Lcd.printf("CO2 sensor LTE connect\n");  //実際に表示される内容

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.setCursor(0, 24);
}

//ボタンの描画
static void drawButton(uint8_t button, uint32_t value)
{
  M5.Lcd.fillRect(35 + (95 * button), 200, 60, 40, 0x07FF);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_BLACK, 0x07FF);
  M5.Lcd.setCursor(40 + (95 * button), 210);
  M5.Lcd.printf("%4d", value);
}

//2番目のボタンの描画
void drawBUtton2(){
  
  M5.Lcd.fillRect(130, 200, 60, 40, 0x07FF);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_BLACK, 0x07FF);
  M5.Lcd.setCursor(135, 210);
  M5.Lcd.printf("pub");
  
}


static void setCursorResultWindow(void)
{
  M5.Lcd.setTextColor(TFT_BLACK, 0xce79);
  M5.Lcd.setCursor(0, 121);
}

//Resultボタンを表示する
static void drawResultWindow(void)
{
  M5.Lcd.setTextSize(1);

  M5.Lcd.fillRect(0, 110, 320, 10, 0x07FF);
  M5.Lcd.setTextColor(TFT_BLACK, 0x07FF);
  M5.Lcd.setCursor(1, 111);
  M5.Lcd.printf("RESULT");

  M5.Lcd.fillRect(0, 120, 320, 78, 0xce79);
  setCursorResultWindow();
}



#ifdef ENABLE_GNSS
//位置情報を描画する
static void printGnssLocation(GnssLocation *gnss_location_p) {
  //fixedはbooleanなので、接続されているかどうかを判断している？
  if (!gnss_location_p->fixed) {
    M5.Lcd.printf("Not fixed\n");
  }else{
   M5.Lcd.printf("Fixed\n");
  }

   //位置情報を描画する
   M5.Lcd.printf("%.6f %.6f\n", gnss_location_p->latitude, gnss_location_p->longitude);

   M5.Lcd.printf("%04d-%02d-%02d %02d:%02d:%02d (UTC)\n",
    gnss_location_p->year, gnss_location_p->month, gnss_location_p->day,
    gnss_location_p->hour, gnss_location_p->minute, gnss_location_p->second
   );
}

static void drawGnssLocation(GnssLocation *gnss_location_p) {

  M5.Lcd.setTextSize(1);

  M5.Lcd.fillRect(0, 70, 320, 10, 0xfaae);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.setCursor(1, 71);
  M5.Lcd.printf("GNSS");
  M5.Lcd.fillRect(0, 80, 320, 30, 0xce79);

  M5.Lcd.setTextColor(TFT_BLACK, 0xce79);
  M5.Lcd.setCursor(0, 81);

  if(gnss_location_p == NULL) {
    M5.Lcd.println("GNSS error");
  }else{
    printGnssLocation(gnss_location_p);
  }

  setCursorResultWindow();
}
#endif


void FloatToBin(float value, char *payload_buff);

void setup(){

  //M5Stackの初期化：LED,SD,Serial,i2cを使用するかどうか引数で渡す
  M5.begin();
  /*次の処理をしないと初期化がうまくいかない*/
  M5.Power.begin(); //Init Power module.  初始化电源模块
  Serial.println("CO2 sensor  test start");
  //タイマーのセット
  TimerSet(3);
  
  SensorSet();    
  delay(8*1000); //センサーの応答時間から決めた

  Serial.println("sensor init done");


  //ここからディスプレイ
  M5.Lcd.setBrightness(127);

  //タイトルの描画
  drawTitle();

  M5.Lcd.printf("Booting...");
  if (resetSipfModule() == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }

  //認証モードの確認
  M5.Lcd.printf("Setting auth mode...");
  //SIM認証モードで確認している
  if (SipfSetAuthMode(0x01) == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }

#ifdef ENABLE_GNSS
  M5.Lcd.printf("Enable GNSS..");
  if (SipfSetGnss(true) == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }
#endif
  drawResultWindow();

  cnt_btn1 = 0;
  cnt_btn2 = 0;
  cnt_btn3 = 0;

  drawButton(0, cnt_btn1);
  //drawButton(1, cnt_btn2);
  drawButton(2, cnt_btn3);

  //2番目のボタンの描画
  drawBUtton2();

  Serial.println("+++ Ready +++");
  //UARTの受信バッファを読み捨てる
  SipfClientFlushReadBuff();

}


void loop(){

  float temp, hum, co2;
  
  if (MesureCO2( &temp, &hum, &co2) == true){

    //確認用の表示
    Serial.print("temp : ");
    Serial.println(temp);
    Serial.print("hum : ");
    Serial.println(hum);
    Serial.print("CO2 : ");
    Serial.println(co2);
    Serial.println("");

    //ディスプレイへの表示
    M5.Lcd.setTextSize(2);
    M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    M5.Lcd.setCursor(0, 50);
    M5.Lcd.printf("temp : %.1f\n", temp);
    M5.Lcd.printf("humi : %.1f\n", hum);
    M5.Lcd.printf("CO2  : %.1f\n", co2);

  }

  if (timeCounter1 > 0 && MesureCO2( &temp, &hum, &co2) == true){
      
      timeCounter1 = 0;
      //送信間隔を決めて割り込みが入ったら送信するようにする
      //送信する文字列を作成する
      char payload_buff[9];
      String PubPayload;
      FloatToBin(temp, payload_buff);
      PubPayload = payload_buff;
      FloatToBin(hum, payload_buff);
      PubPayload += payload_buff;
      FloatToBin(co2, payload_buff);
      PubPayload += payload_buff;
      
      //文字列を文字の配列に変換する
      unsigned int char_length = PubPayload.length() + 1;   //null文字を一文字足す
      char payload[char_length];  
      PubPayload.toCharArray(payload, char_length);
      
      //取得した値を文字列で送信する
      int ret = SipfCmdTx(0x01, OBJ_TYPE_STR_UTF8, (uint8_t*)payload, char_length, buff);

      //コマンド送信ができたか確認する（モジュールからの返信も含めて）
      //送信に失敗したら3回トライしてそれでもダメな時はあきらめる
      int publish_try_num = 0;
      if (ret == 0) {
        
        //M5.Lcd.printf("OK\nOTID: %s\n", buff);
        M5.Lcd.printf("OK\n");
        M5.Lcd.printf("\n");
        Serial.println("OK\nOTID");
        Serial.println("");
        
      } else {
        M5.Lcd.printf("NG: %d\n", ret);
        Serial.println("NG: %d\n");
        Serial.println("");
        while(publish_try_num < 3){
          publish_try_num += 1;
          //もう一度通信に挑戦する
          ret = SipfCmdTx(0x01, OBJ_TYPE_STR_UTF8, (uint8_t*)payload, char_length, buff);
          delay(3000);
          //3回目の通信失敗でフラグを立ててモジュールリセットをかけるようにした方がよいかも
        }      
      }
    } 

}   //loopend


//IEEE 単精度浮動小数点を4バイトのバイナリにする関数
//第一引数　ペイロードで送信したい値(単精度浮動小数点数)
//第二引数　ペイロードの保存先(4byteで8文字＋のnullで9バイト確保)
void FloatToBin(float value, char *payload_buff){

  // IEEE 754形式の浮動小数点のメモリイメージを32bit整数にコピー
  // メモリ上のバイト順は浮動小数点、整数それぞれ共通
  uint32_t SendValue;
  
  memcpy( &SendValue, &value, sizeof(SendValue)); 
  sprintf(payload_buff, "%08X", SendValue);
    
}
