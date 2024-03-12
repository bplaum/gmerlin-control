
#include <string.h>
#include <signal.h>
#include <locale.h>

#include <uuid/uuid.h>

#include <config.h>

#include <gavl/metatags.h>
#include <gavl/gavlsocket.h>


#include <gmerlin/cfg_registry.h>
#include <gmerlin/cfgctx.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/utils.h>
// #include <gmerlin/remote.h>
#include <gmerlin/translation.h>
#include <gmerlin/player.h>
#include <gmerlin/playermsg.h>

#include <gmerlin/bgmsg.h>
#include <gmerlin/http.h>

#include <gmerlin/bggavl.h>
#include <gmerlin/log.h>
#include <gmerlin/application.h>
#include <gmerlin/resourcemanager.h>
#include <control.h>

#define LOG_DOMAIN "gmerlin-control"

static char * addr = NULL;
static int delay = 0;
static int flags = 0;

#define FLAG_MONITOR       (1<<0)
#define FLAG_IDLE          (1<<1)
#define FLAG_DUMP_CONTROLS (1<<2)
#define FLAG_HAVE_STATE    (1<<3)
#define FLAG_COMMANDS_SENT (1<<4)
#define FLAG_REMOTE        (1<<5) // Talking to a remote controlcenter

bg_control_t ctrl;

static gavl_dictionary_t controls;
// static gavl_dictionary_t state;


static char * extract_var_val(const char * arg, gavl_value_t * val)
  {
  char * var;
  const char * pos;
  const gavl_dictionary_t * control;
    
  pos = strchr(arg, '=');
  if(!pos)
    return NULL;

  var = gavl_strndup(arg, pos);
  pos++;

  if(!(control = gavl_control_get_child(&controls, var)))
    {
    free(var);
    return NULL;
    }
  val->type = gavl_control_get_type(control);
  gavl_value_from_string(val, pos);
  return var;
  }

static void set_remote(const char * arg, const char * cmd)
  {
  char * ctx;
  char * val;
  gavl_buffer_t buf;
    
  // gmerlin-control -addr example.host:8886 -set /path/var=1 -delay 10
  // -> http://example.host:8886/set/path/var?v=1&delay=10
    
  char * uri;
    
  ctx = gavl_strdup(arg);

  if(!(val = strchr(ctx, '=')))
    {
    /* Error */
    }

  *val = '\0';
  val++;
    
  uri = gavl_sprintf("http%s/%s%s?v=%s", strstr(addr, "://"), cmd, ctx, val);

  if(delay > 0)
    {
    char * tmp_string = gavl_sprintf("&delay=%d", delay);
    uri = gavl_strcat(uri, tmp_string);
    free(tmp_string);
    }

  gavl_buffer_init(&buf);
    
  if(!bg_http_get(uri, &buf, NULL))
    {
    /* Error */
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "bg_http_get failed for %s", uri);
    
    }

  gavl_buffer_free(&buf);
  free(uri);
  
  }

static void cmd_set(void * data, int * argc, char *** argv, int arg)
  {
  
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -set requires an argument");
    exit(-1);
    }

  if(flags & FLAG_REMOTE)
    {
    set_remote((*argv)[arg], "set");
    }
  else
    {
    char * var;
    gavl_value_t val;
    gavl_msg_t * msg;
    gavl_value_init(&val);
    
    if(!(var = extract_var_val((*argv)[arg], &val)))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid argument: %s", (*argv)[arg]);
      exit(-1);
      }
    msg = bg_msg_sink_get(ctrl.cmd_sink);

    gavl_msg_set_state(msg, GAVL_CMD_SET_STATE,
                       1, "/", var, &val);
  
    if(delay > 0)
      gavl_dictionary_set_int(&msg->header, GAVL_CONTROL_DELAY, delay);
  
    bg_msg_sink_put(ctrl.cmd_sink);
    flags &= ~FLAG_IDLE;
    gavl_value_free(&val);
    }
  
  bg_cmdline_remove_arg(argc, argv, arg);
  }

static void cmd_setrel(void * data, int * argc, char *** argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -set-rel requires an argument");
    exit(-1);
    }
  
  if(flags & FLAG_REMOTE)
    {
    set_remote((*argv)[arg], "setrel");
    }
  else
    {
    char * var;
    gavl_value_t val;
  
    gavl_value_init(&val);
    
    if(!(var = extract_var_val((*argv)[arg], &val)))
      exit(-1);
    
    if(gavl_control_handle_set_rel(&controls, "/", var, &val))
      {
      gavl_msg_t * msg;
      msg = bg_msg_sink_get(ctrl.cmd_sink);
      gavl_msg_set_state(msg, GAVL_CMD_SET_STATE,
                         1, "/", var, &val);

      if(delay > 0)
        gavl_dictionary_set_int(&msg->header, GAVL_CONTROL_DELAY, delay);
  
      bg_msg_sink_put(ctrl.cmd_sink);
      }
    flags &= ~FLAG_IDLE;
    gavl_value_free(&val);
    }
  
  bg_cmdline_remove_arg(argc, argv, arg);
  }

