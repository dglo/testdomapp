// DOMdataCompression.h //

// DOM-related includes
#include "domapp_common/DOMtypes.h"

void initDOMdataCompression(int bufLen);

UBYTE *startDOMdataBuffer(UBYTE *bufferLoc);

UBYTE *addAndCompressEvent(UBYTE *incomingData, UBYTE *bufferLoc);
