/*************************************************************************************
 *
 *
 *************************************************************************************/

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "Wiegand_out.h"
#include <string.h>

#include "Porting_SAMD21.h"

#include "cmsis_gcc.h"


#define ERROR_DATA_ABNORMAL 			0
#define ERROR_DATA_LEN_OVERFLOW 		0

extern phacDiscLoop_Sw_DataParams_t * pDiscLoop;

BYTE BitMask(BYTE bpos);
void bit_set(BYTE* p, u16 offset, bool value);
void Wiegand_Gpio_Init(void);
void delay_tpw(void);
void delay_tpi(void);
void Wiegand_High(void);
void Wiegand_Low(void);

#define W1(x) ((x==1)?(phDriver_PinWrite(PHDRIVER_PIN_nDATA1, DATA_ACT_HIGH_LEVEL)):(phDriver_PinWrite(PHDRIVER_PIN_nDATA1, DATA_ACT_LOW_LEVEL)))
#define W0(x) ((x==1)?(phDriver_PinWrite(PHDRIVER_PIN_nDATA0, DATA_ACT_HIGH_LEVEL)):(phDriver_PinWrite(PHDRIVER_PIN_nDATA0, DATA_ACT_LOW_LEVEL)))


BYTE BitMask(BYTE bpos)
{
    BYTE bmask;

    switch (bpos)
    {
        case 0:
            bmask = Bit0;
            break;
        case 1:
            bmask = Bit1;
            break;
        case 2:
            bmask = Bit2;
            break;
        case 3:
            bmask = Bit3;
            break;
        case 4:
            bmask = Bit4;
            break;
        case 5:
            bmask = Bit5;
            break;
        case 6:
            bmask = Bit6;
            break;
        case 7:
            bmask = Bit7;
            break;
        default:
            bmask = 0;
    }

    return bmask;
}


bool bitvalue(BYTE* dat, BYTE offset)
{
	BYTE bi,bo;

	bi = (offset / 8);
	bo = (offset % 8);
	if ((dat[bi] & BitMask(bo)) != 0)
		return 1;
	return 0;
}

void bit_set(BYTE* p, u16 offset, bool value)
{
	u8 bi,bo;

	bi = (offset / 8);
	bo = (offset % 8);

    if ( value )
		p[bi] |= BitMask(bo);
    else
        p[bi] &= ~BitMask(bo);
}


BYTE AddBits(BYTE* buf, BYTE offset, BYTE dat, BYTE bits, bool inv_order)
{
	BYTE bi, bo, b;
	bi = (offset / 8);
	bo = (offset % 8);

	for (b = 0; b < bits; b++)
	{
        if(inv_order == FALSE)
        {
            if (dat & BitMask(b))
                buf[bi] |= BitMask(bo);
            else
                buf[bi] &= ~BitMask(bo);
        }
        else
        {
            if (dat & BitMask(bits - 1 - b))
                buf[bi] |= BitMask(bo);
            else
                buf[bi] &= ~BitMask(bo);
        }

		bo++;
		if (bo > 7)
		{
			bo = 0;
			bi++;
		}			
	}

	return (bi * 8 + bo);
}

u8 InvertBits(u8 byte_data, u8 data_bits)
{
    u8 i, invert_data;

    for(i = 0; i < data_bits; i++)
    {
        if(byte_data & BitMask(i))
            invert_data |=  BitMask(data_bits - 1 - i);
        else
            invert_data &= ~BitMask(data_bits - 1 - i);
    }

    return invert_data;
}

// Can be used for shift within one byte or over different byte.
// [   Byte0   ] [   Byte1   ] [   Byte2   ] [   Byte3   ] ......
// Bit7.....Bit0 Bit15....Bit8 Bit23...Bit16 Bit31...Bit24 ......
// low_to_high is true for Bit0 -> Bit6, Bit8 -> Bit20, etc.
// low_to_high is false for Bit14 -> Bit8, Bit23 -> Bit10, etc.
bool ShiftBits(u8* dest_buf, u8* src_buf, u8 src_ofs, u8 sft_bits, u16 src_bits, u16 max_bits, bool low_to_high)
{
    u8 i, dest_ofs, src_byte, src_bit, dest_byte, dest_bit;

    if(low_to_high == FALSE)
    {
        if(sft_bits > src_ofs)
            return FALSE;

        dest_ofs = src_ofs - sft_bits;
    }
    else
    {
        if((src_bits + sft_bits) > max_bits)
            return FALSE;

        dest_ofs = src_ofs + sft_bits;
    }

    src_byte = src_ofs / 8;
    src_bit  = src_ofs % 8;
    dest_byte = dest_ofs / 8;
    dest_bit  = dest_ofs % 8;

    for(i = 0; i < (src_bits - src_ofs); i++)
    {
        if(src_buf[src_byte] & BitMask(src_bit))
            dest_buf[dest_byte] |=  BitMask(dest_bit);
        else
            dest_buf[dest_byte] &= ~BitMask(dest_bit);

        if(++src_bit > 7)
        {
            src_bit = 0;
            src_byte++;
        }

        if(++dest_bit > 7)
        {
            dest_bit = 0;
            dest_byte++;
        }
    }

    return TRUE;
}

void bit_copy_LE(u8* dest, u8* sour, u8 start, u8 bits)
{
    u8 b,v,s1,s2,i=0;

    s1 = start;
    s2 = bits-1;
    while(i<bits)
    {
        for (b=0; b<8 && i<bits; b++)
        {
            v = bitvalue((BYTE*)sour, s1+(7-b));
            bit_set((BYTE*)dest, s2-b, v);
            i++;
        }
        s1+=8;
        s2 =  (s2>=8) ? s2-8 : 0;
    }
}