static void cmd_get(void * data, int * argc, char *** argv, int arg)
  {
  const gavl_value_t * val;
  const gavl_dictionary_t * control;

  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -get requires an argument");
    exit(-1);
    }

  if(flags & FLAG_REMOTE)
    {
    /* TODO */
    char * ctx;
    gavl_buffer_t buf;
    
    // gmerlin-control -addr example.host:8886 -set /path/var=1 -delay 10
    // -> http://example.host:8886/set/path/var?v=1&delay=10
    
    char * uri;

    gavl_buffer_init(&buf);
    
    ctx = gavl_strdup((*argv)[arg]);
    
    uri = gavl_sprintf("http%s/get%s", strstr(addr, "://"), ctx);
    
    
    if(!bg_http_get(uri, &buf, NULL))
      {
      /* Error */
      goto fail;
      }

    printf("%s\n", (const char*)buf.buf);
    
    fail:
    
    gavl_buffer_free(&buf);
    free(uri);
    
    }
  else
    {
    if(!(control = gavl_control_get(&controls, "/")) ||
       !(control = gavl_control_get(&controls, (*argv)[arg])) ||
       !(val = gavl_dictionary_get(control, GAVL_CONTROL_VALUE)))
      {
      if(!control)
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No such control %s", (*argv)[arg]);
      exit(-1);
      }
    gavl_value_dump(val, 0);
    gavl_dprintf("\n");
    }
  
  bg_cmdline_remove_arg(argc, argv, arg);

  }

bg_cmdline_arg_t commands[] =
  {
    {
      .arg =         "-set",
      .help_arg =    "name=value",
      .help_string = TRS("Set a variable"),
      .callback =    cmd_set,
    },
    {
      .arg =         "-set-rel",
      .help_arg =    "name=value",
      .help_string = TRS("Add to a value"),
      .callback =    cmd_setrel,
    },
    {
      .arg =         "-get",
      .help_string = TRS("Get a variable"),
      .callback =    cmd_get,
    },
    { /* End of options */ }
  };

static void opt_addr(void * data, int * argc, char *** argv, int arg)
  {
  
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -addr requires an argument");
    exit(-1);
    }
  addr = gavl_strrep(addr, (*argv)[arg]);
  bg_cmdline_remove_arg(argc, argv, arg);
  }

static void opt_dump_controls(void * data, int * argc, char *** argv, int arg)
  {
  flags |= FLAG_DUMP_CONTROLS;
  }

static void opt_delay(void * data, int * argc, char *** argv, int arg)
  {
  
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -delay requires an argument");
    exit(-1);
    }
  delay = atoi((*argv)[arg]);
  bg_cmdline_remove_arg(argc, argv, arg);
  }

static void opt_monitor(void * data, int * argc, char *** argv, int arg)
  {
  flags |= FLAG_MONITOR;
  }

static bg_cmdline_arg_t options[] =
  {
    {
      .arg =         "-addr",
      .help_arg =    "<addr>",
      .help_string = TRS("Address to connect to."),
      .callback =    opt_addr,
    },
    {
      .arg =         "-delay",
      .help_arg =    "delay",
      .help_string = TRS("Seconds to wait before action is taken (not supported by all backends)"),
      .callback =    opt_delay,
    },
    {
      .arg =         "-M",
      .help_string = TRS("Monitor events"),
      .callback =    opt_monitor,
    },
    {
      .arg =         "-dc",
      .help_string = TRS("Dump controls"),
      .callback =    opt_dump_controls,
    },
    { /* End of options */ }
  };

#if 0
static void opt_help(void * data, int * argc, char *** argv, int arg)
  {
  FILE * out = stderr;
  
  fprintf(out, "Usage: %s [options] command\n\n", (*argv)[0]);
  fprintf(out, "Options:\n\n");
  bg_cmdline_print_help(global_options);
  fprintf(out, "\ncommand is of the following:\n\n");
  bg_cmdline_print_help(commands);
  exit(0);
  }
#endif

const bg_cmdline_app_data_t app_data =
  {
    .package =  PACKAGE,
    .version =  VERSION,
    .synopsis = TRS("[options] command\n"),
    .help_before = TRS("Control stuff from the command line\n"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Options"), options },
                                         { TRS("Commands"), commands },
                                       {  } },
    
  };

