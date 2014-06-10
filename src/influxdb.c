#include <string.h>
#include <curl/curl.h>

#include "libsvc/cfg.h"
#include "libsvc/htsmsg.h"
#include "libsvc/htsmsg_json.h"
#include "libsvc/curlhelpers.h"
#include "libsvc/trace.h"

#include "influxdb.h"






void
influxdb_put(const char *id, double value)
{
  char url[1024];
  cfg_root(root);

  const char *urlprefix = cfg_get_str(root, CFG("influxdb", "url"), NULL);
  const char *db        = cfg_get_str(root, CFG("influxdb", "db"), NULL);
  const char *username  = cfg_get_str(root, CFG("influxdb", "username"), NULL);
  const char *password  = cfg_get_str(root, CFG("influxdb", "password"), NULL);
  if(urlprefix == NULL || db == NULL || username == NULL || password == NULL)
    return;

  snprintf(url, sizeof(url), "%s/db/%s/series?u=%s&p=%s",
           urlprefix, db, username, password);

  htsmsg_t *doc = htsmsg_create_list();
  htsmsg_t *item = htsmsg_create_map();

  htsmsg_add_str(item, "name", id);

  htsmsg_t *columns = htsmsg_create_list();
  htsmsg_add_str(columns, NULL, "value");
  htsmsg_add_msg(item, "columns", columns);

  htsmsg_t *points = htsmsg_create_list();
  htsmsg_t *point = htsmsg_create_list();
  htsmsg_add_dbl(point, NULL, value);
  htsmsg_add_msg(points, NULL, point);
  htsmsg_add_msg(item, "points", points);

  htsmsg_add_msg(doc, NULL, item);

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
