/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Experiment Control service thread to handle
	special run-related functions.  Performs all 
	"standard" DOM service functions.
Modification: 1/5/04 Jacobsen :-- add monitoring functionality
Modification: 1/19/04 Jacobsen :-- add configurable engineering event
*/

/* system include files */
#if defined (CYGWIN) || defined (LINUX)
#include <stdio.h>
#endif

#include <string.h>

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
#include "dataAccess/DOMdataCompression.h"
#include "dataAccess/moniDataAccess.h"
#include "domapp_common/DOMstateInfo.h"
#include "slowControl/DSCmessageAPIstatus.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);

/* external data */
extern UBYTE DOM_state;

/* global storage */
UBYTE FPGA_trigger_mode=CPU_TRIG_MODE;
int FPGA_ATWD_select=0;

/* Format masks: default, and configured by request */
#define DEF_FADC_SAMP_CNT  255
#define DEF_ATWD01_MASK    0xFF
#define DEF_ATWD23_MASK    0xFF


/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO datacs;

void dataAccessInit(void) {
  //MESSAGE_STRUCT *m;

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
    initFormatEngineeringEvent(DEF_FADC_SAMP_CNT, DEF_ATWD01_MASK, DEF_ATWD23_MASK);
    initDOMdataCompression(MAXDATA_VALUE);

}

/* data access  Entry Point */
void dataAccess(MESSAGE_STRUCT *M) {
#define BSIZ 1024
  //char buf[BSIZ]; int n;
  //unsigned long long time;

    UBYTE *data;
    int tmpInt;
    //UBYTE tmpByte;
    UBYTE *tmpPtr;
    //USHORT tmpShort;
    MONI_STATUS ms;
    ULONG moniHdwrIval, moniConfIval;
    int len;
    struct moniRec aMoniRec;
    int total_moni_len;
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
	      Message_setDataLen(M, DAC_ACC_DATA_AVAIL_LEN);
	      break;
	      
	    /*  check for available data */ 
	    case DATA_ACC_GET_DATA:
	      // try to fill in message buffer with waveform data
	      tmpInt = fillMsgWithData(data);
	      Message_setDataLen(M, tmpInt);
	      Message_setStatus(M, SUCCESS);
	      break;
	      
	      /* JEJ: Deal with configurable intervals for monitoring events */
	    case DATA_ACC_SET_MONI_IVAL:
	      moniHdwrIval = unformatLong(Message_getData(M));
	      moniConfIval = unformatLong(Message_getData(M)+sizeof(ULONG));
	      moniSetIvals(moniHdwrIval, moniConfIval);
	      //moniSetIvals(0,0x10000000);
	      //moniSetIvals(0x10000000, 0x10000000);
	      //moniSetIvals(0,0);
              Message_setDataLen(M, 0);
	      Message_setStatus(M, SUCCESS);
	      //moniInsertDiagnosticMessage("MONI GOT SET INTERVAL REQUEST", 0, 29);
              break;

	    /* JEJ: Deal with requests for monitoring events */
	    case DATA_ACC_GET_NEXT_MONI_REC:
	      ms = moniFetchRec(&aMoniRec);
	      switch(ms) {
	      case MONI_NOTINITIALIZED:
		datacs.msgRefused++;
		strcpy(datacs.lastErrorStr, DAC_MONI_NOT_INIT);
                datacs.lastErrorID = DAC_Moni_Not_Init;
		datacs.lastErrorSeverity = WARNING_ERROR;
                Message_setDataLen(M, 0);
                Message_setStatus(M, WARNING_ERROR);
		break;
	      case MONI_NODATA:
		Message_setDataLen(M, 0);
                Message_setStatus(M, SUCCESS);
		break;
	      case MONI_WRAPPED:
	      case MONI_OVERFLOW:
		Message_setDataLen(M, 0);
                strcpy(datacs.lastErrorStr, DAC_MONI_OVERFLOW);
		datacs.lastErrorID = DAC_Moni_Overrun;
                datacs.lastErrorSeverity = WARNING_ERROR;
                Message_setDataLen(M, 0);
                Message_setStatus(M, WARNING_ERROR);
                break;
	      case MONI_OK:
		moniAcceptRec();
		total_moni_len = aMoniRec.dataLen + 2 + 2 + 6; /* Total rec length */
		
		formatShort(total_moni_len, data);
		formatShort(aMoniRec.fiducial.fstruct.moniEvtType, data+2);

		/* Special case: big-endian 48-bit time, no format* function for it */
		data[9] = (aMoniRec.time >> 0) & 0xFF;
		data[8] = (aMoniRec.time >> 8) & 0xFF;
		data[7] = (aMoniRec.time >> 16) & 0xFF;
		data[6] = (aMoniRec.time >> 24) & 0xFF;
		data[5] = (aMoniRec.time >> 32) & 0xFF;
		data[4] = (aMoniRec.time >> 40) & 0xFF;
		
		//memcpy(data+4, &aMoniRec.time, MONI_TIME_LEN);
		len = 0;
		if(aMoniRec.dataLen > 0) {
		  /* Truncate to avoid overflow */
		  len = (aMoniRec.dataLen > MAXMONI_DATA) ? MAXMONI_DATA : aMoniRec.dataLen;
		  memcpy(data+10, aMoniRec.data, len);
		} 
		Message_setDataLen(M, 10+len);
		Message_setStatus(M, SUCCESS);
		break;
	      default:
		Message_setDataLen(M, 0);
                strcpy(datacs.lastErrorStr, DAC_MONI_BADSTAT);
                datacs.lastErrorID = DAC_Moni_Badstat;
                datacs.lastErrorSeverity = WARNING_ERROR;
                Message_setDataLen(M, 0);
                Message_setStatus(M, WARNING_ERROR);
                break;
	      }
	      break;
	    case DATA_ACC_SET_ENG_FMT:
	      Message_setDataLen(M, 0);
	      Message_setStatus(M, SUCCESS);
	      initFormatEngineeringEvent(data[0], data[1], data[2]);
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

