// See LICENSE for license details.

#ifndef _PK_USERMEM_H
#define _PK_USERMEM_H

#include <stdbool.h>
#include <stddef.h>

void memset_user(void* dst, int ch, size_t n);
void memcpy_to_user(void* dst, const void* src, size_t n);
void memcpy_from_user(void* dst, const void* src, size_t n);
bool strcpy_from_user(char* dst, const char* src, size_t n);

#endif
