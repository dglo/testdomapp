
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Modifications by John Jacobsen 2004 to implement configurable engineering events
*/
#include <stddef.h>

#if defined (CYGWIN) || defined(LINUX)
#include <stdio.h>
#endif

#include <string.h>
#include <stdio.h>

// DOM-related includes
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "domapp_common/DOMstateInfo.h"
#include "message/message.h"
#include "dataAccess/DOMdataCompression.h"
#include "dataAccess/moniDataAccess.h"
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
UBYTE *TimeMove(UBYTE *buffer, int useLatched);
void formatEngineeringEvent(UBYTE *buffer);
void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);

/** Set by initFormatEngineeringEvent: */
/* UBYTE ATWDCh0Mask; */
/* UBYTE ATWDCh1Mask; */
/* UBYTE ATWDCh2Mask; */
/* UBYTE ATWDCh3Mask; */
UBYTE ATWDChMask[4];

/* int ATWDCh0Len; */
/* int ATWDCh1Len; */
/* int ATWDCh2Len; */
/* int ATWDCh3Len; */
int ATWDChLen[4];

/* BOOLEAN ATWDCh0Byte; */
/* BOOLEAN ATWDCh1Byte; */
/* BOOLEAN ATWDCh2Byte; */
/* BOOLEAN ATWDCh3Byte; */
BOOLEAN ATWDChByte[4];

/* USHORT *ATWDCh0Data; */
/* USHORT *ATWDCh1Data; */
/* USHORT *ATWDCh2Data; */
/* USHORT *ATWDCh3Data; */
USHORT *ATWDChData[4];

USHORT *FlashADCData;

UBYTE FlashADCLen;
UBYTE MiscBits = 0;
UBYTE Spare = 0;

#define ATWDCHSIZ 128
USHORT Channel0Data[ATWDCHSIZ];
USHORT Channel1Data[ATWDCHSIZ];
USHORT Channel2Data[ATWDCHSIZ];
USHORT Channel3Data[ATWDCHSIZ];
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

  //ULONG FPGAeventIndex;
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

UBYTE *ATWDByteMoveFromBottom(USHORT *data, UBYTE *buffer, int count) {
  /** Old version of ATWDByteMove, which assumes waveform is sorted from early
      time to later */
    int i;

    for(i = 0; i < count; i++) {
	*buffer++ = (UBYTE)(*data++ & 0xff);
    }
    return buffer;
}

UBYTE *ATWDShortMoveFromBottom(USHORT *data, UBYTE *buffer, int count) {
  /** Old version of ATWDShortMove, which assumes waveform is sorted from early
      time to later */
    int i;
    UBYTE *ptr = (UBYTE *)data;

    for(i = 0; i < count; i++) {
	*buffer++ = *(ptr+1);
	*buffer++ = *ptr++;
	ptr++;
    }
    return buffer;
}


UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count) {
  /** Assumes earliest samples are last in the array */
    int i;

    data += ATWDCHSIZ-count;

    if(count > ATWDCHSIZ || count <= 0) return buffer;
    for(i = ATWDCHSIZ-count; i < ATWDCHSIZ; i++) {
	*buffer++ = (UBYTE)(*data++ & 0xff);
    }
    return buffer;
}


UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count) {
  /** Assumes earliest samples are last in the array */
    int i;

    UBYTE *ptr = (UBYTE *)data;

    ptr += (ATWDCHSIZ-count)*2;

    if(count > ATWDCHSIZ || count <= 0) return buffer; 
    for(i = ATWDCHSIZ - count; i < ATWDCHSIZ; i++) {
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

UBYTE *TimeMove(UBYTE *buffer, int useLatched) {
    int i;
    union DOMtime {unsigned long long time;
	UBYTE timeBytes[8];};
    union DOMtime t;

    //t.time = hal_FPGA_TEST_get_local_clock(); /* Old code didn't use latched ATWD time */
    if(useLatched) {
      if(FPGA_ATWD_select == 0) {
	t.time = hal_FPGA_TEST_get_atwd0_clock();
      } else {
	t.time = hal_FPGA_TEST_get_atwd1_clock();
      }
    } else {
      t.time = hal_FPGA_TEST_get_local_clock();
    }

    for(i = 0; i < 6; i++) {
	*buffer++ = t.timeBytes[5-i];
    }

    return buffer;
}

void initFormatEngineeringEvent(UBYTE fadc_samp_cnt_arg, 
				UBYTE atwd01_mask_arg,
				UBYTE atwd23_mask_arg) {
  int ich;
#define BSIZ 1024
  //UBYTE buf[BSIZ]; int n;
  //unsigned long long time;
  /* Endianness set to agree w/ formatEngineeringEvent */
  ATWDChMask[0] = atwd01_mask_arg & 0xF;
  ATWDChMask[1] = (atwd01_mask_arg >> 4) & 0xF;
  ATWDChMask[2] = atwd23_mask_arg & 0xF;
  ATWDChMask[3] = (atwd23_mask_arg >> 4) & 0xF;

  for(ich = 0; ich < 4; ich++) {
    switch(ATWDChMask[ich]) {
    case 1: //0b0001:
      ATWDChLen[ich]  = 32;
      ATWDChByte[ich] = TRUE;
      break;
    case 5: //0b0101:
      ATWDChLen[ich]  = 64;
      ATWDChByte[ich] = TRUE;
      break;
    case 9: //0b1001:
      ATWDChLen[ich]  = 16;
      ATWDChByte[ich] = TRUE;
      break;
    case 13: //0b1101:
      ATWDChLen[ich]  = 128;
      ATWDChByte[ich] = TRUE;
      break;
    case 3: //0b0011:
      ATWDChLen[ich]  = 32;
      ATWDChByte[ich] = FALSE;
      break;
    case 7: //0b0111:
      ATWDChLen[ich]  = 64;
      ATWDChByte[ich] = FALSE;
      break;
    case 11: //0b1011:
      ATWDChLen[ich]  = 16;
      ATWDChByte[ich] = FALSE;
      break;
    case 15: //0b1111:
      ATWDChLen[ich]  = 128;
      ATWDChByte[ich] = FALSE;
      break;
    default:
      ATWDChLen[ich]  = 0;
      ATWDChByte[ich] = TRUE;
      break;
    }
  }

  FlashADCLen = fadc_samp_cnt_arg;
  MiscBits = 0;
  Spare = 0;
  
  ATWDChData[0] = Channel0Data;
  ATWDChData[1] = Channel1Data;
  ATWDChData[2] = Channel2Data;
  ATWDChData[3] = Channel3Data;
  FlashADCData = FADCData;
}

void formatEngineeringEvent(UBYTE *event) {
#define BSIZ 1024
  //char buf[BSIZ]; int n;
  //unsigned long long time;
    UBYTE *beginOfEvent=event;
    USHORT length;
    int ich;

    //    int i;
    //UBYTE *ev=event;

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
    *event++ = (ATWDChMask[1]<<4) | ATWDChMask[0];
    *event++ = (ATWDChMask[3]<<4) | ATWDChMask[2];

    //printf("formatEngineeringEvent: header done, event: %x\r\n", event);

//  set trigger mode
    
    /* Form up the mask indicating which sort of event it was */

    /* set to match trigger mode values -DH */

    switch(FPGA_trigger_mode) {
    case TEST_PATTERN_TRIG_MODE:
      *event++ = 0;
      break;
    case CPU_TRIG_MODE:
      *event++ = 1;
      break;
    case TEST_DISC_TRIG_MODE:
      *event++ = 2;
      break;
    default:
      *event++ = 0x80;
      break;
    }
 
//  spare byte
    *event++ = Spare;
    
//  insert the time
    event = TimeMove(event, (FPGA_trigger_mode == TEST_DISC_TRIG_MODE ? 1 : 0));

//  do something with flash data
    event = FADCMove(FlashADCData, event, (int)FlashADCLen);

//  now the ATWD data
    for(ich = 0; ich < 4; ich++) {
      if(ATWDChByte[ich]) {
	event = ATWDByteMove(ATWDChData[ich], event, ATWDChLen[ich]);
      } else {
	event = ATWDShortMove(ATWDChData[ich], event, ATWDChLen[ich]);
      }
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
    for(i = 0; i < ATWDCHSIZ; i++) {
	Ch0Data[i] = i;
  	Ch1Data[i] = ATWDCHSIZ - i;
	Ch2Data[i] = i;
	Ch3Data[i] = ATWDCHSIZ - i;
    }

    for(i = 0; i < 256; i++) {
	FADC[i] = i;
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

    /* poison the atwd and fadc data to make sure we're reading
     * the real thing!
     *
     * this is for testing only!
     */
    if (Ch0Data!=NULL) memset(Ch0Data, 0xaa, ATWDCHSIZ*sizeof(short));
    if (Ch1Data!=NULL) memset(Ch1Data, 0xbb, ATWDCHSIZ*sizeof(short));
    if (Ch2Data!=NULL) memset(Ch2Data, 0xcc, ATWDCHSIZ*sizeof(short));
    if (Ch3Data!=NULL) memset(Ch3Data, 0xdd, ATWDCHSIZ*sizeof(short));
    if (FADC!=NULL) memset(FADC, 0xee, FlashADCLen*sizeof(short));

    for(i = 0; i < ATWD_TIMEOUT_COUNT; i++) {
	if(hal_FPGA_TEST_readout_done(trigger_mask)) {
	    //fprintf(stderr,"readout ready\r\n");

	    if(FPGA_ATWD_select == 0) {
	    	hal_FPGA_TEST_readout(Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 0, 0, 0, 0, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    else {
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
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
		    Ch2Data, Ch3Data, 0, 0, 0, 0, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    else {
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    return TRUE;
	}
	else {
	    halUSleep(ATWD_TIMEOUT_USEC);
	}
    }
    return FALSE;
}
