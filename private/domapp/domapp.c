/*
 * @mainpage domapp - DOM Application program
 * @author Chuck McParland originally, now updated and maintained by 
 * J. Jacobsen (jacobsen@npxdesigns.com)
 * Based on original code by mcp.
 *
 * $Date: 2004-07-14 22:57:47 $
 *
 *
 */

/**
 * @file domapp.c
 * 
 * This file contains low level initialization routines used to
 * settup and manage the environment used in simulating execution of
 * the DOM application on other platforms.
 *
 * $Revision: 1.28 $
 * $Author: jacobsen $
 * Based on original code by Chuck McParland
 * $Date: 2004-07-14 22:57:47 $
*/

#include <unistd.h> /* Needed for read/write */
#include <stdio.h> /* snprintf */

#if defined (CYGWIN) || defined (LINUX)
/** system include files */
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>   /* for time() */
#include <signal.h> /* For signal() */
#include <errno.h>  /* For errno, on open */
#include <fcntl.h>

#endif

// DOM-related includes
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_pld.h"
#include "domapp_common/DOMstateInfo.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "dataAccess/DOMdataCompression.h"
#include "dataAccess/moniDataAccess.h"
#include "expControl/expControl.h"
#include "expControl/EXPmessageAPIstatus.h"
#include "slowControl/DSCmessageAPIstatus.h"
#include "dataAccess/moniDataAccess.h"
#include "msgHandler/msgHandler.h"
#include "slowControl/domSControl.h"
#include "dataAccess/dataAccess.h"
#include "message/genericMsgSendRecv.h"

int recvMsg_arm_nonblock(void);

/** Monitoring delay --- now set by dataAccess message */
//#define DOMAPP_MONI_DELAY 0xA000000

/** fds index for stdin, stdout */
#define STDIN 0
#define STDOUT 1

/** Where we get messages from : either STDIN or an open device file descriptor */
int dom_input_file;      
/** Where we send messages to : either STDOUT or an open device file descriptor */
int dom_output_file;

/** error reporting flags, mostly just for information */
#define ERROR -1
#define COM_ERROR -2
#define COM_EOF   -3
#define COM_EAGAIN -4

/** maximum values for some MESSAGE_STRUCT fields */
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255

/** timeout values for select timer struct */
#define TIMEOUT_10MSEC 10000
#define TIMEOUT_100MSEC 100000

/** routines to handle send and receive of messages through stdin/out */
int recvMsg(void);
int sendMsg(void);
int readmsg(int, MESSAGE_STRUCT *, char *);

/** packet driver counters, etc. For compatibility purposes only */
ULONG PKTrecv;
ULONG PKTsent;
ULONG NoStorage;
ULONG FreeListCorrupt;
ULONG PKTbufOvr;
ULONG PKTbadFmt;
ULONG PKTspare;

ULONG MSGrecv;
ULONG MSGsent;
ULONG tooMuchData;
ULONG IDMismatch;
ULONG CRCproblem;

/* single message buffer pointer to use */
MESSAGE_STRUCT *messageBuffer;

/* Monitoring buffer, static allocation: */
UBYTE monibuf[MONI_CIRCBUF_RECS * MONI_REC_SIZE];

/**
 * Main entry that starts the DOM application off.  Should
 * never return unless error encountered.
 *
 * @param integer ID to be used as DOM ID for this simulation.
 *	If not present default value of 1234 is used.
 *
 * @return ERROR or COM_ERROR, no non-error returns possible.
 *
 * @see domappFile.c for file based version of same program.
*/


#define pfprintf(...) /* makes it easier to comment out, 
			 when you want less verbose messages */
#if defined (CYGWIN) || (LINUX)
static FILE *log;
volatile sig_atomic_t fatal_error_in_progress = 0;
void cleanup_exit(int);
#endif



