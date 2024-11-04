/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "ams_types.h"

#ifndef _WIEGANG_OUT_H_
#define _WIEGAND_OUT_H_

void Wiegand_Init(void);
void Wiegand_MSB(BYTE* dat, BYTE bits, bool bWGOutParity);
void Wiegand_LSB(BYTE* dat, BYTE bits, bool bWGOutParity);
int DecToHex (const char* cBuffer,int len,unsigned char* bOutBuffer,int iOutLen);

#ifdef _ODM_ST_
void Wiegand_MSB_ST(BYTE* dat, BYTE bits);
#endif

// misc
bool bitvalue(BYTE* dat, BYTE offset);
BYTE AddBits(BYTE* buf, BYTE offset, BYTE dat, BYTE bits, bool inv_order);
bool ShiftBits(u8* dest_buf, u8* src_buf, u8 src_ofs, u8 sft_bits, u16 src_bits, u16 max_bits, bool low_to_high);

void bit_copy_LE(u8* dest, u8* sour, u8 start, u8 bits);


#endif

