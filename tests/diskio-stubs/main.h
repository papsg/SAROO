/* Test-only hardware boundary; actual CMSIS declarations remain vendored. */
#ifndef SAROO_TEST_DISKIO_MAIN_H
#define SAROO_TEST_DISKIO_MAIN_H
#include <stdint.h>
#include "cmsis_os2.h"
typedef uint8_t u8;
typedef uint32_t u32;
int printk(const char *format, ...);
#endif
