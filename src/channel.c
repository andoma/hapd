#include <sys/param.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "libsvc/cfg.h"
#include "libsvc/trace.h"
#include "libsvc/cmd.h"

#include "channel.h"
#include "zway.h"
#include "hap_output.h"

/**
 *
 */
cfg_t *
channel_by_id(cfg_t *root, int id)
{
  cfg_t *channel_conf;
  htsmsg_field_t *f;
  cfg_t *channels_conf = cfg_get_list(root, "channels");
  if(channels_conf == NULL)
    return NULL;

  HTSMSG_FOREACH(f, channels_conf) {
    channel_conf = htsmsg_get_map_by_field(f);
    if(channel_conf == NULL)
      continue;
    if(htsmsg_get_u32_or_default(channel_conf, "id", 0) == id)
      return channel_conf;
  }
  return NULL;
}


/**
 *
 */
cfg_t *
channel_by_name(cfg_t *root, const char *name)
{
  cfg_t *channel_conf;
  htsmsg_field_t *f;
  cfg_t *channels_conf = cfg_get_list(root, "channels");
  if(channels_conf == NULL)
    return NULL;

  HTSMSG_FOREACH(f, channels_conf) {
    channel_conf = htsmsg_get_map_by_field(f);
    if(channel_conf == NULL)
      continue;
    const char *str = htsmsg_get_str(channel_conf, "name");
    if(str == NULL)
      continue;
    if(!strcmp(str, name))
      return channel_conf;
  }
  return NULL;
}


/**
 *
 */
static void
channel_set_binary_internal(cfg_t *ch, int on, const char *source)
{
  trace(LOG_INFO, "Setting channel %s to %s on behalf of %s",
        htsmsg_get_str(ch, "name"), on ? "on" : "off", source);

  zway_set_channel(ch, on);
  hap_set_channel(ch, on);
}

/**
 *
 */
void
channel_set_binary(const char *chname, int on, const char *source)
{
  cfg_root(root);
  cfg_t *ch = channel_by_name(root, chname);
  if(ch == NULL) {
    trace(LOG_ERR, "Unable to set channel %s to %s -- Channel not known",
          chname, on ? "on" : "off");
    return;
  }

  channel_set_binary_internal(ch, on, source);
}


/**
 *
 */
static void
channel_set_dim_internal(cfg_t *ch, int level, const char *source)
{
  level = MAX(MIN(level, 100), 0);

  trace(LOG_INFO, "Setting channel %s to %d%% on behalf of %s",
        htsmsg_get_str(ch, "name"), level, source);

  hap_set_channel(ch, -level);
}

/**
 *
 */
void
channel_set_dim(const char *chname, int level, const char *source)
{
  cfg_root(root);
  cfg_t *ch = channel_by_name(root, chname);
  if(ch == NULL) {
    trace(LOG_ERR,
          "Unable to set channel %s to %d%% -- Channel not known",
          chname, level);
    return;
  }

  channel_set_dim_internal(ch, level, source);
}



static int
channel_cli_set(const char *user,
                int argc, const char **argv, int *intv,
                void (*msg)(void *opaque, const char *fmt, ...),
                void *opaque)
{
  cfg_root(root);

  const char *chname = argv[0];
  cfg_t *ch = channel_by_name(root, chname);
  if(ch == NULL) {
    msg(opaque, "Channel %s not found", chname);
    return 1;
  }

  int level = atoi(argv[1]);
  if(level) {
    msg(opaque, "Setting chanel %s to %d%%", chname, level);

    channel_set_dim_internal(ch, level, user);

  } else {
    int on = !strcmp(argv[1], "on");
    msg(opaque, "Setting chanel %s to %s", chname,
        on ? "on" : "off");

    channel_set_binary_internal(ch, on, user);
  }

  return 0;
}

CMD(channel_cli_set,
    CMD_LITERAL("set"),
    CMD_VARSTR("channel"),
    CMD_VARSTR("on | off | percentage"));
