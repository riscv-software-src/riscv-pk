#include <string.h>
#include "mtrap.h"
#include "platform_interface.h"

void print_logo()
{
  const char *logo = platform__get_logo();
  if (logo != NULL)
    putstring(logo);
}
