#include <math.h>
#include <stdlib.h>
#include "scrollstring.h"

uint16_t scroll_string(char *s, size_t n, char *display, size_t size, uint16_t state) {
  /* Make sure count is always smaller than n */
  uint16_t count = fmin(state & 0x7fff, n);
  char *start = s+abs(count);
  strncpy(display, start, size);
  return state;
}

uint16_t scroll_string_x_auto(char *s, size_t n, char *display, size_t size, uint16_t state) {
  uint8_t direction = (state & 0x8000) >> 15;
  /* Make sure count is always smaller than n */
  uint16_t count = fmin(state & 0x7fff, n);
  char *start = s+abs(count);
  scroll_string(s, n, display, size, state);
  if (*(start+size) == '\0' && n > size)
    direction=1;
  if (count == 0)
    direction=0;
  if (direction == 0)
    count++;
  else
    count--;
  return count | direction << 15;
}

uint16_t scroll_string_paged(char *s, size_t n, char *display, size_t size, uint16_t state) {
  uint16_t page = state;
  uint16_t offset = fmin(page*size/3*2, fmax(0, n-size*2/3));
  scroll_string(s, n, display, size, offset);
  return page;
}

uint16_t scroll_string_paged_inc(char *s, size_t n, size_t size, uint16_t state) {
  uint16_t page = state;
  if ((page+1)*size/3*2 < n) {
    page++;
  }
  return page;
}

uint16_t scroll_string_paged_dec(char *s, size_t n, size_t size, uint16_t state) {
  uint16_t page = state;
  if (page > 0) {
    page--;
  }
  return page;
}

uint16_t scroll_string_paged_total(char *s, size_t n, size_t pagesize) {
  return 1+n/pagesize*3/2;
}

void replace_chr(char *s, char r, char w, uint16_t length) {
  for (uint16_t i=0; i<length-2; i++) {
    if (s[i] == r)
      s[i] = w;
  }
  s[length - 1] = '\0';
}

void rstrip(char *s, uint16_t length) {
  char *index = s+length;
  do {
    index--;
  } while (index != s && (*index == ' ' ||
                          *index == '\0' ||
                          *index == '\t'));
  if (index != s) {
    *(index+1) = '\0';
  }
}
