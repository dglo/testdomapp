
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Last Modification:
*/

#if defined (CYGWIN) || defined(LINUX)
#include <stdio.h>
#endif

// DOM-related includes
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
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

/* defines for ATWD readout */
#define ATWD_TIMEOUT_COUNT 20
#define ATWD_TIMEOUT_USEC 1000

/* global storage and functions */
extern UBYTE FPGA_trigger_mode;
extern int FPGA_ATWD_select;

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
UBYTE lclEventCopy[2000];

// routines used for generating engineering events
UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *TimeMove(UBYTE *buffer);
void initFormatEngineeringEvent(void);
void formatEngineeringEvent(UBYTE *buffer);
void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
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
USHORT *FlashADCData;

UBYTE FlashADCLen;
UBYTE MiscBits = 0;
UBYTE Spare = 0;

USHORT Channel0Data[128];
USHORT Channel1Data[128];
USHORT Channel2Data[128];
USHORT Channel3Data[128];
USHORT FADCData[256];

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

void initFillMsgWithData(void) {
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

    //fprintf(stderr,"fillMsgWithData\r\n");

    while(!done) {
	switch (FPGA_trigger_mode) {
	    case TEST_PATTERN_TRIG_MODE:
		getPatternEvent(Channel0Data, Channel1Data,
		    Channel2Data, Channel3Data, FADCData);
		break;
	    case CPU_TRIG_MODE:
		if(!getCPUEvent(Channel0Data, Channel1Data,
		    Channel2Data, Channel3Data, FADCData)) {
		    done = TRUE;
		}
		break;
	    case TEST_DISC_TRIG_MODE:
		if(!getTestDiscEvent(Channel0Data, Channel1Data,
		    Channel2Data, Channel3Data, FADCData)) {
		    done = TRUE;
	   	}
		break;
	    default:
		getPatternEvent(Channel0Data, Channel1Data, 
		    Channel2Data, Channel3Data, FADCData);
	 	break;
	}


	if(!done) {
	    //fprintf(stderr,"dataAccessRoutines: formatting event\r\n");
	    formatEngineeringEvent(lclEventCopy);
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
   
    //fprintf(stderr,"fillMsgWithData: bufferLength: %d\r\n",
	//(int)(dataBufferPtr-msgBuffer));
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

UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count) {
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
    union DOMtime {unsigned long long time;
	UBYTE timeBytes[8];};
    union DOMtime t;

    t.time = hal_FPGA_TEST_get_local_clock();
    for(i = 0; i < 6; i++) {
	*buffer++ = t.timeBytes[6-i];
    }

    return buffer;
}

void initFormatEngineeringEvent(void) {
    ATWDCh0Mask = 0xf;
    ATWDCh1Mask = 0xf;
    ATWDCh2Mask = 0xf;
    ATWDCh3Mask = 0xf;

    ATWDCh0Len = 128;
    ATWDCh1Len = 128;
    ATWDCh2Len = 128;
    ATWDCh3Len = 128;

    ATWDCh0Byte = FALSE;
    ATWDCh1Byte = FALSE;
    ATWDCh2Byte = FALSE;
    ATWDCh3Byte = FALSE;

    FlashADCLen = 255;
    MiscBits = 0;
    Spare = 0;

    ATWDCh0Data = Channel0Data;
    ATWDCh1Data = Channel1Data;
    ATWDCh2Data = Channel2Data;
    ATWDCh3Data = Channel3Data;
    FlashADCData = FADCData;
}

void formatEngineeringEvent(UBYTE *event) {
    UBYTE *beginOfEvent=event;
    USHORT length;

    int i;
    UBYTE *ev=event;

    //printf("formatEngineeringEvent: starting, event: %x\r\n", event);

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
    *event++ = (ATWDCh0Mask<<4) | ATWDCh1Mask;
    *event++ = (ATWDCh2Mask<<4) | ATWDCh3Mask;

    //printf("formatEngineeringEvent: header done, event: %x\r\n", event);

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

    //fprintf(stderr,"formatEngineeringEvent: trigger done, event: %x\r\n", event);

//  do something with flash data
    event = FADCMove(FlashADCData, event, (int)FlashADCLen);

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

    //fprintf(stderr,"formatEngineeringEvent:  done, event: %x\r\n", event);

    length = (USHORT)((event - beginOfEvent) & 0xffff);
    *beginOfEvent++ = (length >> 8) & 0xff;
    *beginOfEvent++ = length & 0xff;

/*
    printf("formatEngineeringEvent: length: %d\r\n", length);
    for(i=0;i<=length/10;i++) {
	printf("%d: %x %x %x %x %x %x %x %x %x %x\r\n",
	    i*10, *ev++, *ev++,*ev++,*ev++,*ev++,*ev++,
	    *ev++,*ev++,*ev++,*ev++);
    }
*/

} 

void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
        USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;

    for(i = 0; i < 128; i++) {
	Channel0Data[i] = i;
  	Channel1Data[i] = 128 - i;
	Channel2Data[i] = i;
	Channel3Data[i] = 128 - i;
    }

    for(i = 0; i < 256; i++) {
	FADCData[i] = i;
    }
}

BOOLEAN getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;
    UBYTE trigger_mask;
    
    //fprintf(stderr, "getCPUEvent\r\n");

    if(FPGA_ATWD_select == 0) {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    else {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    hal_FPGA_TEST_trigger_forced(trigger_mask);

    for(i = 0; i < ATWD_TIMEOUT_COUNT; i++) {
	if(hal_FPGA_TEST_readout_done(trigger_mask)) {
	    //fprintf(stderr,"readout ready\r\n");

	    if(FPGA_ATWD_select == 0) {
	    	hal_FPGA_TEST_readout(Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 0, 0, 0, 0, 128, 
		    FADC, (int)FlashADCLen, FPGA_ATWD_select);
	    }
	    else {
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 128, 
		    FADC, (int)FlashADCLen, FPGA_ATWD_select);
	    }
	    return TRUE;
	}
	else {
	    halUSleep(ATWD_TIMEOUT_USEC);
	}
    }
    return FALSE;
}

BOOLEAN getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;
    UBYTE trigger_mask;
    
    if(FPGA_ATWD_select == 0) {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    else {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    hal_FPGA_TEST_trigger_disc(trigger_mask);

    for(i = 0; i < ATWD_TIMEOUT_COUNT; i++) {
	if(hal_FPGA_TEST_readout_done(trigger_mask)) {
	    if(FPGA_ATWD_select == 0) {
	    	hal_FPGA_TEST_readout(Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 0, 0, 0, 0, 128, 
		    FADC, (int)FlashADCLen, FPGA_ATWD_select);
	    }
	    else {
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 128, 
		    FADC, (int)FlashADCLen, FPGA_ATWD_select);
	    }
	    return TRUE;
	}
	else {
	    halUSleep(ATWD_TIMEOUT_USEC);
	}
    }
    return FALSE;
}
