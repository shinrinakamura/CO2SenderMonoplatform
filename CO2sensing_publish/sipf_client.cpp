/*
 * Copyright (c) 2021 Sakura Internet Inc.
 *
 * SPDX-License-Identifier: MIT
 * 
 * otid：オブジェクト型の識別子でオブジェクト型を判断するものだと思われるが、確証はない
 * このシステムにおけるotidという言葉の意味を確認する必要がある
 */
 
 /*コマンドドキュメント
 https://github.com/sakura-internet/sipf-std-client_nrf9160/wiki/commands　*/
#include <Arduino.h>
//さくらインターネットが作成したファームウェアに対応したライブラリ
#include "sipf_client.h"
#include <stdio.h>
#include <string.h>

//モジュールからの返り値を受信するための領域
static char cmd[256];

//UARTの受信バッファを読み捨てる
void SipfClientFlushReadBuff(void)
{
    //受信メッセージの長さ
    int len;
    //これはなんなのかわからない commented by S.Nakamrua. 2021-08-25 
    uint8_t b;
    //commented by S.Nakamrua. 2021-08-25 
    //シリアルポートから受信可能なバイト数（文字数 ）を取得
    len = Serial2.available();
    //受信した文字数のみ読み込む
    for (int i = 0; i < len; i++) {
        Serial2.read();
    }
}


//１行([CR] or [LF]の手前まで)をバッファに詰める
//第一引数：バッファ（？）のメモリのポインタ
//第二引数：セットするサイズ
//第三引数：タイムアウト
//返り値：領域の長さもしくはエラーコード
int SipfUtilReadLine(uint8_t *buff, int buff_len, int timeout_ms)
{
    uint32_t t_recved, t_now;
    int ret;
    int len, idx = 0;
    uint8_t b;
    /*memset メモリに指定バイト数の分の値をセットする
    第一引数：メモリのポインタ、第二引数：セットする値、第三引数：セットするサイズ
    */
    memset(buff, 0, buff_len);
    //受信時間時間をセット
    t_recved = millis();
    
    //中で指定した条件までforで回す
    for (;;) {
        t_now = millis();
        len = Serial2.available();
        for (int i = 0; i < len; i++) {
            ret = Serial2.read();;
            if (ret >= 0) {
                b = (uint8_t)ret;
                //改行コードがくるまでバッファに入れる
                if (idx < buff_len) {
                    //行末を判定
                    if ((b == '\r') || (b == '\n')) {
                        buff[idx] = '\0';
                        return idx + 1; //長さを返す
                    }
                    //バッファに詰める
                    buff[idx] = b;
                    idx++;
                }
            }
            t_recved = t_now;
        }
        //タイムアウト判定
        //
        if ((int32_t)((t_recved + timeout_ms) - t_now) < 0) {
            //キャラクター間タイムアウト
            return -3;
        }
    }   //ここまでfor
    return idx; //読み込んだ長さ
}



/**
 * $Wコマンドを送信
 */
 //指定アドレスへのレジスタ書き込み
 //第一引数：指定アドレス（指定アドレスのレジスタを読み込む）
 //第二引数：
static int sipfSendW(uint8_t addr, uint8_t value)
{
    int len;
    int ret;
    
    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $Wコマンド送信　レジスタの読み込み
    len = sprintf(cmd, "$W %02X %02X\r\n", addr, value);
    ret = Serial2.write((uint8_t*)cmd, len);

    // $Wコマンド応答待ち
    for (;;) {
        //返り値を読み込む
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // キャラクタ間タイムアウト500msで1行読む
        //タイムアウトのエラーコマンドがきた時
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        /*int memcmp (const void *str1 , const void *str2, size_t len );
        第一引数：比較したい文字１、比較したい文字２、比較するバイト数
        str1 < str2; return < 0,  str1 == str2; return == 0, str1 > str2: return >0
        つまり文字列が合っていれば返り値が0になるはずである
        この使い方が本当に正しいか確認する必要がある
        さくらインターネットが採用しているので、おそらく正しいのではないかと思われる 
        */
        //モジュールからの返り値を確認
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            return 0;
        } else if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return 1;
        }
        delay(1);
    }
    return ret;
}

/**
 * $Rコマンド送信
 */
 //指定アドレスのレジスタを読み出す
