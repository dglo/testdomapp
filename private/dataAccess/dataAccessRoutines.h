
/*dataAccessRoutines.h */
#ifndef _DATA_ACCESS_ROUTINES_
#define _DATA_ACCESS_ROUTINES_

BOOLEAN beginRun(void); 

BOOLEAN endRun(void);

BOOLEAN forceRunReset(void);

void initFillMsgWithData(void);

BOOLEAN checkDataAvailable(void);

int fillMsgWithData(UBYTE *msgBuffer);

#endif
