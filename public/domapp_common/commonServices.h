/* commonServices.h */

#ifndef _COMMONSERVICES_
#define _COMMONSERVICES_

/* Defines, etc. that are common for DOM Services
   like Experimental Control, Data Access, etc.
   If a data struct or definition is common to all
   services, it should be defined here so that
   other programs, like the message handler, can
   understand and access the appropriate structs. */

#define MAX_ERROR_STR_LEN 80
#define NULL_ERROR_STR ""

typedef struct {	
	UBYTE state;
	UBYTE lastErrorID;
	UBYTE lastErrorSeverity;
	UBYTE majorVersion;
	UBYTE minorVersion;
	UBYTE spare;
	char lastErrorStr[MAX_ERROR_STR_LEN];
	ULONG msgReceived;
	ULONG msgRefused;
	ULONG msgProcessingErr;
} COMMON_SERVICE_INFO;

void formatLong(ULONG value, UBYTE * buf);
void formatShort(USHORT value, UBYTE *buf);
void formatTime(unsigned long long time, UBYTE *buf);
ULONG unformatLong(UBYTE *buf);
USHORT unformatShort(UBYTE *buf);

#endif
