#ifndef SHIFTREG_H
#define SHIFTREG_H

#define MAX_BITS 256
#define BIT_TIME_CYC 128

typedef struct {
    unsigned int nbits;
    // PRU r30 bit number
    char ser;
    char serclk;
    char latch;
    char clear;
} shiftreg_t;

void shiftreg_clear(shiftreg_t *sr);
void shiftreg_iterate(shiftreg_t *sr, char *pwmval, unsigned char count);

#endif

