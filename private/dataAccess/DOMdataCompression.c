
#include <string.h>

// DOMdataCompression.c //

// DOM-related includes
#include "domapp_common/DOMtypes.h"
#include "dataAccess/DOMdataCompression.h"

// local functions, data
int bufferLen;
int bufferLenRemaining;

void initDOMdataCompression(int bufLen) {
    bufferLen = bufLen;
}

UBYTE *startDOMdataBuffer(UBYTE *bufferLoc) {
    bufferLenRemaining = bufferLen;
    return bufferLoc;
}

UBYTE *addAndCompressEvent(UBYTE *incomingData, UBYTE *bufferLoc) {
    int incomingDataLen;

    // assume simulated, well formated event as input with byte
    // length in the first short word
    incomingDataLen = *incomingData*256 + *(incomingData+1);
/*
    printf("addAndCompress: in: %x, loc: %x, len: %d\n",
	incomingData, bufferLoc, incomingDataLen);
*/

    // verify that we have enough room left
    if(incomingDataLen > bufferLenRemaining) {
	// just return a null
	//printf("addAndCompressEvent: no room\r\n");
	return (UBYTE *)0;
    }
    else {
	memcpy(bufferLoc, incomingData, incomingDataLen);
	bufferLenRemaining-=incomingDataLen;

/*
	printf("addAndCompressEvent: return addr: %x\r\n",
		(bufferLoc+incomingDataLen));
*/
	return (UBYTE *)(bufferLoc + incomingDataLen);
    }
}
