#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "libsvc/cfg.h"
#include "libsvc/trace.h"
#include "libsvc/cmd.h"

#include "channel.h"
#include "zway.h"

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



static int
channel_cli_set(const char *user,
                int argc, const char **argv, int *intv,
                void (*msg)(void *opaque, const char *fmt, ...),
                void *opaque)
{
  cfg_root(root);
  int on = !strcmp(argv[1], "on");

  const char *chname = argv[0];
  cfg_t *ch = channel_by_name(root, chname);
  if(ch == NULL) {
    msg(opaque, "Channel %s not found", chname);
    return 1;
  }

  msg(opaque, "Setting chanel %s to %s", chname,
      on ? "on" : "off");

  channel_set_binary_internal(ch, on, user);
  return 0;
}

CMD(channel_cli_set,
    CMD_LITERAL("set"),
    CMD_VARSTR("channel"),
    CMD_VARSTR("on | off"));
