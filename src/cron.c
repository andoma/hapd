#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "libsvc/cfg.h"
#include "libsvc/talloc.h"
#include "libsvc/misc.h"

#include "cron.h"
#include "channel.h"
#include "sun.h"

LIST_HEAD(cron_entry_list, cron_entry);

/**
 *
 */
typedef struct cron_entry {

  LIST_ENTRY(cron_entry) ce_link;

  int ce_asserted;
  cfg_t *ce_conf;

} cron_entry_t;



static struct cron_entry_list cron_entries;
static int cron_need_reload;


typedef struct timeinfo {
  int dayminute;
  int dayminute_sunrise;
  int dayminute_sunset;
  int weekday;
  int sun_is_up;

} timeinfo_t;

/**
 *
 */
static int
get_dayminute_from_str(const char *str)
{
  int hour, min;
  if(sscanf(str, "%d:%d", &hour, &min) != 2)
    return -1;

  return hour * 60 + min;
}


/**
 *
 */
static int
check_hh_ss(const char *str, const timeinfo_t *ti)
{
  return get_dayminute_from_str(str) == ti->dayminute;
}

/**
 *
 */
static int
check_sunrise(const timeinfo_t *ti)
{
  return ti->dayminute_sunrise == ti->dayminute;
}


/**
 *
 */
static int
check_sunset(const timeinfo_t *ti)
{
  return ti->dayminute_sunset == ti->dayminute;
}

/**
 *
 */
static int
check_sunrise_if_earier_than(const char *str, const timeinfo_t *ti)
{
  return ti->dayminute_sunrise == ti->dayminute &&
    ti->dayminute < get_dayminute_from_str(str);
}


/**
 *
 */
static int
check_sunrise_if_later_than(const char *str, const timeinfo_t *ti)
{
  return ti->dayminute_sunrise == ti->dayminute &&
    ti->dayminute >= get_dayminute_from_str(str);
}


/**
 *
 */
static int
check_sunset_if_earier_than(const char *str, const timeinfo_t *ti)
{
  return ti->dayminute_sunset == ti->dayminute &&
    ti->dayminute < get_dayminute_from_str(str);
}


/**
 *
 */
static int
check_sunset_if_later_than(const char *str, const timeinfo_t *ti)
{
  return ti->dayminute_sunset == ti->dayminute &&
    ti->dayminute >= get_dayminute_from_str(str);
}


/**
 *
 */
static int
cron_entry_active(cfg_t *c, const timeinfo_t *ti)
{
  const char *str;

  if((str = htsmsg_get_str(c, "sun")) != NULL) {
    if(!strcmp(str, "up") && !ti->sun_is_up)
      return 0;

    if(!strcmp(str, "down") && ti->sun_is_up)
      return 0;
  }


  if((str = htsmsg_get_str(c, "at")) != NULL) {
    if(!strcmp(str, "sunrise"))
      return check_sunrise(ti);

    if(!strcmp(str, "sunset"))
      return check_sunset(ti);

    return check_hh_ss(str, ti);
  }

  if((str = htsmsg_get_str(c, "at-sunrise-if-earlier-than")) != NULL)
    return check_sunrise_if_earier_than(str, ti);

  if((str = htsmsg_get_str(c, "at-sunrise-if-laster-than")) != NULL)
    return check_sunrise_if_later_than(str, ti);

  if((str = htsmsg_get_str(c, "at-sunset-if-earlier-than")) != NULL)
    return check_sunset_if_earier_than(str, ti);

  if((str = htsmsg_get_str(c, "at-sunset-if-laster-than")) != NULL)
    return check_sunset_if_later_than(str, ti);

  return 0;
}


/**
 *
 */
static void
cron_entry_assert(cfg_t *c)
{
  cfg_t *actions = htsmsg_get_map(c, "actions");
  htsmsg_field_t *f;

  if(actions == NULL)
    return;

  HTSMSG_FOREACH(f, actions) {

    const char *chname = f->hmf_name;
    if(chname == NULL)
      continue;

    switch(f->hmf_type) {
    case HMF_STR:
      if(!strcmp(f->hmf_str, "on"))
        channel_set_binary(chname, 1, "cron");
      else if(!strcmp(f->hmf_str, "off"))
        channel_set_binary(chname, 0, "cron");
      break;
    }
  }
}


/**
 *
 */
static time_t
calc_suntime(int rising, const struct tm *tm)
{
  struct tm tmp = *tm;

  int dayminute = suntime(rising, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);

  tmp.tm_hour = dayminute / 60;
  tmp.tm_min  = dayminute % 60;
  tmp.tm_sec  = 0;

  return timegm(&tmp);
}


/**
 *
 */
static void
cron_check(void)
{
  struct tm tm, sunrise_tm, sunset_tm;
  time_t now;
  time(&now);

  localtime_r(&now, &tm);

  time_t sunrise = calc_suntime(1, &tm);
  time_t sunset  = calc_suntime(0, &tm);

  localtime_r(&sunrise, &sunrise_tm);
  localtime_r(&sunset, &sunset_tm);

  timeinfo_t ti = {0};
  ti.dayminute         =         tm.tm_hour * 60 + tm.tm_min;
  ti.dayminute_sunrise = sunrise_tm.tm_hour * 60 + sunrise_tm.tm_min;
  ti.dayminute_sunset  = sunset_tm.tm_hour  * 60 + sunset_tm.tm_min;
  ti.weekday = tm.tm_wday;

  ti.sun_is_up = 
    ti.dayminute >= ti.dayminute_sunrise &&
    ti.dayminute  < ti.dayminute_sunset;

  cron_entry_t *ce;

  LIST_FOREACH(ce, &cron_entries, ce_link) {
    if(!cron_entry_active(ce->ce_conf, &ti)) {
      ce->ce_asserted = 0;
      continue;
    }
    if(ce->ce_asserted)
      continue;

    cron_entry_assert(ce->ce_conf);
    ce->ce_asserted = 1;
  }
}


/**
 *
 */
static void
cron_reload(void)
{
  cron_entry_t *ce;
  htsmsg_field_t *f;

  while((ce = LIST_FIRST(&cron_entries)) != NULL) {
    htsmsg_destroy(ce->ce_conf);
    LIST_REMOVE(ce, ce_link);
    free(ce);
  }


  cfg_root(root);
  cfg_t *cron = cfg_get_list(root, "cron");
  if(cron == NULL)
    return;

  HTSMSG_FOREACH(f, cron) {
    cfg_t *entry = htsmsg_get_map_by_field(f);
    if(entry == NULL)
      continue;

    ce = calloc(1, sizeof(cron_entry_t));
    ce->ce_conf = htsmsg_copy(entry);
    LIST_INSERT_HEAD(&cron_entries, ce, ce_link);
  }
}


/**
 *
 */
static void *
cron_thread(void *aux)
{
  cron_need_reload = 1;
  while(1) {

    if(cron_need_reload) {
      cron_need_reload = 0;
      cron_reload();
    }

    cron_check();
    sleep(10);
  }
  return NULL;
}


/**
 *
 */
void
cron_init(void)
{
  pthread_t tid;
  pthread_create(&tid, NULL, cron_thread, NULL);
}


void
cron_refresh(void)
{
  cron_need_reload = 1;
}


