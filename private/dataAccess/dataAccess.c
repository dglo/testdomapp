
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Experiment Control service thread to handle
	special run-related functions.  Performs all 
	"standard" DOM service functions.
Last Modification:
*/

/* system include files */
#include <stdio.h>

/* DOM-related includes */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "message/message.h"
#include "dataAccess/dataAccess.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonServices.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "dataAccess/DACmessageAPIstatus.h"
#include "dataAccess/dataAccessRoutines.h"
#include "domapp_common/DOMstateInfo.h"
#include "slowControl/DSCmessageAPIstatus.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);

/* external data */
extern UBYTE DOM_state;

/* global storage */
UBYTE FPGA_trigger_mode=CPU_TRIG_MODE;

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO datacs;

void dataAccessInit(void) {
    datacs.state = SERVICE_ONLINE;
    datacs.lastErrorID = COMMON_No_Errors;
    datacs.lastErrorSeverity = INFORM_ERROR;
    datacs.majorVersion = DAC_MAJOR_VERSION;
    datacs.minorVersion = DAC_MINOR_VERSION;
    strcpy(datacs.lastErrorStr, DAC_ERS_NO_ERRORS);
    datacs.msgReceived = 0;
    datacs.msgRefused = 0;
    datacs.msgProcessingErr = 0;

    /* now initialize the data filling routines */
    initFillMsgWithData();
}

/* data access  Entry Point */
void dataAccess(MESSAGE_STRUCT *M) {

    UBYTE *data;
    int tmpInt;
    UBYTE tmpByte;
    UBYTE *tmpPtr;
    USHORT tmpShort;
    int i;


	/* get address of data portion. */
	/* Receiver ALWAYS links a message */
	/* to a valid data buffer-even */ 
	/* if it is empty. */
	data = Message_getData(M);
	datacs.msgReceived++;
	switch ( Message_getSubtype(M) ) {
	    /* Manditory Service SubTypes */
	    case GET_SERVICE_STATE:
	        /* get current state of Data Access */
	    	data[0] = datacs.state;
	    	Message_setDataLen(M, GET_SERVICE_STATE_LEN);
	    	Message_setStatus(M, SUCCESS);
	    	break;

	    case GET_LAST_ERROR_ID:
	        /* get the ID of the last error encountered */
		data[0] = datacs.lastErrorID;
		data[1] = datacs.lastErrorSeverity;
		Message_setDataLen(M, GET_LAST_ERROR_ID_LEN);
		Message_setStatus(M, SUCCESS);
		break;

	    case GET_SERVICE_VERSION_INFO:
		/* get the major and minor version of this */
		/*	Data Access */
		data[0] = datacs.majorVersion;
		data[1] = datacs.minorVersion;
		Message_setDataLen(M, GET_SERVICE_VERSION_INFO_LEN);
		Message_setStatus(M, SUCCESS);
		break;

	    case GET_SERVICE_STATS:
		/* get standard service statistics for */
		/*	the Data Access */
		formatLong(datacs.msgReceived, &data[0]);
		formatLong(datacs.msgRefused, &data[4]);
		formatLong(datacs.msgProcessingErr, &data[8]);
		Message_setDataLen(M, GET_SERVICE_STATS_LEN);
		Message_setStatus(M, SUCCESS);
		break;

	    case GET_LAST_ERROR_STR:
		/* get error string for last error encountered */
		strcpy(data,datacs.lastErrorStr);
		Message_setDataLen(M, strlen(datacs.lastErrorStr));
		Message_setStatus(M, SUCCESS);
		break;

	    case CLEAR_LAST_ERROR:
		/* reset both last error ID and string */
		datacs.lastErrorID = COMMON_No_Errors;
		datacs.lastErrorSeverity = INFORM_ERROR;
		strcpy(datacs.lastErrorStr, DAC_ERS_NO_ERRORS);
		Message_setDataLen(M, 0);
		Message_setStatus(M, SUCCESS);
		break;

	    case REMOTE_OBJECT_REF:
		/* remote object reference */
		/*	TO BE IMPLEMENTED..... */
		datacs.msgProcessingErr++;
		strcpy(datacs.lastErrorStr, DAC_ERS_BAD_MSG_SUBTYPE);
		datacs.lastErrorID = COMMON_Bad_Msg_Subtype;
		datacs.lastErrorSeverity = WARNING_ERROR;
		Message_setDataLen(M, 0);
		Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
		break;

	    case GET_SERVICE_SUMMARY:
		/* init a temporary buffer pointer */
		tmpPtr = data;
		/* get current state of slow control */
		*tmpPtr++ = datacs.state;
		/* get the ID of the last error encountered */
		*tmpPtr++ = datacs.lastErrorID;
		*tmpPtr++ = datacs.lastErrorSeverity;
		/* get the major and minor version of this */
		/*	exp control */
		*tmpPtr++ = datacs.majorVersion;
		*tmpPtr++ = datacs.minorVersion;
		/* get standard service statistics for */
		/*	the exp control */
		formatLong(datacs.msgReceived, tmpPtr);
		tmpPtr += sizeof(ULONG);
		formatLong(datacs.msgRefused, tmpPtr);
		tmpPtr += sizeof(ULONG);
		formatLong(datacs.msgProcessingErr, tmpPtr);
		tmpPtr += sizeof(ULONG);
		/* get error string for last error encountered */
		strcpy(tmpPtr, datacs.lastErrorStr);
		Message_setDataLen(M, (int)(tmpPtr-data) +
			strlen(datacs.lastErrorStr));
		Message_setStatus(M, SUCCESS);
		break;

	    /*-------------------------------*/
	    /* Data Access specific SubTypes */

	    /*  check for available data */ 
	    case DATA_ACC_DATA_AVAIL:
		data[0] = checkDataAvailable();
		Message_setStatus(M, SUCCESS);
		Message_setDataLen(M, DAC_data_avail);
		break;

	    /*  check for available data */ 
	    case DATA_ACC_GET_DATA:
		// try to fill in message buffer with waveform data
		tmpInt = fillMsgWithData(data);
		Message_setDataLen(M, tmpInt);
		Message_setStatus(M, SUCCESS);
		break;

	    /*----------------------------------- */
	    /* unknown service request (i.e. message */
	    /*	subtype), respond accordingly */
	    default:
		datacs.msgRefused++;
		strcpy(datacs.lastErrorStr, DAC_ERS_BAD_MSG_SUBTYPE);
		datacs.lastErrorID = COMMON_Bad_Msg_Subtype;
		datacs.lastErrorSeverity = WARNING_ERROR;
		Message_setDataLen(M, 0);
		Message_setStatus(M,
		    UNKNOWN_SUBTYPE|WARNING_ERROR);
		break;

	}

}

