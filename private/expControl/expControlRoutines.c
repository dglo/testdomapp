/* ExpControlRoutines.c */

/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Last Modification:
*/

// DOM-related includes
#include "domapp_common/DOMtypes.h"
#include "domapp_common/DOMstateInfo.h"
#include "message/message.h"
#include "expControl/expControl.h"
#include "expControl/EXPmessageAPIstatus.h"

/* local functions, data */
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;
extern UBYTE DOM_status;
extern UBYTE DOM_cmdSource;
extern ULONG DOM_constraints;
extern char *DOM_errorString;

// local functions, data


BOOLEAN forceRunReset() {
    DOM_state=DOM_IDLE;
    return TRUE;
}


