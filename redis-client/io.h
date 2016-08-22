#ifndef _IO_H
#define _IO_H
int io_init(const char *devname);
int io_set_nchannels(int fd, uint16_t channels);
int io_set_pwm(int fd, uint8_t chainaddr, uint8_t pinaddr, uint8_t value);
void io_close(int fd);
#endif
