/* moniDataAccess.h
 * Header file for store/fetch operations on monitoring data
 * Part of dataAccess thread
 * John Jacobsen, JJ IT Svcs, for LBNL
 * May, 2003
 * $Id: moniDataAccess.h,v 1.1 2003-05-29 00:48:41 mcp Exp $
 */

#ifndef _MONI_DATA_ACCESS_
#define _MONI_DATA_ACCESS_

#define MAXMONI_DATA 512-4 /* makes for 512-byte monitoring records */
#define MONI_TIME_LEN 6

#define MONI_TYPE_HW_SNAPSHOT 0xe1
#define MONI_TYPE_EVENT_MSG   0xe2
#define MONI_TYPE_FLASHER     0xe3
#define MONI_TYPE_LOG_MSG     0xe4

struct moniRec {
  /* QUESTION: should timestamp info appear here? */

  USHORT dataLen;    /* Number of bytes in data portion */

  union {   /* 2-byte Fiducial indicating event type */
    struct FID {
      UBYTE spare;
      UBYTE moniEvtType; /* One of MONI_TYPE... */
    } fstruct;
    USHORT fshort;
  } fiducial;

  UBYTE time[MONI_TIME_LEN];   /* time tag */
  UBYTE data[MAXMONI_DATA];  /* Payload data */
};


#define MONI_REC_SIZE (MAXMONI_DATA + sizeof(USHORT)*2)

#define MONI_CIRCBUF_RECS 1024 /* The circular buffer will be 
				  MONI_CIRCBUF_RECS * MONI_REC_SIZE bytes */

#define MONI_MASK (MONI_CIRCBUF_RECS-1)


/* Return type for consumer function */
typedef enum {
  MONI_OK       = 1,
  MONI_NODATA   = 2,
  MONI_WRAPPED  = 3,
  MONI_OVERFLOW = 4,
  MONI_NOTINITIALIZED = 5
} MONI_STATUS;


/* Prototypes */

void moniInit(
	      UBYTE *bufBaseAddr, 
	      int mask);                      /* Initializes circular buffer 
						 pointers */

void moniInsertRec(struct moniRec *m);        /* Producer function */

MONI_STATUS moniFetchRec(struct moniRec *m);  /* Consumer function 
						 (data access thread) */


/* The following functions use moniInsertRec to insert data */

void moniInsertHardwareSnapshotRec(long long *time);     /* Type 1 - timestamped rec. of all
						 DACs, ADCs, etc. hardware info
						 on board */

void moniInsertDiagnosticMessage(char *msg, long long *time, int len);
                                              /* Type 2 - event log tagged with ASCII
						 text string */

void moniInsertFlasherData(long long *time);             /* Type 3 - flasher data (specify argument
						 type later) */


/* The following are utility functions used by the monitoring code per se */

int  moniGetElementLen(void);                 /* Returns length of monitoring data */

#endif
