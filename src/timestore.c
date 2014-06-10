#include <string.h>
#include <curl/curl.h>

#include "libsvc/cfg.h"
#include "libsvc/htsmsg.h"
#include "libsvc/htsmsg_json.h"
#include "libsvc/curlhelpers.h"
#include "libsvc/trace.h"

#include "timestore.h"






void
timestore_put(int nodeid, double value)
{
  char url[1024];
  cfg_root(root);

  const char *urlprefix = cfg_get_str(root, CFG("timestoreServer"), NULL);
  if(urlprefix == NULL)
    return;

  snprintf(url, sizeof(url), "http://%s/nodes/%d/values", urlprefix, nodeid);

  htsmsg_t *doc = htsmsg_create_map();
  htsmsg_t *values = htsmsg_create_list();
  htsmsg_add_dbl(values, NULL, value);
  htsmsg_add_msg(doc, "values", values);

  char *data = htsmsg_json_serialize_to_str(doc, 0);
  htsmsg_destroy(doc);
  
  size_t len = strlen(data);

  FILE *f = fmemopen(data, len, "r");

  CURL *curl = curl_easy_init();
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &libsvc_curl_waste_output);
  curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
  curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
  curl_easy_setopt(curl, CURLOPT_READDATA, (void *)f);
  curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)len);

  CURLcode result = curl_easy_perform(curl);

  curl_easy_cleanup(curl);

  if(result)
    trace(LOG_ERR, "CURL Failed %s error %d", url, result);
  fclose(f);
  free(data);
}
