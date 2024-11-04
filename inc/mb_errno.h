/**
  ******************************************************************************
  * @file    as3993_errno.h
  * @author  Albert
  * @version V1.0.0
  * @date    13-June-2017
  * @brief   Header for as3993 error definitions
  ******************************************************************************
	*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __AS_3993_ERRNO_H
#define __AS_3993_ERRNO_H

#ifdef __cplusplus
 extern "C" {
#endif

	 
/* Includes ------------------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
/* Exported functions --------------------------------------------------------*/

/*!
 *
 *  \brief Main error codes. Please add your application specific
 *  error codes in your application starting with
 *  \#define MY_APP_ERR_CODE (ERR_LAST_ERROR-1)
 *
 */

//----------------------------------------------------------------------------//

/*!
 * Error codes to be used within the application.
 * They are represented by an s8
 */
#define ERR_NONE        							((char)0)     										// no error occured
#define ERR_LAST_ERROR  							((signed char)-15)

//-------------------------------
// for SW protocol errors
#define ERR_CMD_FIRST_ERROR    	                        (ERR_LAST_ERROR - 1)		// (-16)
#define ERR_CMD_INCORRECT_CODE						    (ERR_LAST_ERROR - 2)
#define ERR_CMD_INCORRECT_PARAM						    (ERR_LAST_ERROR - 3)
#define ERR_CMD_DONT_RESPONSE							(ERR_LAST_ERROR - 4)
#define ERR_CMD_TABLE_OPENED							(ERR_LAST_ERROR - 5)
#define ERR_CMD_TABLE_CLOSED							(ERR_LAST_ERROR - 6)
#define ERR_CMD_TABLE_ENDED								(ERR_LAST_ERROR - 7)
#define ERR_CMD_INVALID_OPERATION					    (ERR_LAST_ERROR - 8)
#define ERR_CMD_LAST_ERROR         				        (ERR_LAST_ERROR - 9)

//----------------------------------------------------------------------------//

// for Barcode reader errors
#define ERR_BARC_OK                                     (ERR_NONE)

#define ERR_BARC_FIRST_ERROR    	                    (ERR_CMD_LAST_ERROR - 1)		// (-25)
#define ERR_BARC_CONFIG									(ERR_CMD_LAST_ERROR - 2)
#define ERR_BARC_READ									(ERR_CMD_LAST_ERROR - 3)
#define ERR_INVALID_BARCSTR								(ERR_CMD_LAST_ERROR - 4)

#define ERR_BARC_LAST_ERROR         			        (ERR_CMD_LAST_ERROR - 8)

//----------------------------------------------------------------------------//

// for application errors
#define ERR_MB700_OK                                    (ERR_NONE)
#define ERR_MB700_FIRST_ERROR    	                    (ERR_BARC_LAST_ERROR - 1)		// (-32)

#define ERR_MB700_IS_EMPTY								(ERR_BARC_LAST_ERROR - 2)
#define ERR_MB700_IS_FULL								(ERR_BARC_LAST_ERROR - 3)
#define ERR_MB700_EE_FULL								(ERR_BARC_LAST_ERROR - 4)
#define ERR_MB700_NO_DATA								(ERR_BARC_LAST_ERROR - 5)
#define ERR_MB700_SIZE_OVERSTEP							(ERR_BARC_LAST_ERROR - 6)

#define ERR_MB700_LAST_ERROR         			        (ERR_BARC_LAST_ERROR - 8)

#define ERR_WIFI_OK                                     (ERR_NONE)

#define ERR_WIFI_CONNECT_WIFI                           (ERR_MB700_LAST_ERROR - 1)		// (-39)
#define ERR_WIFI_CONNECT_SERVER                         (ERR_MB700_LAST_ERROR - 2)		// (-40)
#define ERR_WIFI_AP                                     (ERR_MB700_LAST_ERROR - 3)

#define ERR_WIFI_LAST_ERROR         			        (ERR_MB700_LAST_ERROR - 8)
#ifdef __cplusplus
}
#endif

#endif /* __AS_3993_ERRNO_H */

