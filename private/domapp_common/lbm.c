/* lbm.c: routines for getting in and out of lookback memory.

   Start with malloc version of LBM

   Jacobsen 8/10/04 jacobsen@npxdesigns.com
   $Id: lbm.c,v 1.1 2004-08-10 15:08:05 jacobsen Exp $

*/

int is_initialized = 0;
unsigned char * nonFPGA_lbm = NULL;

