#ifndef SHIFTREG_H
#define SHIFTREG_H

#define MAX_BITS 256
#define BIT_TIME_CYC 16

#define min(X, Y)  ((X) < (Y) ? (X) : (Y))

typedef struct {
    unsigned int nbits;
    // PRU r30 bit number
    char ser0;
    char ser1;
    char ser2;
    char serclk;
    char latch;
    char clear;
} shiftreg_t;

void shiftreg_clear(shiftreg_t *sr);
void shiftreg_iterate(shiftreg_t *sr, char *ser0_val, char *ser1_val, char *ser2_val, unsigned char count);

#endif

