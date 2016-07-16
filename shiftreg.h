#ifndef SHIFTREG_H
#define SHIFTREG_H

#define GPIOCHIP_BASE 0x481AE000
#define GPIO_OUT 0x013c
#define GPIO_CLEARDATAOUT 0x0190
#define GPIO_SETDATAOUT 0x0194

#define BIT_TIME 1

typedef struct {
    unsigned int physaddr;
    int mem_fd;
    volatile void *virtaddr;
} sitara_gpio_t;

typedef struct {
    unsigned int nbits;
    char ser;
    char serclk;
    char latch;
    char clear;
    char *dataptr;
} shiftreg_t;


#endif

