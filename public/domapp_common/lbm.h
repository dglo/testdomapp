/* lbm.h: routines for getting in and out of lookback memory.

   Start with malloc version of LBM

   Jacobsen 8/10/04 jacobsen@npxdesigns.com
   $Id: lbm.h,v 1.1 2004-08-10 15:06:46 jacobsen Exp $

*/

#define LBM_SIZE 8*1024*1024

extern int is_initialized;
extern unsigned char * nonFPGA_lbm;

int lbm_ok(void) { return is_initialized; }

int lbm_init(void) {
  nonFPGA_lbm = (unsigned char *) malloc(LBM_SIZE);
  if(nonFPGA_lbm) {
    is_initialized = 1;
    return 0;
  } 
  return 1; /* Failure */
}
