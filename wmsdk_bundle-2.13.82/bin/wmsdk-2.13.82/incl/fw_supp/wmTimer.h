
#include <wm_os.h>

#ifndef _WMTIMER_H_
#define _WMTIMER_H_

bool wmTimerStart(const char *name, void (*callback)(os_timer_arg_t),
                               keyMgmtInfoSta_t *callbackData,
                               UINT32 expiration_in_ms,
                               os_timer_t *pTimerId);

bool wmTimerStop(os_timer_t pTimerId);
#endif
