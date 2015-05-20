/* logging.c
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <cyassl/ctaocrypt/settings.h>

/* submitted by eof */

#include <cyassl/ctaocrypt/logging.h>
#include <cyassl/ctaocrypt/error.h>


#ifdef __cplusplus
    extern "C" {
#endif
    CYASSL_API int  CyaSSL_Debugging_ON(void);
    CYASSL_API void CyaSSL_Debugging_OFF(void);
#ifdef __cplusplus
    } 
#endif


#ifdef DEBUG_CYASSL

/* Set these to default values initially. */
static CyaSSL_Logging_cb log_function = 0;
static int loggingEnabled = 0;

#endif /* DEBUG_CYASSL */


int CyaSSL_SetLoggingCb(CyaSSL_Logging_cb f)
{
#ifdef DEBUG_CYASSL
    int res = 0;

    if (f)
        log_function = f;
    else
        res = BAD_FUNC_ARG;

    return res;
#else
    (void)f;
    return NOT_COMPILED_IN;
#endif
}


int CyaSSL_Debugging_ON(void)
{
#ifdef DEBUG_CYASSL
    loggingEnabled = 1;
    return 0;
#else
    return NOT_COMPILED_IN;
#endif
}


void CyaSSL_Debugging_OFF(void)
{
#ifdef DEBUG_CYASSL
    loggingEnabled = 0;
#endif
}


#ifdef DEBUG_CYASSL

#ifdef FREESCALE_MQX
    #include <fio.h>
#else
    #include <stdio.h>   /* for default printf stuff */
#endif

#ifdef THREADX
    int dc_log_printf(char*, ...);
#endif

static void cyassl_log(const int logLevel, const char *const logMessage)
{
       wmprintf("%s\n\r", logMessage);
       return;
#if 0
    if (log_function)
        log_function(logLevel, logMessage);
    else {
        if (loggingEnabled) {
#ifdef THREADX
            dc_log_printf("%s\n", logMessage);
#elif defined(MICRIUM)
        #if (NET_SECURE_MGR_CFG_EN == DEF_ENABLED)
            NetSecure_TraceOut((CPU_CHAR *)logMessage);
        #endif
#elif defined(CYASSL_MDK_ARM)
            fflush(stdout) ;
            printf("%s\n", logMessage);
            fflush(stdout) ;
#else
            fprintf(stderr, "%s\n", logMessage);
#endif
        }
    }
#endif /* 0 */
}


void CYASSL_MSG(const char* msg)
{
    if (loggingEnabled)
        cyassl_log(INFO_LOG , msg);
}


void CYASSL_ENTER(const char* msg)
{
    if (loggingEnabled) {
        char buffer[80];
        sprintf(buffer, "CyaSSL Entering %s", msg);
        cyassl_log(ENTER_LOG , buffer);
    }
}


void CYASSL_LEAVE(const char* msg, int ret)
{
    if (loggingEnabled) {
        char buffer[80];
        sprintf(buffer, "CyaSSL Leaving %s, return %d", msg, ret);
        cyassl_log(LEAVE_LOG , buffer);
    }
}


void CYASSL_ERROR(int error)
{
    if (loggingEnabled) {
        char buffer[80];
        sprintf(buffer, "CyaSSL error occured, error = %d", error);
        cyassl_log(ERROR_LOG , buffer);
    }
}

#endif  /* DEBUG_CYASSL */ 
