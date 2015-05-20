/* logging.h
 *
 * Copyright (C) 2006-2013 wolfSSL Inc.  All rights reserved.
 *
 * This file is part of CyaSSL.
 *
 * Contact licensing@yassl.com with any questions or comments.
 *
 * http://www.yassl.com
 */


/* submitted by eof */


#ifndef CYASSL_LOGGING_H
#define CYASSL_LOGGING_H


#ifdef __cplusplus
    extern "C" {
#endif


enum  CYA_Log_Levels {
    ERROR_LOG = 0,
    INFO_LOG,
    ENTER_LOG,
    LEAVE_LOG,
    OTHER_LOG
};

typedef void (*CyaSSL_Logging_cb)(const int logLevel,
                                  const char *const logMessage);

CYASSL_API int CyaSSL_SetLoggingCb(CyaSSL_Logging_cb log_function);


#ifdef DEBUG_CYASSL

    void CYASSL_ENTER(const char* msg);
    void CYASSL_LEAVE(const char* msg, int ret);

    void CYASSL_ERROR(int);
    void CYASSL_MSG(const char* msg);

/* Added for WMSDK */
#define TRACE wmprintf
#define TRACE_LINE wmprintf("%s: %d\n\r", __FUNCTION__, __LINE__)

#else /* DEBUG_CYASSL   */

    #define CYASSL_ENTER(m)
    #define CYASSL_LEAVE(m, r)

    #define CYASSL_ERROR(e) 
    #define CYASSL_MSG(m)

/* Added for WMSDK */
#define TRACE(...)
#define TRACE_LINE

#endif /* DEBUG_CYASSL  */

#ifdef __cplusplus
}
#endif

#endif /* CYASSL_MEMORY_H */
