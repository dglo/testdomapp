/* DACmessageAPIstatus.h */
#ifndef _DATA_MESSAGE_API_STATUS_H_
#define _DATA_MESSAGE_API_STATUS_H_

// This file contains subtype values for the 
// DAQ<=>DOM messaging API used by the Data
// Access service.  
//
// March 30, 1999
// Chuck McParland



// message API subtype values. This field is not
// declared as an enum because each service declares
// values in addion to those in MessageAPIstatus.h.
// Since enums are typedefs, odd compiler problems 
// will ensue.  
//
// Note 255 is the maximum value available for this field. 
#define	DATA_ACC_DATA_AVAIL 10
#define	DATA_ACC_GET_DATA 11

// define service specific error values
#define DSC_Data_Overrun 4

/* These are Data Access specific return message
   formats and values.  In most cases, they are formatted
   as byte values to eliminate any questions of
   "endian-ness".  Note that remote object accesses
   (subtype=REMOTE_OBJECT_REF) will have a format
   determined by that additional protocol.  These
   formats describe "all the rest".
*/

/* Response to: 
	subType: DATA_ACC_DATA_AVAIL
Passed values:
	none
Returned values in data portion of message:
	BOOLEAN dataAvailable
Size of returned values in data portion:
	UBYTE DATA_AVAILABLE */
#define	DAC_ACC_DATA_AVAIL_LEN 1

/* Response to: 
	subType: DATA_GET_DATA
Passed values:
	none
Returned values in data portion of message:
	data buffer, if available
Size of returned values in data portion:
	data length encoded in message header */

#endif


