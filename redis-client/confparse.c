/*
 Configuration parsing routines
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <locale.h>
#include "confparse.h"


/* validates a positive numeric option */
int conf_validate_pnumeric(cfg_t *cfg, cfg_opt_t *opt)
{
  int value = cfg_opt_getnint(opt, 0);

  if (value <= 0) {
    cfg_error(cfg, "Invalid numeric value %d in section '%s'", value, cfg_name(cfg));
    return -1;
  }
  return 0;
}
/* Verifies that a configuration parameter is set */
int conf_validate_isset(cfg_t *cfg, cfg_opt_t *opt)
{
  int value = cfg_opt_size(opt);

  if (value == 0) {
    cfg_error(cfg, "Required configuration option '%s' is missing!", cfg_name(cfg));
    return -1;
  }
  return -1;
}
cfg_t *parse_conf(const char *filename)
{

  cfg_opt_t opts[] = {
    CFG_STR("pru-rproc-node", "/dev/rproc30", CFGF_NONE),
    CFG_STR("redis-host", NULL, CFGF_NODEFAULT),
    CFG_INT("redis-port", 6379, CFGF_NONE),
    CFG_BOOL("redis-authrequired", cfg_false, CFGF_NONE),
    CFG_STR("redis-password", "anonymous", CFGF_NONE),
    CFG_INT("start-address", 0, CFGF_NODEFAULT),
    CFG_INT("num-channels", 0, CFGF_NODEFAULT),
    CFG_END()
  };

  cfg_t *cfg = cfg_init(opts, CFGF_NONE);

  cfg_set_validate_func(cfg, "redis-host", conf_validate_isset);
  cfg_set_validate_func(cfg, "redis-port", conf_validate_pnumeric);
  cfg_set_validate_func(cfg, "start-address", conf_validate_isset);
  cfg_set_validate_func(cfg, "num-channels", conf_validate_isset);
  
  switch (cfg_parse(cfg, filename)) {
    case CFG_FILE_ERROR:
      fprintf(stderr,"warning: configuration file '%s' could not be read: %s\n", filename, strerror(errno));
      return 0;
    case CFG_SUCCESS:
      break;
    case CFG_PARSE_ERROR:
      return 0;
  }

  return cfg;
}
