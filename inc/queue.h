/**
  ******************************************************************************
  * @file    queue.h
  * @author  Albert
  * @version V1.0.0
  * @date    6-July-2017
  * @brief   Header for queue.c module
  ******************************************************************************
    */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __QUEUE_H
#define __QUEUE_H

#ifdef __cplusplus
 extern "C" {
#endif

#define NEW_QUEUE_LIB
     
/* Includes ------------------------------------------------------------------*/
#include "ams_types.h"

/* Defined constants ---------------------------------------------------------*/

// Exported types ------------------------------------------------------------
#if defined(NEW_QUEUE_LIB)
#define Queue_BufferDataBase(BufferSize)                    \
struct                                                      \
{                                                           \
    volatile u16 uPutInx;                                   \
    volatile u16 uGetInx;                                   \
    u8 bDatas[BufferSize];                                  \
}
  
typedef Queue_BufferDataBase(1) Queue_BufferData_t;
// Queue_BufferData_t must put at read/write data (RAM)

typedef const struct
{
    u16 uMaxLength;
    Queue_BufferData_t* ptData;
} Queue_Buffer_t;
// Queue_Buffer_t must put at read-only data (const)

#define Queue_StaticAlloc(BufferSize)                       ((Queue_BufferData_t*)&(Queue_BufferDataBase(BufferSize)){0})
#define Queue_CreateBuffer(QueueName, BufferSize)           \
const Queue_Buffer_t QueueName=                             \
{                                                           \
    .uMaxLength=BufferSize,                                 \
    .ptData=Queue_StaticAlloc(BufferSize)                   \
}

#define Queue_CreateDebugBuffer(QueueName, BufferSize)      \
Queue_BufferDataBase(BufferSize) QueueName##_Data;          \
const Queue_Buffer_t QueueName=                             \
{                                                           \
    .uMaxLength=BufferSize,                                 \
    .ptData=((Queue_BufferData_t*)(&QueueName##_Data))      \
}
#endif //#if defined(NEW_QUEUE_LIB)

/* Exported types ------------------------------------------------------------*/
#if (!defined(NEW_QUEUE_LIB))
typedef struct
{
    u8 bBuffer[UART_QUEUE_SIZE];
    u16 bPutInx;
    u16 bGetInx;
    u16 bLength;
} queueBuf_t;
#endif //#if (!defined(NEW_QUEUE_LIB))

/* Exported types ------------------------------------------------------------*/
#if (!defined(NEW_QUEUE_LIB))
typedef struct
{
    u8 bBuffer[BIG_QUEUE_SIZE];
    u16 bPutInx;
    u16 bGetInx;
    u16 bLength;
} BIG_queueBuf_t;
#endif //#if (!defined(NEW_QUEUE_LIB))

/* Exported constants --------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/
#if (!defined(NEW_QUEUE_LIB))
void QueueInit(queueBuf_t* ptQueue);
u8 QueuePut(queueBuf_t* ptQueue, u8 bData);
u8 QueueGet(queueBuf_t* ptQueue, u8* bData);

void BIG_QueueInit(BIG_queueBuf_t* ptQueue);
u8 BIG_QueuePut(BIG_queueBuf_t* ptQueue, u8 bData);
u8 BIG_QueueGet(BIG_queueBuf_t* ptQueue, u8* bData);
#endif //#if (!defined(NEW_QUEUE_LIB))

#if defined(NEW_QUEUE_LIB)
void Queue_Init(const Queue_Buffer_t* ptQueueBuffer);
void Queue_Discard(const Queue_Buffer_t* ptQueueBuffer);
u16 Queue_GetCount(const Queue_Buffer_t* ptQueueBuffer);
u16 Queue_GetSpaceCount(const Queue_Buffer_t* ptQueueBuffer);
bool Queue_Put(const Queue_Buffer_t* ptQueueBuffer, u8 bData);
bool Queue_Get(const Queue_Buffer_t* ptQueueBuffer, u8* pbData);
#define QueueInit(ptQueueBuffer)                            Queue_Init(ptQueueBuffer)
#define QueuePut(ptQueueBuffer, bData)                      Queue_Put(ptQueueBuffer, bData)
#define QueueGet(ptQueueBuffer, pbData)                     Queue_Get(ptQueueBuffer, pbData)
#endif //#if defined(NEW_QUEUE_LIB)

#ifdef __cplusplus
}
#endif

#endif /* __QUEUE_H */
