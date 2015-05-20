/*
 *  Copyright 2008-2013,2011 Marvell International Ltd.
 *  All Rights Reserved.
 */
/* httpd_ssi.c: This file contains routines for the processing of SSI directives
 * within shtml files.
 */
/*
 * Copyright (c) 2001-2006, Adam Dunkels.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file is part of the uIP TCP/IP stack.
 *
 * Adapted by C Michael Sundius, Solico Group LLC for Cozybit Inc
 *
 * Description
 *  SSI processor for simple web server
 *  syntax of a script is
 *   %! <func_name> [optional_arg]
 *   %! : <include html filename>
 *   <standard html>
 *
 */


#include <string.h>

#include <wmstdio.h>
#include <httpd.h>

#include "httpd_priv.h"

static int ssi_ready;
static struct httpd_ssi_call *calls[MAX_REGISTERED_SSIS];

/* Register an SSI handler */
int httpd_register_ssi(struct httpd_ssi_call *ssi_call)
{
	int i;

	if (!ssi_ready)
		return -WM_FAIL;

	/* Verify that the ssi is not already registered */
	for (i = 0; i < MAX_REGISTERED_SSIS; i++) {
		if (!calls[i]) {
			continue;
		}
		if (strncmp(calls[i]->name, ssi_call->name,
			    strlen(calls[i]->name)) == 0) {
			return -WM_FAIL;
		}
	}

	/* Find an unoccupied slot in the table */
	for (i = 0; i < MAX_REGISTERED_SSIS && calls[i]; i++)
			;

	/* Check that we're not overrunning the table */
	if (i == MAX_REGISTERED_SSIS)
		return -WM_E_HTTPD_SSI_MAX;

	calls[i] = ssi_call;
	httpd_d("Register ssi %s at %d", ssi_call->name, i);

	return WM_SUCCESS;
}

/* Unregister an SSI handler */
void httpd_unregister_ssi(struct httpd_ssi_call *ssi_call)
{
	int i;

	if (!ssi_ready)
		return;

	for (i = 0; i < MAX_REGISTERED_SSIS; i++) {
		if (!calls[i]) {
			continue;
		}
		if (strncmp(calls[i]->name, ssi_call->name,
			    strlen(calls[i]->name)) == 0) {
			calls[i] = NULL;
			httpd_d("Unregister ssi %s at %d", ssi_call->name, i);
			return;
		}
	}

	return;
}

/* The null function is returned if no match is found */
int nullfunction(const httpd_request_t *req, const char *args, int conn,
		 char *scratch)
{
	return WM_SUCCESS;
}

/* Find a matching SSI function from the table and return it */
httpd_ssifunction httpd_ssi(char *name)
{
	struct httpd_ssi_call *f;
	int i;

	if (!ssi_ready)
		return nullfunction;

	/* Find the matching name in the table, return the function. */
	for (i = 0; i < MAX_REGISTERED_SSIS; i++) {
		f = calls[i];
		if (f && strncmp(f->name, name, strlen(f->name)) == 0) {
			httpd_d("Found function: %s", (f)->name);
			return f->function;
		}
	}

	httpd_d("Did not find function %s returning nullfunc", name);

	return nullfunction;
}

/** Initialize the SSI handling data structures */
int httpd_ssi_init(void)
{
	memset(calls, 0, sizeof(struct httpd_ssi_call *) * MAX_REGISTERED_SSIS);
	ssi_ready = 1;
	return WM_SUCCESS;
}
