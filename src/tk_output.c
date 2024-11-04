/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include "tk_output.h"

#include "Porting_SAMD21.h"


#define TK_CLK(x) ((x==1)?(phDriver_PinWrite(PHDRIVER_PIN_nDATA1, DATA_ACT_HIGH_LEVEL)):(phDriver_PinWrite(PHDRIVER_PIN_nDATA1, DATA_ACT_LOW_LEVEL)))
#define TK_DAT(x) ((x==1)?(phDriver_PinWrite(PHDRIVER_PIN_nDATA0, DATA_ACT_HIGH_LEVEL)):(phDriver_PinWrite(PHDRIVER_PIN_nDATA0, DATA_ACT_LOW_LEVEL)))
#define TK_CP(x)  ((x==1)?(phDriver_PinWrite(PHDRIVER_PIN_nTKCP,  TKCP_ACT_HIGH_LEVEL)):(phDriver_PinWrite(PHDRIVER_PIN_nTKCP,  TKCP_ACT_LOW_LEVEL)))


BYTE HEX2BCD(BYTE *hex, BYTE hlen, BYTE *bcd, BYTE blen)
{
	BYTE bi,i;
	int  va, vs;
    u8 v;

	memset(bcd, 0, blen);
	bi = 0;
	while (hlen>0 && bi<blen)
	{
		va = 0;
		for (i=1; i<=hlen; i++)
		{
            v = (u8)hex[hlen-i];
			va = va * 256 + v;
			vs = va / 100;
			va = va % 100;

			hex[hlen-i] = vs;
		}

		if ( bi<blen )
		{
			vs = va / 10;
			bcd[bi] = (va % 10) + ((BYTE)vs<<4);
			bi++;
		}
		
		while( hex[hlen-1]==0 && hlen>0) hlen--;
	}
	return bi*2;
}

///////////////////////////////////////////////////////////////////////////////
BYTE _BCD_(BYTE hex)
{
	BYTE result;
	
	result = hex % 10;
	result = result | ((hex / 10)<<4);
	return result;
}


BYTE BYTE2BCD(BYTE *hex, BYTE hlen, BYTE *bcd, BYTE blen)
{
	bool bHigh;
	BYTE i,bi;
    u8 v;

	bHigh = true;
	bi = 0;

	for (i=0; i<hlen && bi<blen; i++)
	{
		v = (u8)hex[i];
		if ( bHigh )
		{	
			bcd[bi]   = _BCD_(v / 10);		// 2 位					
			bcd[bi+1] = (v % 10)<<4;		// 1 位 
			bi = bi+1;
			bHigh = false;
		}
		else
		{
			bcd[bi] = bcd[bi] | (v / 100);	// 1 位
			bcd[bi+1] = _BCD_(v % 100);		// 2 位
			bi = bi+2;
			bHigh = true;
		}
	}
	if ( !bHigh )
	 	return bi+1;
	return bi;
}

bool check_bcd(BYTE *bcd, BYTE size)
{
	BYTE i;	
	for (i=0; i<size; i++)
	{
		if ( (bcd[i]>>4)>9 )
			return false;

		if ( (bcd[i] & 0x0F)>9 )
			return false;
	}
	return true;
}

#ifndef _SMALL_
BYTE STR2BCD(BYTE *str, BYTE slen, BYTE *bcd, BYTE blen)
{
	BYTE i,b;
	bool h;
    u8 v;
	
	memset(bcd, 0, blen);
	h = false;
	b = 0;
	for (i=0; i<slen && b<blen ; i++)
	{
        v = (u8)str[i];

		if ((v >= 0x30) && (v <= 0x39))
		{
			if ( h )
			{
				 bcd[b] = bcd[b] | ((v - 0x30) << 4);
				 b++;
			}
			else bcd[b] = v - 0x30;
			h = !h;
		}
	}
	return b*2;
}
#endif

/**********************************************************
	MSR Track Output
***********************************************************/
void delay_clk_h(void)
{
    phDriver_TimerStart(PH_DRIVER_TIMER_MICRO_SECS, 800, NULL);//800, NULL);
}

void delay_clk_l(void)
{
    phDriver_TimerStart(PH_DRIVER_TIMER_MICRO_SECS, 900, NULL);//950, NULL);
}

void delay_clk_d(void)
{
    phDriver_TimerStart(PH_DRIVER_TIMER_MICRO_SECS, 200, NULL);//200, NULL);
}

void TK_Init(void)
{
	TK_CP(1);
	TK_DAT(1);
	TK_CLK(1);
}

