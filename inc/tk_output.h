/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ams_types.h"

#ifndef _TK_OUTPUT_
#define _TK_OUTPUT_

BYTE HEX2BCD(BYTE *hex, BYTE hlen, BYTE *bcd, BYTE blen);
BYTE STR2BCD(BYTE *str, BYTE slen, BYTE *bcd, BYTE blen);
bool check_bcd(BYTE *bcd, BYTE size);

BYTE _BCD_(BYTE hex);
BYTE BYTE2BCD(BYTE *hex, BYTE hlen, BYTE *bcd, BYTE blen);

void TK_Init(void);
void TK_SendCode(BYTE tCode, BYTE bpc);
void TK2_Output_LSB(BYTE *bcd, BYTE len);
void TK2_Output_MSB(BYTE *bcd, BYTE len);

void delay_clk_h(void);
void delay_clk_l(void);
void delay_clk_d(void);

void TK_DAT0(void);
void TK_DAT1(void);

#endif

