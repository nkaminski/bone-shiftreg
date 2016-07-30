#ifndef REGISTERS_H
#define REGISTERS_H
#include <stdint.h>

#define R30_SET(x) __R30 |= (1 << (x))
#define R30_CLR(x) __R30 &= ~(1 << (x))

volatile register uint32_t __R30;
volatile register uint32_t __R31;

#endif