void Wiegand_Gpio_Init(void)
{
}

void delay_tpw(void)
{
#if 1
    phDriver_TimerStart(PH_DRIVER_TIMER_MICRO_SECS, 1, NULL);//100, NULL);
#else
    unsigned int k = 4200;//210;
    while(k--);
#endif
}

/**********************************************************
	Wiegand output
 **********************************************************/ 
void Wiegand_Init(void)
{
    Wiegand_Gpio_Init();

    W0(1);
    W1(1);
}

void delay_tpi(void)
{
	char i;

	for(i = 0; i < 18; i++)
		delay_tpw();
}

void Wiegand_High(void)
{

#ifdef _TIBBO_
	W0(0);
#else
	W0(1);
#endif

	W1(0);
	delay_tpw();
	W1(1);
	W0(1);
	delay_tpi();

}

void Wiegand_Low(void)
{

	W1(1);
	W0(0);
	delay_tpw();
	W0(1);
	delay_tpi();

}

/**********************************************************
	Wiegand output 
	Start Bit + bits (Data) + Stop Bit
	Output: MSB first
***********************************************************/
void Wiegand_MSB(BYTE* dat, BYTE bits, bool bWGOutParity)
{
	bool sp, ep;
	BYTE i, len;

	sp = 0;	// Even
	ep = 1;	// Odd

	len = bits / 2;
	
	// parity bit
	if (bWGOutParity == TRUE)
	{
		// check start bit parity (even)
        if((bits % 2) == 0)
        {
            for (i = 0; i < len; i++)
                sp = sp ^ bitvalue(dat, bits - i - 1);
        }
        else
        {
            for (i = 0; i < (len + 1); i++)
                sp = sp ^ bitvalue(dat, bits - i - 1);
        }

		// check stop bit parity (odd)
		for (i = len; i < bits; i++)
			ep = ep ^ bitvalue(dat, bits - i - 1);
		
		// output weigand 32 bits
		if (sp)
			 Wiegand_High();
		else Wiegand_Low();
	}

	// data bit
	for (i = 0; i < bits; i++)
		if (bitvalue(dat, bits - i - 1))
			 Wiegand_High();
		else Wiegand_Low();
	
	// parity bit
	if (bWGOutParity == TRUE)
	{
		if (ep)
			 Wiegand_High();
		else Wiegand_Low();
	}
}


#ifdef _ODM_ST_
/**********************************************************
	Wiegand output (for OEM(ST))
	Start Bit =0
    Stop Bit (Even Parity)
	Output: MSB first
***********************************************************/
void Wiegand_MSB_ST(BYTE* dat, BYTE bits)
{
	bit  sp;
	BYTE i;

    EA = 0;
	sp = 0;	// Even

	// Start Bit:
	Wiegand_Low();	// Always = 0

	// Data Bits
	for (i=0; i<bits; i++)
		if ( bitvalue(dat, bits-i-1) )
		{
			 Wiegand_High();
			 sp = sp ^ 1;
		}
		else Wiegand_Low();

	if ( sp )
		 Wiegand_High();
	else Wiegand_Low();
    EA = 1;
}
#endif // end of _ODM_ST

#ifndef _SMALL_
/**********************************************************
	Wiegand output 
	Start Bit + bits (Data) + Stop Bit
	Output: LSB first
***********************************************************/
void Wiegand_LSB(BYTE* dat, BYTE bits, bool bWGOutParity)
{
	bool sp, ep;
	BYTE i, len;

	sp = 1;	// Odd
	ep = 0;	// Even

	len = bits / 2;
	
	// parity bit
	if (bWGOutParity == TRUE)
	{
		// check start bit parity (Odd)
        if((bits % 2) == 0)
        {
            for (i = 0; i < len; i++)
                sp = sp ^ bitvalue(dat, i);
        }
        else
        {
            for (i = 0; i < (len + 1); i++)
                sp = sp ^ bitvalue(dat, i);
        }

		// check stop bit parity (Even)
		for (i = len; i < bits; i++)
			ep = ep ^ bitvalue(dat, i);
		
		// output weigand 32 bits
		if (sp)
			 Wiegand_High();
		else Wiegand_Low();
	}

	// data bit
	for (i = 0; i < bits; i++)
		if (bitvalue(dat, i))
			 Wiegand_High();
		else Wiegand_Low();
	
	// parity bit
	if (bWGOutParity == TRUE)
	{
		if (ep)
			 Wiegand_High();
		else Wiegand_Low();
	}
}
#endif

int DecToHex (const char* cBuffer, int len, unsigned char* bOutBuffer, int iOutLen)
{
	char cData;
	int iValue;
	int iCount;
	int i;
	int j;
    
	iCount=0;
	for(i=0; i<len; i++)
	{
		cData=cBuffer[i];

        if(cData==0)
        {
            break;
        }
        else if(cData>='0' && cData<='9')
        {
            cData-='0';
            for(j=0; j<iCount; j++)
            {
                iValue=bOutBuffer[j]*10+cData;
                bOutBuffer[j]=(iValue & 0xFF);
                cData=(iValue>>8);
            }

            if((cData!=0) || (iCount==0))
            {
                bOutBuffer[iCount]=cData;
                iCount++;

                if (iCount>iOutLen)
                {
                    iCount=ERROR_DATA_LEN_OVERFLOW;
                    break;
                }
            }
        }
		else
		{
			iCount=ERROR_DATA_ABNORMAL;
			break;
		}
	}
    if(iOutLen>iCount)
        memset(bOutBuffer+iCount, 0, iOutLen-iCount);
	return iCount;
}

