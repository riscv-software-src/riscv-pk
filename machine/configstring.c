#include "configstring.h"
#include "encoding.h"
#include "mtrap.h"
#include <stdio.h>

static void query_mem(const char* config_string)
{
  query_result res = query_config_string(config_string, "ram{0{addr");
  assert(res.start);
  uintptr_t base = get_uint(res);
  res = query_config_string(config_string, "ram{0{size");
  mem_size = get_uint(res);
}

static void query_rtc(const char* config_string)
{
  query_result res = query_config_string(config_string, "rtc{addr");
  assert(res.start);
  mtime = (void*)get_uint(res);
}

static void query_harts(const char* config_string)
{
  for (int core = 0, hart; ; core++) {
    for (hart = 0; ; hart++) {
      char buf[32];
      snprintf(buf, sizeof buf, "core{%d{%d{isa", core, hart);
      query_result res = query_config_string(config_string, buf);
      if (!res.start)
        break;
      hls_t* hls = hls_init(num_harts);

      snprintf(buf, sizeof buf, "core{%d{%d{timecmp", core, hart);
      res = query_config_string(config_string, buf);
      assert(res.start);
      hls->timecmp = (void*)get_uint(res);

      num_harts++;
    }
    if (!hart)
      break;
  }
  assert(num_harts);
  assert(num_harts <= MAX_HARTS);
}

void parse_config_string()
{
  const char* s = (const char*)read_csr(mcfgaddr);
  query_mem(s);
  query_rtc(s);
  query_harts(s);
}
