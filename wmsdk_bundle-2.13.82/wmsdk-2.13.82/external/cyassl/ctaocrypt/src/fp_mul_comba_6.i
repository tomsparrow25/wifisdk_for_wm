/* fp_mul_comba_6.i
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */



#ifdef TFM_MUL6
void fp_mul_comba6(fp_int *A, fp_int *B, fp_int *C)
{
   fp_digit c0, c1, c2, at[12];

   memcpy(at, A->dp, 6 * sizeof(fp_digit));
   memcpy(at+6, B->dp, 6 * sizeof(fp_digit));
   COMBA_START;

   COMBA_CLEAR;
   /* 0 */
   MULADD(at[0], at[6]); 
   COMBA_STORE(C->dp[0]);
   /* 1 */
   COMBA_FORWARD;
   MULADD(at[0], at[7]);    MULADD(at[1], at[6]); 
   COMBA_STORE(C->dp[1]);
   /* 2 */
   COMBA_FORWARD;
   MULADD(at[0], at[8]);    MULADD(at[1], at[7]);    MULADD(at[2], at[6]); 
   COMBA_STORE(C->dp[2]);
   /* 3 */
   COMBA_FORWARD;
   MULADD(at[0], at[9]);    MULADD(at[1], at[8]);    MULADD(at[2], at[7]);    MULADD(at[3], at[6]); 
   COMBA_STORE(C->dp[3]);
   /* 4 */
   COMBA_FORWARD;
   MULADD(at[0], at[10]);    MULADD(at[1], at[9]);    MULADD(at[2], at[8]);    MULADD(at[3], at[7]);    MULADD(at[4], at[6]); 
   COMBA_STORE(C->dp[4]);
   /* 5 */
   COMBA_FORWARD;
   MULADD(at[0], at[11]);    MULADD(at[1], at[10]);    MULADD(at[2], at[9]);    MULADD(at[3], at[8]);    MULADD(at[4], at[7]);    MULADD(at[5], at[6]); 
   COMBA_STORE(C->dp[5]);
   /* 6 */
   COMBA_FORWARD;
   MULADD(at[1], at[11]);    MULADD(at[2], at[10]);    MULADD(at[3], at[9]);    MULADD(at[4], at[8]);    MULADD(at[5], at[7]); 
   COMBA_STORE(C->dp[6]);
   /* 7 */
   COMBA_FORWARD;
   MULADD(at[2], at[11]);    MULADD(at[3], at[10]);    MULADD(at[4], at[9]);    MULADD(at[5], at[8]); 
   COMBA_STORE(C->dp[7]);
   /* 8 */
   COMBA_FORWARD;
   MULADD(at[3], at[11]);    MULADD(at[4], at[10]);    MULADD(at[5], at[9]); 
   COMBA_STORE(C->dp[8]);
   /* 9 */
   COMBA_FORWARD;
   MULADD(at[4], at[11]);    MULADD(at[5], at[10]); 
   COMBA_STORE(C->dp[9]);
   /* 10 */
   COMBA_FORWARD;
   MULADD(at[5], at[11]); 
   COMBA_STORE(C->dp[10]);
   COMBA_STORE2(C->dp[11]);
   C->used = 12;
   C->sign = A->sign ^ B->sign;
   fp_clamp(C);
   COMBA_FINI;
}
#endif
