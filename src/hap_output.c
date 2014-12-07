#include <sys/param.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "libsvc/http.h"
#include "libsvc/cfg.h"
#include "libsvc/talloc.h"
#include "libsvc/misc.h"
#include "libsvc/curlhelpers.h"
#include "libsvc/trace.h"

#include "hap_output.h"

struct pkt_set_channel {

  uint16_t magic;
  uint8_t  version;
  uint8_t  command;

  uint16_t channel;
  uint32_t value;

} __attribute__((packed));



/**
 *
 */
int
hap_set_channel(cfg_t *ch, int value)
{
  const char *host = cfg_get_str(ch, CFG("haphost"), NULL);

  if(host == NULL)
    return -1;

  int channel = cfg_get_int(ch, CFG("hap", "channel"), -1);

  if(channel == -1)
    return -1;

  trace(LOG_DEBUG, "hap: Setting %s (#%d) to %d via %s",
        htsmsg_get_str(ch, "name") ?: "<noname>",
        channel, value, host);

  struct sockaddr_in sin = {0};

  sin.sin_family = AF_INET;
  sin.sin_port = htons(8642);
  sin.sin_addr.s_addr = inet_addr(host);

  struct pkt_set_channel psc;

  psc.magic = 0x4e75;
  psc.version = 1;
  psc.command = 3; // SET channel

  psc.channel = channel;
  if(value < 0) {
    value = MIN(0xf, -value / 6.26f);
    psc.value = 0x10 | value;
  } else {
    psc.value = !!value;
  }

  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

  if(sendto(s, &psc, sizeof(psc), 0,
            (struct sockaddr *)&sin, sizeof(struct sockaddr_in))) {
    trace(LOG_ERR, "sendto failed -- %s", strerror(errno));
  }
  close(s);
  return 0;
}
