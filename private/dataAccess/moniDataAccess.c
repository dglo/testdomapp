/* 
 * moniDataAccess.c
 * Routines to store and fetch monitoring data from a circular buffer
 * John Jacobsen, JJ IT Svcs, for LBNL/IceCube
 * May, 2003
 * $Id: moniDataAccess.c,v 1.1 2003-05-29 00:48:41 mcp Exp $
 */


#include "hal/DOM_MB_hal_simul.h"
#include "dataAccess/moniDataAccess.h"

static UBYTE *moniBaseAddr;
static int   moniMask;
static int   moniInitialized = 0;
static ULONG moniReadIndex, moniWriteIndex;
static int   moniCounterWraps;
static int   moniOverruns;

int moniGetElementLen(void) {  return sizeof(struct moniRec);  }


void moniZeroIndices(void) {
  moniReadIndex = moniWriteIndex = 0;
}


void moniIncWriteIndex(void) {
  moniWriteIndex++;
  if((moniWriteIndex & moniMask) == 0) {
    /* clear masked bits if wrap, a la Chuck */
    moniWriteIndex = ~moniMask;
  }
}


ULONG moniGetWriteIndex(void) {
  ULONG tmp;
  /* Mutex it just to be safe */
  tmp = moniWriteIndex;
  return tmp;
}


UBYTE * moniGetWriteBufferAddr(void) {
  UBYTE *temp;

  temp = moniBaseAddr + (MONI_REC_SIZE * (moniWriteIndex & moniMask));

  return temp;
}


UBYTE * moniBufAddr(long idx) {
  return moniBaseAddr + (MONI_REC_SIZE * (idx & moniMask));
}


void moniInit(
	      UBYTE *bufBaseAddr,
	      int mask) {
 
  moniBaseAddr = bufBaseAddr;
  moniMask     = mask;

  moniZeroIndices();

  moniCounterWraps = 0;
  moniOverruns     = 0;

  //printf("Initialized monitoring buffer with address %p and mask 0x%x.\n",
  //moniBaseAddr, mask);
  moniInitialized = 1;
}


void moniInsertRec(struct moniRec *m) {
  UBYTE *buf;

  if(!moniInitialized) return;

  buf = moniGetWriteBufferAddr();

  //printf("Writing record to address %p.\n",
  //buf);

  memcpy(buf, m, MONI_REC_SIZE);

  moniIncWriteIndex();
}


MONI_STATUS moniFetchRec(struct moniRec *m) {
  /* If this function returns MONI_OK, m is filled with 
     a proper record.  Contents pointed to by m are undefined
     otherwise. */

  ULONG writeIndex;

  if(!moniInitialized) return MONI_NOTINITIALIZED;

  writeIndex = moniGetWriteIndex();

  if(moniReadIndex > writeIndex) {
    /* Counters have wrapped.  Should be extremely rare. */
    moniCounterWraps++;
    moniZeroIndices();
    return MONI_WRAPPED;
  }

  /* Have new data? */
  if((writeIndex - moniReadIndex) != 0) {
    if((writeIndex - moniReadIndex) <= moniMask) {

      /* Good to go.  Copy it. */
      memcpy(m, moniBufAddr(moniReadIndex), MONI_REC_SIZE);
      writeIndex = moniGetWriteIndex();

      /* Check to see if buffer has been blown */
      if((moniReadIndex > writeIndex) || 
	 ((writeIndex - moniReadIndex) > moniMask)) {
	moniOverruns++;
	moniReadIndex = writeIndex;
	return MONI_OVERFLOW;
      }

      moniReadIndex++;
      return MONI_OK;

    } else { /* Buffer overrun */
      moniOverruns++;
      moniReadIndex = writeIndex;
      return MONI_OVERFLOW;
    }

  } else {
    return MONI_NODATA;
  }
}


/* Type 2 - event log tagged with ASCII
   text string */
void moniInsertDiagnosticMessage(char *msg, long long *time, int len) {
  struct moniRec mr;
  int i;
  UBYTE *time_ptr = (UBYTE *)time+2; /* point to last 6 bytes;

  /* truncate if too long */
  mr.dataLen = (len > MAXMONI_DATA) ? MAXMONI_DATA : len;
  mr.fiducial.fstruct.moniEvtType = MONI_TYPE_LOG_MSG;
  for(i = 0; i < 6; i++) {
    mr.time[i] = *time_ptr++;
  }
  memcpy(mr.data, msg, len);
  moniInsertRec(&mr);

}



