/*
 * @mainpage domapp socket based simulation program
 * @author McP, with mods by Jacobsen
 * Based on original code by mcp.
 *
 * $Date: 2003-05-14 01:28:35 $
 *
 * @section ration Rationale
 *
 * This code provides low level initialization and communications
 * code that allow execution of the DOM application within the 
 * either the Linux or Cygwin environment.  This particular verstion
 * uses stdin and stdout to perform all message passing communications.
 * It is intended to be run by the simboot program and, therefore, will
 * have these file descriptors mapped to a pre-established IP socket.
 *
 * @section details Implementation details
 *
 * This implementation uses the standard ipcmsg messaging package to
 * simulate the use of shared message queues between the various threads
 * of the DOM application.  This facility will be replaced by a similar
 * mechanism native to the DOM MB operating system (Nucleus).
 *
 * @subsection lang Language
 *
 * For DOM application compatibility, this code is written in C. 
 *
 */

/**
 * @file domappFile.c
 * 
 * This file contains low level initialization routines used to
 * settup and manage the environment used in simulating execution of
 * the DOM application on other platforms.
 *
 * $Revision: 1.4 $
 * $Author: mcp $
 * Based on original code by Chuck McParland
 * $Date: 2003-05-14 01:28:35 $
*/

#if defined (CYGWIN) || defined (LINUX)
/** system include files */
#include <sys/socket.h>
#include <stdio.h>
#include <time.h>   /* for time() */
#include <signal.h> /* For signal() */

/* JEJ Included these files to pick up O_RDWR flags for open */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
/* JEJ */
#include <errno.h> /** For errno, on open */
#else
#include <unistd.h> /* Needed for read/write */
#endif

/** project include files */
#if defined (CYGWIN) || defined (LINUX)
#include "hal/DOM_MB_hal_simul.h"
#else 
#endif


/* DOMtypes.h needed to build under linux */
#include "domapp_common/DOMtypes.h"

#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "message/genericMsgSendRecv.h"	
#include "expControl/expControl.h"
//#include "dataAccess/dataAccess.h"
#include "slowControl/domSControl.h"

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
#else
int main(void) {
#endif

    /** storage */
    char *errorMsg;
#define DA_MAX_LOGNUM 6
    char id6dig[DA_MAX_LOGNUM+1];
    char logname[] = "dom_log_XXXXXX.txt"; /* Replace Xs w/ low bits of DOM ID */
    
    int i;
    int j;
    int k;
    int dataLen;
    int send;
    int receive;
    int status;
    int port;
    int nready;
    int dom_simulation_file; 
    UBYTE b;

#if defined (CYGWIN) || (LINUX)
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
    //log = stderr; /* Do this if you want messages to stderr instead
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

    /* init messageBuffers */
    messageBuffers_init();

    /* manually init all the domapp components */
    msgHandlerInit();
    domSControlInit();
    expControlInit();

    //printf("domapp: Readdy to go\r\n");
    
    //write(dom_output_file, "domapp starting up.", strlen("domapp starting up."));
    //for (;;) {
	//read(dom_input_file, &b, 1);
	//printf ("received: %x, %c\r\n", b, b);
    //}
    for (;;) {

        /* Flush log file out, each loop */
        //fprintf(log, "About to read...\n");
        //fflush(log);
      
        //printf("domapp: Readdy to recvMsg\r\n");
     
        status = recvMsg();

        if (status <= 0) {
        /* error reported from receive, or EOF */
	    //fprintf(log, "domapp: Error or EOF from recvMsg() (%d).\n",
		    //status);
  	    //fflush(log);
#if defined (CYGWIN) || (LINUX)
	    return COM_ERROR;
#else
	    //printf(" recv4Msg returnned with error.\r\n");
	    return;
#endif
	} else {
	    //fprintf(log, "domapp: Got a message.\n");
	}
	
	//handle the request
    	//printf("received message.");
	msgHandler(messageBuffer);
      
	if ((status = sendMsg()) < 0) {
	  //fprintf(log,"domapp: problem on send: status %d.\n",status);
	  /* error reported from send */
	  //fflush(log);
#if defined (CYGWIN) || (LINUX)
	  return COM_ERROR;
#else
	  return;
#endif
	}
    }

    /* this is really bad, nothing left to do but bail */
    //errorMsg="domapp: lost socket connection";
    //fprintf(log,"%s\n\r",errorMsg);
    //fclose(log);

#if defined (CYGWIN) || (LINUX)
    return 0;
#else
    return;
#endif

}

#if defined (CYGWIN) || (LINUX)
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
  /** length of entire message to be received */
  long msgLength;
  /** number of bytes to be received for a given msg part */
  int targetLength;
  /** recv sts */
  int sts;
  /* pointer to message data buffer */
  UBYTE *dataBuffer_p;
  /* misc buffer pointer */
  UBYTE *buffer_p;
  
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
#if defined (CYGWIN) || (LINUX)
  sts = gmsr_recvMessageGeneric(dom_input_file, messageBuffer, dataBuffer_p, 0);
#else
  sts = readmsg(dom_input_file, messageBuffer, dataBuffer_p);
#endif
  if(sts <= 0) {
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
  long sendLen;
  
  //fprintf(log, "domapp: About to Message_receive_nonblock().\n");
  //fprintf(log, "domapp: have a message to send (messageBuffer == %p)\n",
  ////messageBuffer);

  /* Send out the message using generic function -- works with
     CYGWIN, LINUX, or DOM MB hardware too!  (Just calls write() twice) */
  sts = gmsr_sendMessageGeneric(dom_output_file, messageBuffer);

  /* always delete the message buffer-even if there was a com error */
  messageBuffers_release(messageBuffer);
  
  //fprintf(log, "domapp: Send status was %d.\n",sts);
  
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
    read(fd, (char *) buffer_p, 1);
    buffer_p++;
  }

  //printf("in readmsg: sts: %d\r\n",sts);

  payloadLength = Message_dataLen(recvBuffer_p);
  //printf("in readmsg: payloadLength: %d\r\n", payloadLength);
  if(payloadLength > MAXDATA_VALUE || payloadLength <= 0) return COM_ERROR;

  for(i = 0; i < payloadLength; i++) {
    read(fd, recvData, 1);
    recvData++;
  }

  return payloadLength; /* Keep same default return value (# of payload bytes) 
			   as gmsr_recvMessageGeneric */

}
