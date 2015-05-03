#include <string.h>
#include "file.h"

#define W 46
#define D 21
#define T 5

void print_logo()
{
  static const char end[] = "\n       INSTRUCTION SETS WANT TO BE FREE\n\n";
  static const char fill[T] = {'r', ' ', 'v', ' ', 'r'};
  static const char logo[D][T] =
   {{0, 14, 46, 46, 46},
    {0, 18, 46, 46, 46},
    {13, 20, 46, 46, 46},
    {16, 22, 46, 46, 46},
    {18, 22, 46, 46, 46},
    {18, 22, 46, 46, 46},
    {18, 22, 46, 46, 46},
    {16, 22, 44, 46, 46},
    {13, 20, 42, 46, 46},
    {2, 18, 40, 46, 46},
    {2, 14, 38, 44, 46},
    {4, 10, 36, 42, 46},
    {6, 12, 34, 40, 46},
    {8, 14, 32, 38, 46},
    {10, 16, 30, 36, 46},
    {12, 18, 28, 34, 46},
    {14, 20, 26, 32, 46},
    {16, 22, 24, 30, 46},
    {18, 22, 22, 28, 46},
    {20, 20, 20, 26, 46},
    {22, 22, 22, 24, 46}};

  char result[D*(W+1) + sizeof(end)];
  char* pos = result;
  for (size_t i = 0; i < D; i++) {
    for (size_t j = 0, p = 0; j < T; j++) {
      if (p == logo[i][j])
        continue;
      do {
        *pos++ = fill[j];
        p++;
      } while (p < logo[i][j]);
    }
    *pos++ = '\n';
  }
  memcpy(pos, end, sizeof(end));

  file_write(stderr, result, (pos - result) + sizeof(end));
}