static int sipfSendR(uint8_t addr, uint8_t *read_value)
{
    int len, ret;
    char *endptr;
    //受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $Rコマンド送信
    //返り値：生成した文字列の文字数(nullを除く) 
    len = sprintf(cmd, "$R %02X\r\n", addr);
    /*Serial.write(buf, len);
    第一引数；配列として定義された複数のバイト、第二引数：配列の長さ、返り値：送信したバイト数*/
    ret = Serial2.write((uint8_t*)cmd, len);

    // $Rコマンド応答待ち
    for (;;) {
        //受信コマンドを改行コードの前までバッファに詰める
        //10秒でタイムアウト
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000);
        //エラーが帰ってきた場合
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        //コマンドの入力が始まった場合エコーバックを開始するために処理を抜ける
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        //NGが帰ってきた場合１を返す（エラーコード？）
        if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return 1;
        }
        //文字列の長さが２の時は(null)は含まない
        if (strlen(cmd) == 2) {
            /*long strtol(const char *s,  char **endptr, int base);
            文字列を10真数のlong型に返却する。返却できない文字はその文字列のポインタをendptrに格納する
            baseを指定することにより好きな基数を変更することができる
            */
            //Valueらしきもの　１６真数に変換する
            //nullで終わっていない時はエラー？を返す
            *read_value = strtol(cmd, &endptr, 16);
            if (*endptr != '\0') {
                //Null文字以外で変換が終わってる
                return -1;
            }
            break;
        }
    }
    
    for (;;) {
        //エコーバックの中身を作成する
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // キャラクタ間タイムアウト500msで1行読む
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            break;
        }
    }
    //成功した時は0を返す
    return 0;   
}

/**
 * 認証モード設定
 * 認証情報レジスタ
 * mode: 0x00 パスワード認証, 0x01: IPアドレス(SIM)認証
 * https://github.com/sakura-internet/sipf-std-client_nrf9160/wiki/ip_auth
 */
int SipfSetAuthMode(uint8_t mode)
{
    int ret;
    uint8_t val;
    //$W 0x00 mode\r\n （認証モードをモジュールに教える）
    if (sipfSendW(0x00, mode) != 0) {
        //$W 送信失敗
        return -1;
    }
    //
    for (;;) {
        delay(200);
        //モジュールからの返り値を読み込む
        ret = sipfSendR(0x00, &val);
        // 読み込みが成功しなかった時はエラーコードを返す
        if (ret != 0) {
            return ret;
        }
        //返り値が選択したモードと等しければ終了
        if (val == mode) {
            break;
        }
    }
    return 0;
}


/**
 * 認証情報を設定
 * 認証モードの変更ができたら続けて行う
 */
int SipfSetAuthInfo(char *user_name, char *password)
{
    int len;
    
    //ユーザー名の長さを確認 
    len = strlen(user_name);
    if (len > 80) {
        return -1;
    }
    //
    if (sipfSendW(0x10, (uint8_t)len) != 0) {
        return -1;  //$W送信失敗
    }
    delay(200);
    //ユーザー名を設定
    for (int i = 0; i < len; i++) {
        if (sipfSendW(0x20 + i, (uint8_t)user_name[i]) != 0) {
            return -1;  //$W送信失敗
        }
        delay(200);
    }

    //パスワードの長さを設定
    len = strlen(password);
    if (len > 80) {
        return -1;
    }
    if (sipfSendW(0x80, (uint8_t)len) != 0) {
        return -1;  //$W送信失敗
    }
    delay(200);
    //パスワードを設定
    for (int i = 0; i < len; i++) {
        if (sipfSendW(0x90 + i, (uint8_t)password[i]) != 0) {
            return -1;  //$W送信失敗
        }
        delay(200);
    }

    return 0;
}


//GNSSのセット
//第一引数：GNSSのオン、オフ
int SipfSetGnss(bool is_active) {
    int len;
    int ret;

    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$GNSSENコマンド送信
    //アクティブにするかどうかを文字として作成する
    len = sprintf(cmd, "$$GNSSEN %d\r\n", is_active?1:0);
    ret = Serial2.write((uint8_t*)cmd, len);

    // 応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 1000); // 0.5秒間なにも応答なかったら諦める　元からあったコメント
        if (ret == -3) {
            // タイムアウト
            return -3;
        }
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            // OK
            break;
        }
    }

    return 0;
}

