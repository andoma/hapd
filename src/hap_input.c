#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "libsvc/cfg.h"
#include "libsvc/trace.h"

#include "hap_input.h"
#include "timestore.h"
#include "influxdb.h"


struct pkt_channel_value {
  uint16_t magic;
  uint8_t  version;
  uint8_t  command;
  uint16_t channel;
  uint32_t value;
  uint32_t clock;
} __attribute__((packed));


struct channelvalue {
  uint16_t channel;
  int32_t value;
} __attribute__((packed));


struct pkt_channel_values_multi {
  uint16_t magic;
  uint8_t  version;
  uint8_t  command;

  uint32_t clock;

  struct channelvalue values[0];

} __attribute__((packed));



/**
 *
 */
typedef struct channel {
  LIST_ENTRY(channel) link;
 
  uint16_t id;
 
  int last_value;
  int last_time;

  int last_valid;

  time_t not_before;

} channel_t;

LIST_HEAD(channel_list, channel);

/**
 *
 */
static struct channel_list channels;


/**
 *
 */
static channel_t *
find_channel(uint16_t id)
{
  channel_t *c;
  LIST_FOREACH(c, &channels, link)
    if(c->id == id)
      return c;


  c = calloc(1, sizeof(channel_t));

  c->id = id;
  LIST_INSERT_HEAD(&channels, c, link);
  return c;
}


/**
 *
 */
static void
handle_input(cfg_t *channels_conf, uint16_t channel,
	     int32_t value, uint32_t tim)
{
  channel_t *c = find_channel(channel);

  htsmsg_field_t *f;
  cfg_t *channel_conf = NULL;
  HTSMSG_FOREACH(f, channels_conf) {
    channel_conf = htsmsg_get_map_by_field(f);
    if(channel_conf == NULL)
      continue;
    if(htsmsg_get_u32_or_default(channel_conf, "id", 0) == c->id)
      break;
  }

  if(channel_conf == NULL)
    return;


  uint32_t min_interval;
  if(!htsmsg_get_u32(channel_conf, "mininterval", &min_interval)) {
    time_t now = time(NULL);
    if(c->not_before > now)
      return;

    c->not_before = now + min_interval;
  }

  int timestoreNodeId = htsmsg_get_u32_or_default(channel_conf,
						  "timestoreNodeId",
						  0);

  const char *influxid = htsmsg_get_str(channel_conf, "influxId");

  const char *unit = htsmsg_get_str(channel_conf, "unit");

  int dolog = htsmsg_get_u32_or_default(channel_conf, "log", 0);

  float outval = 0;

  if(!strcmp(unit, "RH")) {
    if(dolog)
      trace(LOG_DEBUG, "HAP input: channel:%d value:%d%% RH (node: %d)",
            channel, value, timestoreNodeId);

    outval = value;

  } else  if(!strcmp(unit, "dC")) {

    if(dolog)
      trace(LOG_DEBUG, "HAP input: channel:%d value:%.1fC (node: %d)",
            channel, value / 10.0f, timestoreNodeId);

    outval = value / 10.0f;
  } else if(!strcmp(unit, "1000/kWh")) {

    uint32_t dv = value - c->last_value;
    uint32_t dt = tim   - c->last_time;

    c->last_value = value;
    c->last_time  = tim;

    if(dt < 3600000 && c->last_valid) {
      int value = 3600000 * dv / dt;

      if(dolog)
        trace(LOG_DEBUG, "HAP input: channel:%d value:%dW (node: %d)",
              channel, value, timestoreNodeId);

      outval = value;
    } else {
      c->last_valid = 1;
      return;
    }

    c->last_valid = 1;

  } else {
    return;
  }

  if(timestoreNodeId)
    timestore_put(timestoreNodeId, outval);

  if(influxid)
    influxdb_put(influxid, outval);

}


/**
 *
 */
static void
handle_single_channel_input(const struct pkt_channel_value *pcv)
{
  cfg_root(root);

  cfg_t *channels_conf = cfg_get_list(root, "channels");
  if(channels_conf == NULL)
    return;

  handle_input(channels_conf, pcv->channel, pcv->value, pcv->clock);
}


/**
 *
 */
static void
handle_multi_channel_input(const struct pkt_channel_values_multi *pcv,
			   int pktsize)
{
  cfg_root(root);

  cfg_t *channels_conf = cfg_get_list(root, "channels");
  if(channels_conf == NULL)
    return;

  pktsize -= sizeof(struct pkt_channel_values_multi);

  int idx = 0;
  while(pktsize >= sizeof(struct channelvalue)) {
    uint16_t channel = pcv->values[idx].channel;
    int value        = pcv->values[idx].value;

    handle_input(channels_conf, channel, value, pcv->clock);
    idx++;
    pktsize -= sizeof(struct channelvalue);
  }
}


/**
 *
 */
static void *
hap_input_thread(void *arg)
{
  struct sockaddr_in sin = {0};

  sin.sin_family = AF_INET;
  sin.sin_port = htons(8642);

  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  bind(s, (struct sockaddr *)&sin, sizeof(sin));

  const struct pkt_channel_value *pcv;
  uint8_t buf[1024];
  while(1) {
    int r;
    r = read(s, buf, sizeof(buf));
    if(r < 4)
      continue;

    pcv = (struct pkt_channel_value *)&buf[0];
    if(pcv->magic != 0x4e75)
      continue;
    if(pcv->version != 1)
      continue;

    if(pcv->command == 1)
      handle_single_channel_input(pcv);
    if(pcv->command == 2)
      handle_multi_channel_input((void *)pcv, r);
  }
  return NULL;
}


void
hap_input_init(void)
{
  pthread_t tid;
  pthread_create(&tid, NULL, hap_input_thread, NULL);
}
