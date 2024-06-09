#include "headers/util.h"

void memcpy(void *dest, const void *src, size_t size) {
  char *_src = (char *)src;
  char *_dest = (char *)dest;

  for (size_t i = 0; i < size; ++i) {
    _dest[i] = _src[i];
  }
}
inline void bp() {}