//位置情報を取得する
//すぐには使用しないので、後回しにする
int SipfGetGnssLocation(GnssLocation *loc) {
    int len;
    int ret;

    if (loc == NULL) {
      return -1;
    }

    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$GNSSLOCコマンド送信
    len = sprintf(cmd, "$$GNSSLOC\r\n");
    ret = Serial2.write((uint8_t*)cmd, len);

    // 位置情報待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000); // 10秒間なにも応答がなかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }

        if (cmd[0] == '$') {
            // エコーバック
            continue;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        //GNSS情報が返ってきたときの処理
        if (cmd[0] == 'A' || cmd[0] == 'V') {
            // 位置情報
            int counter = 0;
            char *head = cmd;
            char *next = cmd;
            bool is_last = false;
            for(;;) {
              while(*next != '\0' && *next != ','){
                next++;
              }
              is_last = (*next == '\0');
              *next = '\0';
              next++;
              String str = String(head);

              switch(counter){
                case 0: // FIXED
                  if (head[0] == 'A')
                    loc->fixed = true;
                  else if (head[0] == 'V')
                    loc->fixed = false;
                  else
                    return -2;
                  break;
                case 1: // Longitude
                  loc->longitude = str.toFloat();
                  break;
                case 2: // Latitude
                  loc->latitude = str.toFloat();
                  break;
                case 3: // Altitude
                  loc->altitude = str.toFloat();
                  break;
                case 4: // Speed
                  loc->speed = str.toFloat();
                  break;
                case 5: // Heading
                  loc->heading = str.toFloat();
                  break;
                case 6: // Datetime
                  if (strlen(head) != 20) {
                    return -2;
                  }
                  if (head[4] != '-' || head[7] != '-' || head[10] != 'T' || head[13] != ':' ||  head[16] != ':' ||  head[19] != 'Z'){
                    return -2;
                  }
                  head[4] = '\0';
                  head[7] = '\0';
                  head[10] = '\0';
                  head[13] = '\0';
                  head[16] = '\0';
                  head[19] = '\0';
                  loc->year = atoi(&head[0]);
                  loc->month = atoi(&head[5]);
                  loc->day = atoi(&head[8]);
                  loc->hour = atoi(&head[11]);
                  loc->minute = atoi(&head[14]);
                  loc->second = atoi(&head[17]);
                  break;
              }

              if (is_last){
                if (counter != 6) {
                  return -2;
                }
                break;
              }
              head = next;
              counter++;
            }
            break;
        }
    }

    // OK応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 1000); // 0.5秒間なにも応答なかったら諦める
        if (ret == -3) {
            // タイムアウト
            return -3;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            // OK
            break;
        }
    }

    return 0;
}


//任意のデータを送信するコマンド
//第一引数：
//第二引数：送信するオブジェクトのタイプ
//第三引数：送信するデータ
//第四引数：送信データの長さ
//第五引数：モジュールからの返り値を保持する領域
int SipfCmdTx(uint8_t tag_id, SimpObjTypeId type, uint8_t *value, uint8_t value_len, uint8_t *otid)
{
    int len;
    int ret;
    
    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$TXコマンド送信
    //オブジェクトのタイプとオブジェクトを詰めこむ
    len = sprintf(cmd, "$$TX %02X %02X ", tag_id, (uint8_t)type);
    
    //オブジェクトの種類に応じて変換の方法を振り分ける
    switch (type) {
        //バイナリと文字列は左側から変換していく
        case OBJ_TYPE_BIN_BASE64:
        case OBJ_TYPE_STR_UTF8:
			//順番どおりに文字列に変換
            for (int i = 0; i < value_len; i++) {
                len += sprintf(&cmd[len], "%02X", value[i]);
            }
            break;
        default:
            //もとからあったコメントだがこのコメントが本当に正しいか確認する必要がある
            // リトルエンディアンだからアドレス上位から順に文字列に変換
            for (int i = (value_len - 1); i >= 0; i--) {
                len += sprintf(&cmd[len], "%02X", value[i]);
            }
            break;
    }

    /*このコードを見て気づいたこと
    実際のデータ型がなんであれバイナリで考えるとint型でつなぎ合わせていくことができる？
    */

    //送信するhexを作成していく
    len += sprintf(&cmd[len], "\r\n");
    //モジュールに送信する
    ret = Serial2.write((uint8_t*)cmd, len);

    // OTID待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000); // 10秒間なにも応答がなかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        if (ret == 33) {
            //OTIDらしきもの
            memcpy(otid, cmd, 32);
            break;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return -1;
        }
    }
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // 0.5秒間なにも応答なかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            break;
        }
    }
    return 0;
}