void TK_DAT0(void)
{
	TK_DAT(1);		// DAT = High
	delay_clk_d();
	TK_CLK(0);		// CLK = Low
	delay_clk_l();
	TK_CLK(1);		// CLK = High
	delay_clk_h();
}

void TK_DAT1(void)
{
	TK_DAT(0);		// DAT = Low
	delay_clk_d();
	TK_CLK(0);		// CLK = Low
	delay_clk_l();
	TK_CLK(1);		// CLK = High
	delay_clk_h();
}

BYTE tkLRC;
void TK_SendCode(BYTE tCode, BYTE bpc)
{
	BYTE i;
	bool p,b;
	
	p = 1; // odd parity for ISO format
	for (i=0; i<bpc; i++)
	{
		if ( i<(bpc-1) )
		{
			b = (tCode>>i) & 0x01;
			p = p ^ b;
		}
		else b = p;

		if ( b )
			 TK_DAT1();
	   	else TK_DAT0();
	}
	tkLRC = tkLRC ^ tCode;
}


#if defined(_CUSTOMER_SAT_)
extern BYTE uid_bcd[];		// 2009/1/21
extern BYTE uid_len;
#endif

#ifndef _SMALL_
void TK2_Output_LSB(BYTE *bcd, BYTE len)
{
	BYTE i;
	bool h;

	tkLRC = 0;
	TK_CP(0);

    phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 25, NULL);

	// lead-zero
	for (i=0; i<10; i++)
		TK_DAT0();

	// Send SS
	TK_SendCode(0x0B, 5);	// ';'

#if defined(_CUSTOMER_SAT_)
	// Send UID
	if ( uid_len & 0x01 )
		 h = true;
	else h = false;

	for (i=0; i<uid_len; i++)
	{
		if ( !h )
			 TK_SendCode(uid_bcd[i/2] & 0x0F, 5);		// Low Order of Byte
		else TK_SendCode((uid_bcd[i/2] >>4) & 0x0F, 5); // High Order of Byte
		h = !h;
	}

	TK_SendCode(0x0E, 5);	// '>'
#endif
	 
	// Send DAta
	h = false;
	for (i=0; i<len; i++)
	{
		if ( !h )
			 TK_SendCode(bcd[i/2] & 0x0F, 5);		// Low Order of Byte
		else TK_SendCode((bcd[i/2] >>4) & 0x0F, 5); // High Order of Byte
		h = !h;
	}

	// Send ES
	TK_SendCode(0x0F, 5);	// '?'

	// Send LRC
	TK_SendCode(tkLRC, 5);

	// lead-zero
	for (i=0; i<10; i++)
		TK_DAT0();

    TK_CLK(0); // CLK Low
    phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 25, NULL);

	// Return to Normal
	TK_CLK(1); // CLK High
	TK_CP(1);	// CP  High
}
#endif

void TK2_Output_MSB(BYTE *bcd, BYTE len)
{
	BYTE i;
	bool h;

	tkLRC = 0;
	TK_CP(0);

    phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 25, NULL);

	// lead-zero
	for (i=0; i<10; i++)
		TK_DAT0();

	// Send SS
	TK_SendCode(0x0B, 5);	// ';'

#if defined(_CUSTOMER_SAT_)
	// Send UID
	if ( uid_len & 0x01 )
		 h = true;
	else h = false;

	for (i=uid_len-1; i>=0; i--)
	{
		if ( !h )
			 TK_SendCode((uid_bcd[i/2] >>4) & 0x0F, 5); // High Order of Byte
		else TK_SendCode(uid_bcd[i/2] & 0x0F, 5);		// Low Order of Byte
		h = !h;
	}

	TK_SendCode(0x0E, 5);	// '>'
#endif

	// Send DAta
	if ( len & 0x01 )
		 h = true;
	else h = false;

	for (i=len-1; i>=0; i--)
	{
		if ( !h )
			 TK_SendCode((bcd[i/2] >>4) & 0x0F, 5); // High Order of Byte
		else TK_SendCode(bcd[i/2] & 0x0F, 5);		// Low Order of Byte
		 
		h = !h;
	}

	// Send ES
	TK_SendCode(0x0F, 5);	// '?'

	// Send LRC
	TK_SendCode(tkLRC, 5);

	// lead-zero
	for (i=0; i<10; i++)
		TK_DAT0();

    TK_CLK(0); // CLK Low
    phDriver_TimerStart(PH_DRIVER_TIMER_MILLI_SECS, 25, NULL);

	// Return to Normal
	TK_CLK(1); // CLK High
	TK_CP(1);	// CP  High
}