#if defined  (CYGWIN) || (LINUX)
int main(int argc, char* argv[]) {
    /** storage */
#define DA_MAX_LOGNUM 6
  char id6dig[DA_MAX_LOGNUM+1];
  char logname[] = "dom_log_XXXXXX.txt"; /* Replace Xs w/ low bits of DOM ID */
  int i;
  int j;
  int k;
  int dataLen;
  int send;
  int receive;
  int port;
  int nready;
  int dom_simulation_file; 
  long fcntl_flags;
  int result;
  UBYTE b;
#else
int main(void) {
#endif
    int status;
    unsigned long long t_hw_last, t_cf_last, tcur;
    unsigned long long moni_hardware_interval, moni_config_interval;

    //    struct pollfd fds[1];

#if defined (CYGWIN) || defined (LINUX)
    fd_set fds;
    struct timeval timeout;

    setvbuf(stdout, (char *)0, _IONBF, 0);

    /* read args and configure */
    if (argc >= 3) {
      /* insert ID and name into hal simulation for later use.
       ID length is important, so make sure its 6 chars */

      if(strlen(argv[1])!=12) {
	halSetBoardID("123456123456");
      } else {
        halSetBoardID(argv[1]);
      }
      halSetBoardName(argv[2]);
    } else {
      /* must have id and name to run, make something up */
      halSetBoardID("000000000000");
      halSetBoardName("none");
    }

    strncpy(id6dig,halGetBoardID()+6, DA_MAX_LOGNUM);
    id6dig[DA_MAX_LOGNUM] = '\0';

    sprintf(logname,"dom_log_%s.txt", id6dig);
    
    log = fopen(logname, "w"); 
    //log = stderr; /* Do this if you want messages to stderrinstead
    //		       of log file */

    fprintf(log, "domapp: opened log file.\n");

    /* standard hello msg, will remove eventually */
    fprintf(log,"domapp: argc=%d, argv[0]=%s\n\r", argc, argv[0]);

    /* show what dom we're running as */
    fprintf(log,"domapp: executing as DOM #%s, %s\n\r",
      halGetBoardID(), halGetBoardName());

    /* Install signal handler so that socket is closed when
       domapp is killed. JEJ */
    signal(SIGHUP, &cleanup_exit);
    signal(SIGINT, &cleanup_exit);
    signal(SIGTERM, &cleanup_exit);

#endif

    dom_input_file  = STDIN;
    dom_output_file = STDOUT;

    t_hw_last = t_cf_last = hal_FPGA_TEST_get_local_clock();

    /* Set input to non-blocking mode -- NOT IMPLEMENTED ON EPXA10 */
    //fcntl_flags = fcntl(dom_input_file, F_GETFL, 0);
    //result = fcntl(dom_input_file, F_SETFL, fcntl_flags|O_NONBLOCK);
    
    /* init messageBuffers */
    messageBuffers_init();
    //fprintf(stderr,"domapp: Ready to go\r\n");

    /* Start up monitoring system -- do this before other *Init()'s because
       they may want to insert monitoring information */
    moniInit(monibuf, MONI_MASK);
    moniRunTests();
    //moniTestAllMonitorRecords();

    /* manually init all the domapp components */
    msgHandlerInit();
    domSControlInit();
    expControlInit();
    dataAccessInit();

    halEnableBarometer(); /* Increases power consumption slightly but 
			     enables power to be read out */
    halStartReadTemp();
    USHORT temperature = 0; // Chilly

    for (;;) {
      
      /* Insert periodic monitoring records */
      tcur = hal_FPGA_TEST_get_local_clock();      
      moni_hardware_interval = moniGetHdwrIval();
      moni_config_interval   = moniGetConfIval();
      long long dt  = tcur-t_hw_last;

      if(moni_hardware_interval > 0) {
	if(dt < 0  /* overflow case (should be RARE)  */
	   || dt > moni_hardware_interval) {

	  /* Update temperature if it's done */
	  if(halReadTempDone()) {
	    temperature = halFinishReadTemp();
	    halStartReadTemp();
	  }
	  //temperature = halReadTemp();
	  //temperature = 666;
	  moniInsertHdwrStateMessage(tcur, temperature);

#ifdef DEBUGMONI
          mprintf("MONI "
		  "tcur=0x%08lx%08lx "
		  "hwlast=0x%08lx%08lx "
		  "cflast=0x%08lx%08lx "
		  "hwival=0x%lx%lx "
		  "cfival=0x%lx%lx "
		  "dt=%ld ", 
		  (unsigned long) (tcur>>32 & 0xFFFFFFFF),
		  (unsigned long) (tcur & 0xFFFFFFFF),
		  (unsigned long) (t_hw_last>>32 & 0xFFFFFFFF),
		  (unsigned long) (t_hw_last & 0xFFFFFFFF),
		  (unsigned long) (t_cf_last>>32 & 0xFFFFFFFF),
		  (unsigned long) (t_cf_last & 0xFFFFFFFF),
		  (unsigned long) (moni_hardware_interval>>32 & 0xFFFFFFFF),
		  (unsigned long) (moni_hardware_interval & 0xFFFFFFFF),
		  (unsigned long) (moni_config_interval>>32 & 0xFFFFFFFF),
		  (unsigned long) (moni_config_interval & 0xFFFFFFFF),
		  (unsigned long) dt);
#endif
	  t_hw_last = tcur;
	}
      }

      if(moni_config_interval > 0) {
        if(t_cf_last > tcur /* overflow case (should be RARE)  */
           || (tcur-t_cf_last) > moni_config_interval) {
	  moniInsertConfigStateMessage(tcur);
	  t_cf_last = tcur;
	}
      }


      /* Get a message, if available. */
#     if defined (CYGWIN) || defined (LINUX)
         status = recvMsg();
#     else
         status = recvMsg_arm_nonblock();
#     endif

      if (status < 0) {
	/* JJ: We used to bail out of domapp here, but now we use the
	   opportunity to do something else, like catch up on some
	   sleep.  Or we could, if there was some sort of CPU wait
	   state and timer interrupt, but there isn't, yet, so we keep
	   it hyperactive: */
	continue;
      }
      
      msgHandler(messageBuffer);
      
      if ((status = sendMsg()) < 0) {
	/* error reported from send */
	return COM_ERROR;
      }
    }
    
    /* Never get here */
    return 0;
}

#if defined (CYGWIN) || defined (LINUX)
void cleanup_exit(int sig) {
  /* Close input/output files/sockets.
     Since this handler is established for more than one kind of signal, 
     it might still get invoked recursively by delivery of some other kind
     of signal.  Use a static variable to keep track of that. */
  if(fatal_error_in_progress) 
    raise(sig);
  fatal_error_in_progress = 1;

  fprintf(log,"domapp: Caught signal--closing log and socket or device file.\n");
  fclose(log);
  close(dom_input_file);
  close(dom_output_file);

  /* Now reraise the signal.  We reactivate the signal's
     default handling, which is to terminate the process.
     We could just call exit or abort,
     but reraising the signal sets the return status
     from the process correctly. */
  signal(sig, SIG_DFL);
  raise(sig);
  //exit(0);
}
#endif

 
int recvMsg_arm_nonblock(void) {
  /* JEJ:
     Non-blocking version of recvMsg.  
     Currently not very efficient 
        (allocate/deallocate message buffer ea. time)
     @return negative value if error or no data.
     @return positive value otherwise.
  */
  UBYTE *dataBuffer_p;
  int   sts;
  //static int itmp=0;

  /* Check for availability of data -- go back if nothing. */
  if(! halIsInputData()) return COM_EAGAIN;

  messageBuffer = messageBuffers_allocate();
  if (messageBuffer == NULL) {
    return ERROR;
  }
  dataBuffer_p = Message_getData(messageBuffer);
  sts = readmsg(dom_input_file, messageBuffer, dataBuffer_p);
  if(sts < 0) {
    Message_setData(messageBuffer, dataBuffer_p, MAXDATA_VALUE);
    messageBuffers_release(messageBuffer);
    return sts;
  } 
  return 1;
}

/** recvMsg(): 
 * receive a complete message, place it into a message buffer
 * and queue it up to the msgHandler for processing.
 *
 * @return 1 if message properly handled, other wise error from
 *	communications routine.
 *
 * @see sendMsg() complimentary function.
*/
int recvMsg(void) {
  /** recv sts */
  int sts;
  /* pointer to message data buffer */
  UBYTE *dataBuffer_p;
  
  //printf("in rcvMsg: ready to allocate buffer\r\n");
  /* receive the message header */
  messageBuffer = messageBuffers_allocate();

  //printf("in rcvMsg: buffer allocated %x\r\n", messageBuffer);
  
  if (messageBuffer == NULL) {
    return ERROR;
  }
  
  /* save data buffer address for  this message buffer */
  dataBuffer_p = Message_getData(messageBuffer);

  //printf("in rcvMsg: dataBuffer: %x\r\n", dataBuffer_p);
  //fprintf(log, "domapp: about to gmsr_recvMessageGeneric.\n");
  
  /* Read in the whole message, now using generic functions */
#if defined (CYGWIN) || defined (LINUX)
  sts = gmsr_recvMessageGeneric(dom_input_file, messageBuffer, dataBuffer_p, 0);
#else
  sts = readmsg(dom_input_file, messageBuffer, dataBuffer_p);
#endif
  if(sts < 0) {
    /* patch up the message buffer and release it */
    Message_setData(messageBuffer, dataBuffer_p, MAXDATA_VALUE);
    messageBuffers_release(messageBuffer);
    //fprintf(log, "domapp: got sts %d in recv\n",sts);
    return sts == 0 ? COM_EOF : sts;
  } 
  
  return 1;
}

/** sendMsg(): 
 * Check msgHandler message queue for outgoing messages. If
 * one is present, assemble message and send it.
 * Should work for LINUX, CYGWIN, or DOM Main Board Hardware (JEJ).
 *
 * @return 0  no errors, otherwise return value of low level 
 * 	communications routine.
 *
 * @see recvMsg()
*/
int sendMsg() {
  int sts;

  /* Send out the message using generic function -- works with
     CYGWIN, LINUX, or DOM MB hardware too!  (Just calls write() twice) */
  sts = gmsr_sendMessageGeneric(dom_output_file, messageBuffer);

  /* always delete the message buffer-even if there was a com error */
  messageBuffers_release(messageBuffer);

  if(sts < 0) {
    return sts;
  }
  return 0;
}
 
 
/** readmsg(): 
    Jacobsen 2003.05.05 - Routine for MB to recv data using newlib's read().
    Simply reads the header and then the appropriate number of payload bytes.
    Assumes space has been allocated for recvBuffer_p and recvData. 
    Assumes blocking semantics for read. */

int readmsg(int fd, MESSAGE_STRUCT *recvBuffer_p, char *recvData) {
  
  int i;
  int payloadLength;
  char *buffer_p = (char *)recvBuffer_p;

  //printf("in readmsg:\r\n");
  for(i =0; i<MSG_HDR_LEN; i++) {
    while(read(fd, (char *) buffer_p, 1) != 1) {}; /* simulate blocking */
    //read(fd, (char *) buffer_p, 1);
    buffer_p++;
  }

  //printf("in readmsg: sts: %d\r\n",sts);

  payloadLength = Message_dataLen(recvBuffer_p);
  //printf("in readmsg: payloadLength: %d\r\n", payloadLength);
  if(payloadLength > MAXDATA_VALUE) return COM_ERROR;

  if(payloadLength == 0) return payloadLength;

  for(i = 0; i < payloadLength; i++) {
    while(read(fd, recvData, 1) != 1) {}; /* simulate blocking */
    //read(fd, recvData, 1);
    recvData++;
  }

  return payloadLength; /* Keep same default return value (# of payload bytes) 
			   as gmsr_recvMessageGeneric */

}
