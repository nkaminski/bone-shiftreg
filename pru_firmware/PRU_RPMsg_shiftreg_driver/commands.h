#define COMMAND_LEN 3
typedef enum {
  NOOP,
  SET_PWM0,
  SET_PWM1,
  SET_PWM2,
  SET_PWM_ALL,
  SET_NBITS
} opcode_t;
