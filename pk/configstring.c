#include "encoding.h"
#include "mtrap.h"

static const char* skip_whitespace(const char* str)
{
  while (*str && *str <= ' ')
    str++;
  return str;
}

static const char* skip_string(const char* str)
{
  while (*str && *str++ != '"')
    ;
  return str;
}

static int is_hex(char ch)
{
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

static int parse_hex(char ch)
{
  return (ch >= '0' && ch <= '9') ? ch - '0' :
         (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 :
                                    ch - 'A' + 10;
}

static const char* skip_key(const char* str)
{
  while (*str >= 35 && *str <= 122 && *str != ';')
    str++;
  return str;
}

typedef struct {
  const char* start;
  const char* end;
} query_result;

static query_result query_config_string(const char* str, const char* k)
{
  size_t ksize = 0;
  while (k[ksize] && k[ksize] != '{')
    ksize++;
  int last = !k[ksize];

  query_result res = {0, 0};
  while (1) {
    const char* key_start = str = skip_whitespace(str);
    const char* key_end = str = skip_key(str);
    int match = (key_end - key_start) == ksize;
    if (match)
      for (size_t i = 0; i < ksize; i++)
        if (key_start[i] != k[i])
          match = 0;
    const char* value_start = str = skip_whitespace(str);
    while (*str != ';') {
      if (!*str) {
        return res;
      } else if (*str == '"') {
        str = skip_string(str+1);
      } else if (*str == '{') {
        const char* search_key = match && !last ? k + ksize + 1 : "";
        query_result inner_res = query_config_string(str + 1, search_key);
        if (inner_res.start)
          return inner_res;
        str = inner_res.end + 1;
      } else {
        str = skip_key(str);
      }
      str = skip_whitespace(str);
    }
    res.end = str;
    if (match && last) {
      res.start = value_start;
      return res;
    }
    str = skip_whitespace(str+1);
    if (*str == '}') {
      res.end = str;
      return res;
    }
  }
}

static void parse_string(query_result r, char* buf)
{
  if (r.start < r.end) {
    if (*r.start == '"') {
      for (const char* p = r.start + 1; p < r.end && *p != '"'; p++) {
        char ch = p[0];
        if (ch == '\\' && p[1] == 'x' && is_hex(p[2])) {
          ch = parse_hex(p[2]);
          if (is_hex(p[3])) {
            ch = (ch << 4) + parse_hex(p[3]);
            p++;
          }
          p += 2;
        }
        *buf++ = ch;
      }
    } else {
      for (const char* p = r.start; p < r.end && *p > ' '; p++)
        *buf++ = *p;
    }
  }
  *buf = 0;
}

#define get_string(name, search_res) \
  char name[(search_res).end - (search_res).start + 1]; \
  parse_string(search_res, name)

static unsigned long __get_uint_hex(const char* s)
{
  unsigned long res = 0;
  while (*s) {
    if (is_hex(*s))
      res = (res << 4) + parse_hex(*s);
    else if (*s != '_')
      break;
    s++;
  }
  return res;
}

static unsigned long __get_uint_dec(const char* s)
{
  unsigned long res = 0;
  while (*s) {
    if (*s >= '0' && *s <= '9')
      res = res * 10 + (*s - '0');
    else
      break;
    s++;
  }
  return res;
}

static unsigned long __get_uint(const char* s)
{
  if (s[0] == '0' && s[1] == 'x')
    return __get_uint_hex(s+2);
  return __get_uint_dec(s);
}

static unsigned long get_uint(query_result res)
{
  get_string(s, res);
  return __get_uint(s);
}

static long get_sint(query_result res)
{
  get_string(s, res);
  if (s[0] == '-')
    return -__get_uint(s+1);
  return __get_uint(s);
}

static void query_mem(const char* config_string)
{
  query_result res = query_config_string(config_string, "ram{0{addr");
  kassert(res.start);
  uintptr_t base = get_uint(res);
  res = query_config_string(config_string, "ram{0{size");
  mem_size = get_uint(res);
  debug_printk("at %p, found %d MiB of memory\n", base, (int)(mem_size >> 20));
}

static void query_harts(const char* config_string)
{
  for (int core = 0, hart; ; core++) {
    for (hart = 0; ; hart++) {
      char buf[32];
      snprintf(buf, sizeof buf, "core{%d{%d{addr", core, hart);
      query_result res = query_config_string(config_string, buf);
      if (!res.start)
        break;
      csr_t* base = (csr_t*)get_uint(res);
      uintptr_t hart_id = base[CSR_MHARTID];
      hls_init(hart_id, base);
      debug_printk("at %p, found hart %ld\n", base, hart_id);
      kassert(num_harts++ == hart_id);
    }
    if (!hart)
      break;
  }
}

void parse_config_string()
{
  const char* s = (const char*)read_csr(mcfgaddr);
  debug_printk("%s\n", s);
  query_mem(s);
  query_harts(s);
}
