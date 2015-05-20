/*! \file wmerrno.h
 *  \brief Error Management
 *
 *  Copyright 2008-2013-12, Marvell International Ltd.
 *  All Rights Reserved.
 *
 */
#ifndef WM_ERRNO_H
#define WM_ERRNO_H

/* Get the module index number from error code (4th byte from LSB)*/
#define get_module_base(code) ((code&0xF000)>>12)

/* Get notifier message type i.e Error, Warning or Info (3rd byte from LSB)*/
#define get_notifier_msg_type(code) ((code&0x0F00)>>8)

/* Get module notifier code (2nd and 1st byte from LSB)*/
#define get_code(code) (code&0xFF)

#define MOD_ERROR_START(x)  (x << 12 | 0)
#define MOD_WARN_START(x)   (x << 12 | 1)
#define MOD_INFO_START(x)   (x << 12 | 2)

/* Create Module index */
#define MOD_GENERIC    0
/** Unused */
#define MOD_UNUSED_3   2
/** HTTPD module index */
#define MOD_HTTPD      3
/** Application framework module index */
#define MOD_AF         4
/** FTFS module index */
#define MOD_FTFS       5
/** RFGET module index */
#define MOD_RFGET      6
/** JSON module index  */
#define MOD_JSON       7
/** TELNETD module index */
#define MOD_TELNETD    8
/** SIMPLE MDNS module index */
#define MOD_SMDNS      9
/** EXML module index */
#define MOD_EXML       10
/** DHCPD module index */
#define MOD_DHCPD      11
/** MDNS module index */
#define MOD_MDNS       12
/** SYSINFO module index */
#define MOD_SYSINFO   13
/** Unused module index */
#define MOD_UNUSED_1     14
/** CRYPTO module index */
#define MOD_CRYPTO     15
/** HTTP-CLIENT module index */
#define MOD_HTTPC      16
/** PROVISIONING module index */
#define MOD_PROV       17
/** SPI module index */
#define MOD_SPI        18
/** PSM module index */
#define MOD_PSM        19
/** TTCP module index */
#define MOD_TTCP       20
/** DIAGNOSTICS module index */
#define MOD_DIAG       21
/** Unused module index */
#define MOD_UNUSED_2    22
/** WPS module index */
#define MOD_WPS        23
/** WLAN module index */
#define MOD_WLAN        24
/** USB module index */
#define MOD_USB        25
/** WIFI driver module index */
#define MOD_WIFI        26


/* Globally unique success code */
#define WM_SUCCESS 0

enum wm_errno {
	/* First Generic Error codes */
	WM_GEN_E_BASE = MOD_ERROR_START(MOD_GENERIC),
	WM_FAIL,
	WM_E_PERM,   /* Operation not permitted */
	WM_E_NOENT,  /* No such file or directory */
	WM_E_SRCH,   /* No such process */
	WM_E_INTR,   /* Interrupted system call */
	WM_E_IO,     /* I/O error */
	WM_E_NXIO,   /* No such device or address */
	WM_E_2BIG,   /* Argument list too long */
	WM_E_NOEXEC, /* Exec format error */
	WM_E_BADF,   /* Bad file number */
	WM_E_CHILD,  /* No child processes */
	WM_E_AGAIN,  /* Try again */
	WM_E_NOMEM,  /* Out of memory */
	WM_E_ACCES,  /* Permission denied */
	WM_E_FAULT,  /* Bad address */
	WM_E_NOTBLK, /* Block device required */
	WM_E_BUSY,   /* Device or resource busy */
	WM_E_EXIST,  /* File exists */
	WM_E_XDEV,   /* Cross-device link */
	WM_E_NODEV,  /* No such device */
	WM_E_NOTDIR, /* Not a directory */
	WM_E_ISDIR,  /* Is a directory */
	WM_E_INVAL,  /* Invalid argument */
	WM_E_NFILE,  /* File table overflow */
	WM_E_MFILE,  /* Too many open files */
	WM_E_NOTTY,  /* Not a typewriter */
	WM_E_TXTBSY, /* Text file busy */
	WM_E_FBIG,   /* File too large */
	WM_E_NOSPC,  /* No space left on device */
	WM_E_SPIPE,  /* Illegal seek */
	WM_E_ROFS,   /* Read-only file system */
	WM_E_MLINK,  /* Too many links */
	WM_E_PIPE,   /* Broken pipe */
	WM_E_DOM,    /* Math argument out of domain of func */
	WM_E_RANGE,  /* Math result not representable */
	WM_E_CRC,    /* Error in CRC check */
};

#endif /* ! WM_ERRNO_H */
