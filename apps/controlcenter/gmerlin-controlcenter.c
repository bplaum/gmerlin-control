#include <locale.h>
#include <string.h>
#include <signal.h>


#include <config.h>

#include "controlcenter.h"

#include <gmerlin/translation.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/utils.h>
#include <gmerlin/application.h>

#include <gavl/log.h>
#define LOG_DOMAIN "controlcenter"


static char * label = "Control center";
static char * addr = "127.0.0.1";
static int port = 0;
//static char * configfile = NULL;

control_center_t center;

static void opt_c(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -c requires an argument");
    exit(-1);
    }

  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_addr(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -addr requires an argument");
    exit(-1);
    }

  addr = (*_argv)[arg];
  
  bg_cmdline_remove_arg(argc, _argv, arg);
  
  }

static void opt_port(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -p requires an argument");
    exit(-1);
    }

  port = atoi((*_argv)[arg]);
  
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_label(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -l requires an argument");
    exit(-1);
    }

  label = (*_argv)[arg];
  
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static bg_cmdline_arg_t global_options[] =
  {
   {
    .arg = "-addr",
    .help_string = "addr",
    .help_string = TRS("Address to bind to"),
    .callback =    opt_addr,
    
   },
   {
    .arg = "-p",
    .help_string = "port",
    .help_string = TRS("Port to bind to"),
    .callback =    opt_port,
   },
   {
    .arg = "-l",
    .help_string = "label",
    .help_string = TRS("Label"),
    .callback =    opt_label,
   },
   {
      .arg =         "-c",
      .help_arg =    "\"control_file\"",
      .help_string = TRS("Load controls from file"),
      .callback =    opt_c,
    },
    { /* End of options */ }
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .synopsis = TRS("[options]\n"),
    .help_before = TRS("Control center"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Global options"), global_options },
                                         {  } },
  };


int main(int argc, char ** argv)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 50; // 20 ms
  
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  bg_handle_sigint();
  signal(SIGPIPE, SIG_IGN);
  
  bg_cmdline_init(&app_data);
  bg_plugins_init();
  
  controlcenter_init(&center);
  
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  bg_app_init("gmerlin-controlcenter", label, "controlcenter");
  
#if 0
  gavl_value_init(&val);
  gavl_value_set_string_nocopy(&val, addr);
  bg_http_server_set_parameter(srv, "addr", &val);

  /* Don't free here! */
  gavl_value_init(&val);
  gavl_value_set_int(&val, port);
  bg_http_server_set_parameter(srv, "port", &val);
#endif
 
  
  while(1)
    {
    if(!controlcenter_iteration(&center))
      gavl_time_delay(&delay_time);
    
    if(bg_got_sigint())
      {
      fprintf(stderr, "Got sigint");
      break;
      }
    }
  
  
  }
