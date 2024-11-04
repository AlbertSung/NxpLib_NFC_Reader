/**
  ******************************************************************************
  * @file    queue.c
  * @author  Albert
  * @version V1.0.0
  * @date    6-July-2017
  * @brief   queue buffer sub-routine
  ******************************************************************************
    */


/* Includes ------------------------------------------------------------------*/
#include "queue.h"
#include "string.h"

#include "Porting_SAMD21.h"

#include "cmsis_gcc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

#if defined(NEW_QUEUE_LIB)
void Queue_Init(const Queue_Buffer_t* ptQueueBuffer)
{
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        ptQueueBuffer->ptData->uPutInx=0;
        ptQueueBuffer->ptData->uGetInx=0;
        memset(ptQueueBuffer->ptData->bDatas, 0, ptQueueBuffer->uMaxLength);
    }
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
void Queue_Discard(const Queue_Buffer_t* ptQueueBuffer)
{
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        __disable_irq();
        ptQueueBuffer->ptData->uPutInx=0;
        ptQueueBuffer->ptData->uGetInx=0;
        __enable_irq();
    }
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
static u16 Queue_IndexInRange(u16 uIndex, u16 uMaxLength)
{
    if(uMaxLength<=uIndex)
    {
        uIndex=0;
    }
    return uIndex;
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
u16 Queue_GetCount(const Queue_Buffer_t* ptQueueBuffer)
{
    u16 uCount;
    uCount=0;
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        u16 uGetInx;
        u16 uPutInx;
        u16 uMaxLen;
        uMaxLen=ptQueueBuffer->uMaxLength;
        uPutInx=Queue_IndexInRange(ptQueueBuffer->ptData->uPutInx, uMaxLen);
        uGetInx=Queue_IndexInRange(ptQueueBuffer->ptData->uGetInx, uMaxLen);
        uCount=(uPutInx-uGetInx);
        if(uMaxLen<=uCount)
        {
            uCount+=uMaxLen;
        }
    }
    return uCount;
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
u16 Queue_GetSpaceCount(const Queue_Buffer_t* ptQueueBuffer)
{
    u16 uSpaceCount;
    uSpaceCount=0;
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        uSpaceCount=(ptQueueBuffer->uMaxLength-Queue_GetCount(ptQueueBuffer));
        if(0<uSpaceCount)
        {
            uSpaceCount--;
        }
    }
    return uSpaceCount;
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
bool Queue_Put(const Queue_Buffer_t* ptQueueBuffer, u8 bData)
{
    bool bIsPutData;
    bIsPutData=false;
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        u16 uPutNewInx;
        u16 uPutInx;
        __disable_irq();
        uPutInx=ptQueueBuffer->ptData->uPutInx;
        uPutNewInx=Queue_IndexInRange((uPutInx+1), ptQueueBuffer->uMaxLength);
        if(uPutNewInx!=ptQueueBuffer->ptData->uGetInx)
        {
            ptQueueBuffer->ptData->bDatas[uPutInx]=bData;
            ptQueueBuffer->ptData->uPutInx=uPutNewInx;
            bIsPutData=true;
        }
        __enable_irq();
    }
    return bIsPutData;
}
#endif //#if defined(NEW_QUEUE_LIB)

#if defined(NEW_QUEUE_LIB)
bool Queue_Get(const Queue_Buffer_t* ptQueueBuffer, u8* pbData)
{
    bool bIsGetData;
    bIsGetData=false;
#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    if(NULL!=ptQueueBuffer)
#endif //#if defined(NEW_QUEUE_LIB_CHECK_NULL_POINTER)
    {
        if(ptQueueBuffer->ptData->uGetInx!=ptQueueBuffer->ptData->uPutInx)
        {   // Do not disable irq before check GetIdx!=uPutInx
            u16 uGetInx;
            __disable_irq();
            uGetInx=ptQueueBuffer->ptData->uGetInx;
            *pbData=ptQueueBuffer->ptData->bDatas[uGetInx];
            ptQueueBuffer->ptData->uGetInx=Queue_IndexInRange((uGetInx+1), ptQueueBuffer->uMaxLength);
            __enable_irq();
            bIsGetData=true;
        }
    }
    return bIsGetData;
}
#endif //#if defined(NEW_QUEUE_LIB)

/******************************************************************************/

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval None
  */
void QueueInit(queueBuf_t* ptQueue)
{
    memset(ptQueue->bBuffer, 0x0, UART_QUEUE_SIZE);
    ptQueue->bPutInx = 0;
    ptQueue->bGetInx = 0;
    ptQueue->bLength = 0;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval None
  */
void QueueSkip(queueBuf_t* ptQueue)
{
    ptQueue->bGetInx = ptQueue->bPutInx;
    ptQueue->bLength = 0;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval 
  */
u8 QueuePut(queueBuf_t* ptQueue, u8 bData)
{
    bool State;
    State=false;
    if(UART_QUEUE_SIZE>ptQueue->bLength)
    {
        State=true;
        __disable_irq();
        ptQueue->bBuffer[ptQueue->bPutInx] = bData;

        if(++(ptQueue->bPutInx) == UART_QUEUE_SIZE)
            ptQueue->bPutInx = 0;

        ptQueue->bLength++;
        __enable_irq();
    }

    return State;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval 
  */
u8 QueueGet(queueBuf_t* ptQueue, u8* bData)
{
    bool State;
    State=false;
    if(0!=ptQueue->bLength)
    {
        State=true;
        __disable_irq();
        *bData = ptQueue->bBuffer[ptQueue->bGetInx];

        if(++(ptQueue->bGetInx) == UART_QUEUE_SIZE)
        {
            ptQueue->bGetInx=0;
        }

        ptQueue->bLength--;
        __enable_irq();
    }
    return State;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

/******************************************************************************/

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval None
  */
void BIG_QueueInit(BIG_queueBuf_t* ptBigQueue)
{
    memset(ptBigQueue->bBuffer, 0x0, BIG_QUEUE_SIZE);
    ptBigQueue->bPutInx = 0;
    ptBigQueue->bGetInx = 0;
    ptBigQueue->bLength = 0;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval None
  */
void BIG_QueueSkip(BIG_queueBuf_t* ptBigQueue)
{
    ptBigQueue->bGetInx = ptBigQueue->bPutInx;
    ptBigQueue->bLength = 0;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval 
  */
u8 BIG_QueuePut(BIG_queueBuf_t* ptBigQueue, u8 bData)
{
    bool State;
    State=false;
    if(BIG_QUEUE_SIZE<=ptBigQueue->bLength)
    {
        State = true;
        __disable_irq();
        ptBigQueue->bBuffer[ptBigQueue->bPutInx] = bData;

        if(++(ptBigQueue->bPutInx) == BIG_QUEUE_SIZE)
                ptBigQueue->bPutInx = 0;

        ptBigQueue->bLength++;
        __enable_irq();
    }

    return State;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

#if (!defined(NEW_QUEUE_LIB))
/**
  * @brief  
  * @param  
  * @retval 
  */
u8 BIG_QueueGet(BIG_queueBuf_t* ptBigQueue, u8* bData)
{
    bool State;
    State = false;
    if(0!=ptBigQueue->bLength)
    {
        State = true;
        __disable_irq();
        *bData = ptBigQueue->bBuffer[ptBigQueue->bGetInx];

        if(++(ptBigQueue->bGetInx) == BIG_QUEUE_SIZE)
                ptBigQueue->bGetInx = 0;

        ptBigQueue->bLength--;
        __enable_irq();
    }
    return State;
}
#endif //#if (!defined(NEW_QUEUE_LIB))

