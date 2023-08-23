
/* System includes */
#include <stdlib.h>
#include <locale.h>

/* local includes */
#include <config.h>
#include <gmerlin/translation.h>

#include <gavl/log.h>
#define LOG_DOMAIN "controlpanel"

#include <gmerlin/utils.h>
#include <gmerlin/cmdline.h>

static char * addr = NULL;
gavf_io_t * io = NULL;


static void opt_addr(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -addr requires an argument");
    exit(-1);
    }

  addr = (*_argv)[arg];

  if(io)
    {
    gavf_io_destroy(io);
    io = NULL;
    }

  bg_cmdline_remove_arg(argc, _argv, arg);
  
  }

static void opt_set(void * data, int * argc, char *** _argv, int arg)
  {
  char * cmd;
  
  const char * var;
  const char * val;
  
  if(arg + 1 >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -set requires two arguments");
    exit(-1);
    }

  var = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);

  val = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);

  cmd = bg_sprintf("%s/command?c=set&var=%s&val=%s", addr, var, val);
#if 0
  bg_http_send_request(cmd, 0, NULL, &io);
  bg_http_read_response(gavf_io_t * io, int timeout,
                        char ** redirect,
                        gavl_dictionary_t * res)
#endif
    
  free(cmd);
  }

static void opt_cmd(void * data, int * argc, char *** _argv, int arg)
  {
  char * cmd;
  const char * var;

  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -cmd requires an argument");
    exit(-1);
    }

  var = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);

  cmd = bg_sprintf("%s/command?c=set&var=%s", addr, var);
  
  free(cmd);
  
  }

static void opt_get(void * data, int * argc, char *** _argv, int arg)
  {
  const char * var;
  char * cmd;

  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -cmd requires an argument");
    exit(-1);
    }

  var = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);
  
  cmd = bg_sprintf("%s/command?c=get&var=%s", addr, var);
  
  free(cmd);
  
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
    .arg = "-set",
    .help_string = "var val",
    .help_string = TRS("Set variable to value"),
    .callback =    opt_set,
   },
   {
    .arg = "-get",
    .help_string = "var",
    .help_string = TRS("Get value"),
    .callback =    opt_get,
   },
   {
    .arg = "-cmd",
    .help_string = "command",
    .help_string = TRS("Call command (e.g. button)"),
    .callback =    opt_cmd,
   },
   { },
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .synopsis = TRS("[options]\n"),
    .help_before = TRS("Remote control panel"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Global options"), global_options },
                                         {  } },
  };

int main(int argc, char ** argv)
  {
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  bg_cmdline_init(&app_data);

  if(argc < 2)
    bg_cmdline_print_help(argv[0], 0);

  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  
  
  }
