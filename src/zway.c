#include <string.h>
#include <stdio.h>

#include "libsvc/http.h"
#include "libsvc/cfg.h"
#include "libsvc/talloc.h"
#include "libsvc/misc.h"
#include "libsvc/curlhelpers.h"
#include "libsvc/trace.h"

#include "zway.h"

/**
 *
 */
int
zway_set_channel(cfg_t *ch, int value)
{
  const char *host = cfg_get_str(ch, CFG("zwayhost"), NULL);

  if(host == NULL)
    return -1;

  int device   = cfg_get_int(ch, CFG("zwave", "device"), -1);
  int instance = cfg_get_int(ch, CFG("zwave", "instance"), -1);

  if(device == -1 || instance == -1)
    return -1;

  trace(LOG_DEBUG, "zway: Setting %s to %d",
        htsmsg_get_str(ch, "name") ?: "<noname>",
        value);

  char *cmd = tsprintf("/ZWaveAPI/Run/devices[%d].instances[%d].commandClasses[0x25].Set(%d)",
                       device, instance, value);

  char *path = url_escape_tmp(cmd, URL_ESCAPE_PATH);
  char *url = tsprintf("http://%s%s", host, path);

  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, libsvc_curl_waste_output);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "hapd");
  curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

  curl_easy_perform(curl);
  curl_easy_cleanup(curl);
  return 0;
}


/**
 *
 */
void
zway_gw_init(void)
{
}
