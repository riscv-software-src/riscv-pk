#include "configstring.h"
#include "encoding.h"
#include "mtrap.h"
#include "atomic.h"
#include <stdio.h>

static void query_mem(const char* config_string)
{
  query_result res = query_config_string(config_string, "ram{0{addr");
  assert(res.start);
  uintptr_t base = get_uint(res);
  assert(base == DRAM_BASE);
  res = query_config_string(config_string, "ram{0{size");
  mem_size = get_uint(res);
}

static void query_rtc(const char* config_string)
{
  query_result res = query_config_string(config_string, "rtc{addr");
  assert(res.start);
  mtime = (void*)(uintptr_t)get_uint(res);
}

static void query_plic(const char* config_string)
{
  query_result res = query_config_string(config_string, "plic{priority");
  if (!res.start)
    return;
  plic_priorities = (uint32_t*)(uintptr_t)get_uint(res);

  res = query_config_string(config_string, "plic{ndevs");
  if (!res.start)
    return;
  plic_ndevs = get_uint(res);
}

static void query_hart_plic(const char* config_string, hls_t* hls, int core, int hart)
{
  char buf[32];
  snprintf(buf, sizeof buf, "core{%d{%d{plic{m{ie", core, hart);
  query_result res = query_config_string(config_string, buf);
  if (res.start)
    hls->plic_m_ie = (void*)(uintptr_t)get_uint(res);

  snprintf(buf, sizeof buf, "core{%d{%d{plic{m{thresh", core, hart);
  res = query_config_string(config_string, buf);
  if (res.start)
    hls->plic_m_thresh = (void*)(uintptr_t)get_uint(res);

  snprintf(buf, sizeof buf, "core{%d{%d{plic{s{ie", core, hart);
  res = query_config_string(config_string, buf);
  if (res.start)
    hls->plic_s_ie = (void*)(uintptr_t)get_uint(res);

  snprintf(buf, sizeof buf, "core{%d{%d{plic{s{thresh", core, hart);
  res = query_config_string(config_string, buf);
  if (res.start)
    hls->plic_s_thresh = (void*)(uintptr_t)get_uint(res);
}

static void query_harts(const char* config_string)
{
  for (int core = 0, hart; ; core++) {
    for (hart = 0; ; hart++) {
      char buf[32];
      snprintf(buf, sizeof buf, "core{%d{%d{ipi", core, hart);
      query_result res = query_config_string(config_string, buf);
      if (!res.start)
        break;
      hls_t* hls = hls_init(num_harts);
      hls->ipi = (void*)(uintptr_t)get_uint(res);

      query_hart_plic(config_string, hls, core, hart);

      snprintf(buf, sizeof buf, "core{%d{%d{timecmp", core, hart);
      res = query_config_string(config_string, buf);
      assert(res.start);
      hls->timecmp = (void*)(uintptr_t)get_uint(res);

      mb();

      // wake up the hart
      *hls->ipi = 1;

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
  uint32_t addr = *(uint32_t*)CONFIG_STRING_ADDR;
  const char* s = (const char*)(uintptr_t)addr;
  query_mem(s);
  query_plic(s);
  query_rtc(s);
  query_harts(s);
}
