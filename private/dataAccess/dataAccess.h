/* dataAccess.h */
#ifndef _DATA_ACCESS_
#define _DATA_ACCESS_

/* Header file for defines, structs, etc. for */
/* the Data Access Service. */

/* Data Access version info. */
#define DAC_MAJOR_VERSION 2
#define DAC_MINOR_VERSION 0

/* maximum length of Data Access last */
/* error string. */
#define DAC_ERROR_STR_LEN 80

/* Data Access error strings */
/* for DAC_No_Errors */
#define DAC_ERS_NO_ERRORS "DAC: No errors."
/* for DAC_Bad_Msg_Subtype */
#define DAC_ERS_BAD_MSG_SUBTYPE "DAC: Bad msg subtype."
/* for DAC_Data_Overrun */
#define DAC_DATA_OVERRUN "DAC: Readout buffer data overrun."

/* define ALLOCATE_READOUT_BUFFER if you want dataAccess
   to allocate memory to be used for FPGA simulation.
   Otherwise define READOUT_BASE_ADDR to the real address
 */
#define ALLOCATE_READOUT_BUFFER

/* define the size of the memory buffer shared between the
   FPGA data engine and domapp. Format is a bit mask that 
   matches the field size of the memory buffer length.  MUST
   be a power of 2!  I.e. mask of 0x1ff means that the FPGA
   will rotate through a set of 512 buffers that are, as defined
   by the FPGA code, to be some fixed length.  At the moment, this
   length is set to 2048
 */
#define FPGA_MEMORY_MASK 0x1ff

/* data access entry point */
void dataAccessInit(void);
void dataAccess(MESSAGE_STRUCT *M);

#endif
