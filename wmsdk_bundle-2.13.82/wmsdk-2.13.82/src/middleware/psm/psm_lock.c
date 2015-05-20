/* 
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** psm_lock.c: Locking for psm
 */
#include <psm.h>

int psm_get_mutex()
{
	/* Wait forever for the mutex */
	return os_mutex_get(&psm_mutex, OS_WAIT_FOREVER);
}

int psm_put_mutex()
{
	return os_mutex_put(&psm_mutex);
}
