#include <stdint.h>
#include <string.h>
#ifndef _SCROLLSTRING_H
#define _STROLLSTRING_H
uint16_t scroll_string(char *s, size_t n, char *display, size_t size, uint16_t state);
uint16_t scroll_string_paged_inc(char *s, size_t n, size_t size, uint16_t state);
uint16_t scroll_string_paged_dec(char *s, size_t n, size_t size, uint16_t state);
uint16_t scroll_string_paged(char *s, size_t n, char *display, size_t size, uint16_t state);
uint16_t scroll_string_paged_total(char *s, size_t n, size_t pagesize);

uint16_t scroll_string_x_auto(char *s, size_t n, char *display, size_t size, uint16_t state);

void replace_chr(char *s, char r, char w, uint16_t length);
void rstrip(char *s, uint16_t length);
#endif
