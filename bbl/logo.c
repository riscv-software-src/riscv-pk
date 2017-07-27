#include <string.h>
#include "mtrap.h"
#include "platform_interface.h"

void print_logo()
{
  putstring(platform__get_logo());
}
