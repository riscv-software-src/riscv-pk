#include <string.h>
#include <stdint.h>
#include <ctype.h>

void *memchr(const void *s, int c, size_t n)
{
    const unsigned char*  p   = s;
    const unsigned char*  end = p + n;
    for (;;) {
        if (p >= end || p[0] == c) 
		break; 
	p++;
        if (p >= end || p[0] == c)
		break; 
	p++;
        if (p >= end || p[0] == c)
		break;
	p++;
        if (p >= end || p[0] == c)
		break;
	p++;
    }
    if (p >= end)
        return NULL;
    else
        return (void*) p;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char*  p1   = s1;
    const unsigned char*  end1 = p1 + n;
    const unsigned char*  p2   = s2;
    int                   d = 0;
    for (;;) {
        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;
        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;
        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;
        if (d || p1 >= end1) break;
        d = (int)*p1++ - (int)*p2++;
    }
    return d;
}

void *memmove(void *dst, const void *src, size_t n)
{
  const char *p = src;
  char *q = dst;
  /* We can use the optimized memcpy if the destination is below the
   * source (i.e. q < p), or if it is completely over it (i.e. q >= p+n).
   */
  if (__builtin_expect((q < p) || ((size_t)(q - p) >= n), 1)) {
    return memcpy(dst, src, n);
  } else {
    bcopy(src, dst, n);
    return dst;
  }
}

void* memcpy(void* dest, const void* src, size_t len)
{
  const char* s = src;
  char *d = dest;

  if ((((uintptr_t)dest | (uintptr_t)src) & (sizeof(uintptr_t)-1)) == 0) {
    while ((void*)d < (dest + len - (sizeof(uintptr_t)-1))) {
      *(uintptr_t*)d = *(const uintptr_t*)s;
      d += sizeof(uintptr_t);
      s += sizeof(uintptr_t);
    }
  }

  while (d < (char*)(dest + len))
    *d++ = *s++;

  return dest;
}

void* memset(void* dest, int byte, size_t len)
{
  if ((((uintptr_t)dest | len) & (sizeof(uintptr_t)-1)) == 0) {
    uintptr_t word = byte & 0xFF;
    word |= word << 8;
    word |= word << 16;
    word |= word << 16 << 16;

    uintptr_t *d = dest;
    while (d < (uintptr_t*)(dest + len))
      *d++ = word;
  } else {
    char *d = dest;
    while (d < (char*)(dest + len))
      *d++ = byte;
  }
  return dest;
}

size_t strlen(const char *s)
{
  const char *p = s;
  while (*p)
    p++;
  return p - s;
}

size_t  strnlen(const char*  str, size_t  maxlen)
{
    char*  p = memchr(str, 0, maxlen);
    if (p == NULL)
        return maxlen;
    else
        return (p - str);
}

int strcmp(const char* s1, const char* s2)
{
  unsigned char c1, c2;

  do {
    c1 = *s1++;
    c2 = *s2++;
  } while (c1 != 0 && c1 == c2);

  return c1 - c2;
}

char* strcpy(char* dest, const char* src)
{
  char* d = dest;
  while ((*d++ = *src++))
    ;
  return dest;
}

long atol(const char* str)
{
  long res = 0;
  int sign = 0;

  while (*str == ' ')
    str++;

  if (*str == '-' || *str == '+') {
    sign = *str == '-';
    str++;
  }

  while (*str) {
    res *= 10;
    res += *str++ - '0';
  }

  return sign ? -res : res;
}
