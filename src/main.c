#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <fcntl.h>
#include <errno.h>

#include "libsvc/libsvc.h"
#include "libsvc/trace.h"
#include "libsvc/cfg.h"
#include "libsvc/http.h"
#include "libsvc/ctrlsock.h"

#include "hap_input.h"
#include "zway.h"
#include "cron.h"

static int running = 1;
static int reload = 0;

/**
 *
 */
static void
handle_sigpipe(int x)
{
  return;
}


/**
 *
 */
static void
doexit(int x)
{
  running = 0;
}



/**
 *
 */
static void
doreload(int x)
{
  reload = 1;
}


/**
 *
 */
static void
refresh_subsystems(void)
{
  cron_refresh();
}



/**
 *
 */
static void
http_init(void)
{
  cfg_root(cr);

  int port = cfg_get_int(cr, CFG("http", "port"), 9900);
  const char *bindaddr = cfg_get_str(cr, CFG("http", "bindAddress"),
                                     "127.0.0.1");
  if(http_server_init(port, bindaddr))
    exit(1);

  http_serve_static("/public/static", "www/static");

  zway_gw_init();

}


/**
 *
 */
int
main(int argc, char **argv)
{
  int c;
  sigset_t set;
  const char *cfgfile = NULL;
  const char *defconf = "hapd.json";

  signal(SIGPIPE, handle_sigpipe);

  while((c = getopt(argc, argv, "c:s:")) != -1) {
    switch(c) {
    case 'c':
      cfgfile = optarg;
      break;
    case 's':
      enable_syslog("hapd", optarg);
      break;
    }
  }

  sigfillset(&set);
  sigprocmask(SIG_BLOCK, &set, NULL);

  srand48(getpid() ^ time(NULL));

  if(cfg_load(cfgfile, defconf)) {
    fprintf(stderr, "Unable to load config (check -c option). Giving up\n");
    exit(1);
  }

  libsvc_init();

  ctrlsock_init("/tmp/hapctrl");

  http_init();

  hap_input_init();

  cron_init();

  running = 1;
  sigemptyset(&set);
  sigaddset(&set, SIGTERM);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGHUP);

  signal(SIGTERM, doexit);
  signal(SIGINT, doexit);
  signal(SIGHUP, doreload);

  pthread_sigmask(SIG_UNBLOCK, &set, NULL);

  while(running) {
    if(reload) {
      reload = 0;
      if(!cfg_load(NULL, defconf)) {
        refresh_subsystems();
      }
    }
    pause();
  }

  return 0;
}
