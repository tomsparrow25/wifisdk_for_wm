/*
 * vsprintf.c
 */

#include <stdio.h>

int vsprintf(char *buffer, const char *format, va_list ap)
{
	return vsnprintf(buffer, ~(size_t) 0, format, ap);
}
