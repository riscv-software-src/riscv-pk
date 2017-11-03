#include <string.h>
#include "mtrap.h"

static const char logo[] =
"              vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
"                  vvvvvvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrr       vvvvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrrrrr      vvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrrrrrrr    vvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrrrrrrr    vvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrrrrrrr    vvvvvvvvvvvvvvvvvvvvvvvv\n"
"rrrrrrrrrrrrrrrr      vvvvvvvvvvvvvvvvvvvvvv  \n"
"rrrrrrrrrrrrr       vvvvvvvvvvvvvvvvvvvvvv    \n"
"rr                vvvvvvvvvvvvvvvvvvvvvv      \n"
"rr            vvvvvvvvvvvvvvvvvvvvvvvv      rr\n"
"rrrr      vvvvvvvvvvvvvvvvvvvvvvvvvv      rrrr\n"
"rrrrrr      vvvvvvvvvvvvvvvvvvvvvv      rrrrrr\n"
"rrrrrrrr      vvvvvvvvvvvvvvvvvv      rrrrrrrr\n"
"rrrrrrrrrr      vvvvvvvvvvvvvv      rrrrrrrrrr\n"
"rrrrrrrrrrrr      vvvvvvvvvv      rrrrrrrrrrrr\n"
"rrrrrrrrrrrrrr      vvvvvv      rrrrrrrrrrrrrr\n"
"rrrrrrrrrrrrrrrr      vv      rrrrrrrrrrrrrrrr\n"
"rrrrrrrrrrrrrrrrrr          rrrrrrrrrrrrrrrrrr\n"
"rrrrrrrrrrrrrrrrrrrr      rrrrrrrrrrrrrrrrrrrr\n"
"rrrrrrrrrrrrrrrrrrrrrr  rrrrrrrrrrrrrrrrrrrrrr\n"
"\n"
"       INSTRUCTION SETS WANT TO BE FREE\n";

void print_logo()
{
  putstring(logo);
}