static int handle_msg(void * priv, gavl_msg_t * msg)
  {
  if(flags & FLAG_MONITOR)
    {
    gavl_dprintf("Got message\n");
    gavl_msg_dump(msg, 2);
    }
  switch(msg->NS)
    {
    case BG_MSG_NS_STATE:
      switch(msg->ID)
        {
        case BG_MSG_STATE_CHANGED:
          {
          gavl_dictionary_t * ctrl;
          gavl_value_t val;
          const char * ctx;
          const char * var;
          int last = 0;
          
          gavl_value_init(&val);
          
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          if((ctrl = gavl_control_get_create(&controls, ctx)) &&
             (ctrl = gavl_control_get_create(&controls, var)))
            gavl_dictionary_set_nocopy(ctrl, GAVL_CONTROL_VALUE, &val);
          
          gavl_value_free(&val);
          
          if(last)
            flags |= FLAG_HAVE_STATE;
          }
          break;
        }
      break;
    case GAVL_MSG_NS_CONTROL:
      switch(msg->ID)
        {
        case GAVL_MSG_CONTROL_CHANGED:
          break; 
        case GAVL_MSG_CONTROL_OPTION_ADDED:
          break; 
        case GAVL_MSG_CONTROL_OPTION_REMOVED:
          break; 
        case GAVL_MSG_CONTROL_IDLE:
          flags |= FLAG_IDLE;
          break; 
        }
      break;
    }
  return 1;
  }


int main(int argc, char ** argv)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 50;
  const bg_plugin_info_t * info;
  bg_plugin_handle_t * h = NULL;
  bg_control_plugin_t * c;
  char * protocol = NULL;
  int ret;
  bg_controllable_t * controllable = NULL;

  gavl_dictionary_init(&controls);
  
  bg_plugins_init();

  bg_app_init("gmerlin-control", TRS("Control stuff from the command line"), NULL);
  
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  bg_handle_sigint();
  
  bg_cmdline_init(&app_data);
  
  if(argc < 2)
    bg_cmdline_print_help(argv[0], 0);

  bg_cmdline_init(&app_data);

  bg_cmdline_parse(options, &argc, &argv, NULL);
  
  /* Create handle */

  if(gavl_string_starts_with(addr, "remote://"))
    {
    flags |= FLAG_REMOTE;
    }
  else
    {
    gavl_url_split(addr, &protocol, NULL, NULL, NULL, NULL, NULL);
  
    if(!(info = bg_plugin_find_by_protocol(protocol, BG_PLUGIN_CONTROL)))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Plugin for protocol %s not found", protocol);
      return EXIT_FAILURE;
      }
    if(!(h = bg_plugin_load(info)))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Couldn't load plugin %s", info->name);
      return EXIT_FAILURE;
      }
  
    c = (bg_control_plugin_t *)h->plugin;

    if((h->plugin->get_controllable) &&
       (controllable = h->plugin->get_controllable(h->priv)))
      {
      bg_control_init(&ctrl, bg_msg_sink_create(handle_msg, NULL, 1));
      bg_controllable_connect(controllable, &ctrl);
      }

    if(c->open && !c->open(h->priv, addr))
      return EXIT_FAILURE;

    if(c->get_controls)
      {
      c->get_controls(h->priv, &controls);

      if(flags & FLAG_DUMP_CONTROLS)
        {
        gavl_dprintf("Got contols\n");
        gavl_dictionary_dump(&controls, 2);
        gavl_dprintf("\n");
        }
    
      }

    }
  
  /* run commands */

  if(flags & FLAG_REMOTE)
    {
    bg_cmdline_parse(commands, &argc, &argv, NULL);
    }
  else
    {
    while(1)
      {
      if(bg_got_sigint())
        break;
    
      ret = 0;
    
      //      ret += bg_backend_handle_ping(backend);

      if(c->update)
        ret += c->update(h->priv);
    
      bg_msg_sink_iteration(ctrl.evt_sink);
      ret += bg_msg_sink_get_num(ctrl.evt_sink);

      if((flags & FLAG_HAVE_STATE) && !(flags & FLAG_COMMANDS_SENT))
        {
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got state");
        bg_cmdline_parse(commands, &argc, &argv, NULL);
        flags |= FLAG_COMMANDS_SENT;
        }
    
      if((flags & FLAG_IDLE) && !(flags & FLAG_MONITOR))
        break;

        
      //      ret += 
    
      if(!ret)
        gavl_time_delay(&delay_time);
      }
    
    }
  
  
  //  fail:
  
  if(h)
    bg_plugin_unref(h);


  return 0;
  }
