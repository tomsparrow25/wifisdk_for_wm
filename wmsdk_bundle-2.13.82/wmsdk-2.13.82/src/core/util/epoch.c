
/*
 *  Copyright 2008-2013, Marvell International Ltd.
 *  All Rights Reserved.
 */

static unsigned int epoch;

unsigned int sys_get_epoch()
{
	return epoch;
}


void sys_set_epoch(unsigned int epoch_val)
{
	epoch = epoch_val;
}
