// ExpControlRoutines.c //
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Last Modification:
*/

// DOM-related includes
#include "hal/DOM_MB_hal_simul.h"
//#include "domapp_common/DOMtypes.h"
#include "domapp_common/DOMstateInfo.h"
#include "message/message.h"
#include "dataAccess/DOMdataCompression.h"
#include "expControl/expControl.h"
#include "expControl/EXPmessageAPIstatus.h"
#include "slowControl/DSCmessageAPIstatus.h"

/* define size of data buffer in message */
#define DATA_BUFFER_LEN MAXDATA_VALUE
 
/* defines for engineering events */
#define ENG_EVENT_FID 0x1;

/* global storage and functions */
extern UBYTE FPGA_trigger_mode;

/* local functions, data */
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;
extern UBYTE DOM_status;
extern UBYTE DOM_cmdSource;
extern ULONG DOM_constraints;
extern char *DOM_errorString;

// local functions, data
ULONG bufferWraps = 0;
ULONG bufferOverrun = 0;
ULONG eventsDiscarded = 0;
UBYTE lclEventCopy[BUFFER_ELEMENT_LEN];

// routines used for generating engineering events
UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *TimeMove(UBYTE *buffer);
void formatEngineeringEvent(UBYTE *buffer);
void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data);
void getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data);
void getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data);

// storage for generating engineering events
UBYTE ATWDCh0Mask;
UBYTE ATWDCh1Mask;
UBYTE ATWDCh2Mask;
UBYTE ATWDCh3Mask;

int ATWDCh0Len;
int ATWDCh1Len;
int ATWDCh2Len;
int ATWDCh3Len;

BOOLEAN ATWDCh0Byte;
BOOLEAN ATWDCh1Byte;
BOOLEAN ATWDCh2Byte;
BOOLEAN ATWDCh3Byte;

USHORT *ATWDCh0Data;
USHORT *ATWDCh1Data;
USHORT *ATWDCh2Data;
USHORT *ATWDCh3Data;

UBYTE FlashADCLen;
UBYTE MiscBits = 0;
UBYTE Spare = 0;

USHORT Channel0Data[128];
USHORT Channel1Data[128];
USHORT Channel2Data[128];
USHORT Channel3Data[128];

BOOLEAN beginRun() {
    if(DOM_state!=DOM_IDLE) {
	return FALSE;
    }
    else {
	DOM_state=DOM_RUN_IN_PROGRESS;
	return TRUE;
    }
}

BOOLEAN endRun() {
    if(DOM_state!=DOM_RUN_IN_PROGRESS) {
	return FALSE;
    }
    else {
	DOM_state=DOM_IDLE;
	return TRUE;
    }
}

BOOLEAN forceRunReset() {
    DOM_state=DOM_IDLE;
    return TRUE;
}

void initFillMsgWithData(void1) {
}

BOOLEAN checkDataAvailable() {

    return TRUE;
}

int fillMsgWithData(UBYTE *msgBuffer) {
    ULONG FPGAeventIndex;
    BOOLEAN done;
    UBYTE *dataBufferPtr = msgBuffer;
    UBYTE *tmpBufferPtr;

    // start things off
    done = FALSE;
    // notify compression routines that we are starting a new buffer
    dataBufferPtr=startDOMdataBuffer(msgBuffer);

    while(!done) {
#if defined (CYGWIN) || defined (LINUX)
	getPatternEvent(ATWDCh0Data, ATWDCh1Data,
		ATWDCh2Data, ATWDCh3Data);
#else
	switch (FPGA_trigger_mode) {
	    case CPU_TRIG_MODE:
		if(!getCPUEvent(lclEventCopy)) {
		    done = TRUE;
		}
		break;
	    case TEST_DISC_TRIG_MODE:
		if(!getTestDiscEvent(lclEventCopy)) {
		    done = TRUE;
	   	}
	    default:
		getPatternEvent(lclEventCopy);
	 	break;
	}
#endif

	if(!done) {
            //offer it to the data compression routine
            tmpBufferPtr = addAndCompressEvent(lclEventCopy,
                dataBufferPtr);
            if(tmpBufferPtr == 0) {
                //ran out of room
                done = TRUE;
            }
            else {
                //accept it
	        dataBufferPtr = tmpBufferPtr;
            }
	}
    }
    return (int)(dataBufferPtr-msgBuffer);
}

UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count) {
    int i;

    for(i = 0; i < count; i++) {
	*buffer++ = (UBYTE)(*data++ & 0xff);
    }
    return buffer;
}

UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count) {
    int i;
    UBYTE *ptr = (UBYTE *)data;

    for(i = 0; i < count; i++) {
	*buffer++ = *(ptr+1);
	*buffer++ = *ptr++;
	ptr++;
    }
    return buffer;
}

UBYTE *TimeMove(UBYTE *buffer) {
    int i;

    for(i = 0; i < 6; i++) {
	*buffer++ = i;
    }
    return buffer;
}

void formatEngineeringEvent(UBYTE *event) {
    UBYTE *beginOfEvent=event;
    USHORT length;

//  skip over event length
    event++;
    event++;

//  fill in the event ID
    *event++ = 0x0;
    *event++ = ENG_EVENT_FID;

//  various header info
    *event++ = MiscBits;
    *event++ = FlashADCLen;

//  ATWD masks
    *event++ = (ATWDCh0Mask<<4) & ATWDCh1Mask;
    *event++ = (ATWDCh2Mask<<4) & ATWDCh3Mask;

//  set trigger mode
    if(FPGA_trigger_mode == CPU_TRIG_MODE) {
  	*event++ = 0x0;
    }
    else if (FPGA_trigger_mode == TEST_DISC_TRIG_MODE) {
	*event++ = 0x10;
    }
    else {
	*event++ = 0x80;
    }

//  spare byte
    *event++ = Spare;

//  insert the time
    event = TimeMove(event);

//  now the ATWD data
    if(ATWDCh0Byte) {
	event = ATWDByteMove(ATWDCh0Data, event, ATWDCh0Len);
    }
    else {
	event = ATWDShortMove(ATWDCh0Data, event, ATWDCh0Len);
    }

    if(ATWDCh1Byte) {
        event = ATWDByteMove(ATWDCh1Data, event, ATWDCh1Len);
    }
    else {
        event = ATWDShortMove(ATWDCh1Data, event, ATWDCh1Len);
    }
    if(ATWDCh2Byte) {
        event = ATWDByteMove(ATWDCh2Data, event, ATWDCh2Len);
    }
    else {
        event = ATWDShortMove(ATWDCh2Data, event, ATWDCh2Len);
    }
    if(ATWDCh3Byte) {
        event = ATWDByteMove(ATWDCh3Data, event, ATWDCh3Len);
    }
    else {
        event = ATWDShortMove(ATWDCh3Data, event, ATWDCh3Len);
    }

//  do something with flash data

//  fix up length
    length = (USHORT)((event - beginOfEvent) & 0xffff);
    *beginOfEvent++ = (length << 8) & 0xff;
    *beginOfEvent++ = length & 0xff;
} 

void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
        USHORT *Ch2Data, USHORT *Ch3Data) {
    int i;

    for(i = 0; i < 128; i++) {
	Channel0Data[i] = i;
  	Channel1Data[i] = 128 - 1;
	Channel2Data[2] = i;
	Channel3Data[3] = 128 - i;
    }

    ATWDCh0Data = Channel0Data;
    ATWDCh1Data = Channel1Data;
    ATWDCh2Data = Channel2Data;
    ATWDCh3Data = Channel3Data;
}

