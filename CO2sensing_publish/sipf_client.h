/*
 * Copyright (c) 2021 Sakura Internet Inc.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef _SIPF_OBJ_PARSER_H_
#define _SIPF_OBJ_PARSER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//おそらく$$TXコマンドでデータを送信するときのペイロードのデータの形
typedef enum {
    OBJ_TYPE_UINT8      = 0x00,
    OBJ_TYPE_INT8       = 0x01,
    OBJ_TYPE_UINT16     = 0x02,
    OBJ_TYPE_INT16      = 0x03,
    OBJ_TYPE_UINT32     = 0x04,
    OBJ_TYPE_INT32      = 0x05,
    OBJ_TYPE_UINT64     = 0x06,
    OBJ_TYPE_INT64      = 0x07,
    OBJ_TYPE_FLOAT32    = 0x08,
    OBJ_TYPE_FLOAT64    = 0x09,
    OBJ_TYPE_BIN_BASE64 = 0x10,
    OBJ_TYPE_STR_UTF8   = 0x20
}   SimpObjTypeId;


typedef struct {
  bool fixed;
  float longitude;
  float latitude;
  float altitude;
  float speed;
  float heading;
  int year;
  int month;
  int day;
  int hour;
  int minute;
  int second;
} GnssLocation;

int SipfSetAuthMode(uint8_t mode);
int SipfSetAuthInfo(char *user_name, char *password);

int SipfCmdTx(uint8_t tag_id, SimpObjTypeId type, uint8_t *value, uint8_t value_len, uint8_t *otid);

int SipfUtilReadLine(uint8_t *buff, int buff_len, int timeout_ms);
void SipfClientFlushReadBuff(void);

int SipfSetGnss(bool is_active);
int SipfGetGnssLocation(GnssLocation *loc);



#ifdef __cplusplus
}
#endif
#endif
