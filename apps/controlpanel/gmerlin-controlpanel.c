#include <locale.h>
#include <string.h>
#include <signal.h>


#include <gavl/gavl.h>
#include <gavl/gavf.h>
#include <gavl/http.h>

#include <config.h>

#include <gmerlin/translation.h>

#include <gmerlin/log.h>
#define LOG_DOMAIN "controlpanel"

#include <gmerlin/parameter.h>
#include <gmerlin/cfgctx.h>
#include <gmerlin/cfg_registry.h>

#include <gmerlin/cmdline.h>

#include <gmerlin/httpserver.h>
#include <gmerlin/utils.h>

#include <gmerlin/bgmsg.h>

#include <gmerlin/state.h>

#include <gmerlin/websocket.h>
#include <gmerlin/subprocess.h>

/*
 *  Control
 *  Label:     TV
 *  Type:      onoff, slider, button, meter
 *  CmdOn: cmd
 *  CmdOff: cmd
 *  CmdStatus: cmd
 *  CmdSet: cmd
 *  EndControl
 */

#define META_TYPE           "Type"
#define META_COMMAND_ON     "CmdOn"
#define META_COMMAND_OFF    "CmdOff"

#define META_COMMAND_UP     "CmdUp"
#define META_COMMAND_DOWN   "CmdDown"

/* Command must print the current value on stdout */
#define META_COMMAND_STATUS "CmdStatus"

/* Set value. Followed by a floating point argument */
/* Command *must* print the updated value on stdout */
#define META_COMMAND_SET    "CmdSet"

/* Generic command (e.g. button) */
#define META_COMMAND        "Cmd"

#define META_LABEL          "Label"
#define META_MIN            "Min"
#define META_MAX            "Max"
#define META_LOW            "Low"
#define META_HIGH           "High"
#define META_OPTIMUM        "Optimum"
#define META_UNIT           "Unit"
#define META_VALUE          "Value"
#define META_ITEMS          "Items"
#define META_NAME           "Name"

#define TYPE_SLIDER       1
#define TYPE_POWER        2
#define TYPE_VOLUME       3
#define TYPE_BUTTON       4
#define TYPE_RADIOBUTTONS 5

/* Read only types */
#define TYPE_METER        100

static const struct
  {
  int i;
  const char * s;
  }
types[] =
  {
   { TYPE_SLIDER,       "slider" },
   { TYPE_POWER,        "power" },
   { TYPE_VOLUME,       "volume" },
   { TYPE_BUTTON,       "button" },
   { TYPE_RADIOBUTTONS, "radiobuttons" },
   { TYPE_METER,        "meter" },
   { /* End */ }
  };

static int type_by_name(const char * name)
  {
  int idx = 0;

  while(types[idx].s)
    {
    if(!strcmp(name, types[idx].s))
      return types[idx].i;
    idx++;
    }
  return 0;
  }
  
typedef struct
  {
  gavl_dictionary_t dict;
  
  int type;

  gavl_value_t val;

  gavl_value_t val_min;
  gavl_value_t val_max;

  gavl_time_t next_update_time;
  gavl_time_t update_interval;
  
  } control_t;

static int num_controls = 0;
static int controls_alloc = 0;

static control_t * controls = NULL;

static char * label = "Controlpanel";
static char * addr = "127.0.0.1";
static int port = 0;

static gavl_dictionary_t state;

static bg_controllable_t ctrl;

gavl_timer_t * timer = NULL;

static control_t * control_by_name(const char * name)
  {
  int i;
  const char * str;
  
  for(i = 0; i < num_controls; i++)
    {
    if((str = gavl_dictionary_get_string(&controls[i].dict, 
                                          META_NAME)) &&
       !strcmp(str, name))
      return controls + i;
    }
  return NULL;
  }

static int get_string_command(const char * command, gavl_value_t * ret)
  {
  int result = 0;
  char * line = NULL;
  int line_alloc = 0;
  
  bg_subprocess_t * s;
  
  if(!(s = bg_subprocess_create(command, 0, 1, 0)))
    return 0;
  
  bg_subprocess_read_line(s->stdout_fd, &line, &line_alloc, -1);

  if(!bg_subprocess_close(s) && line)
    result = 1;

  if(result)
    gavl_value_set_string(ret, line);

  if(line)
    free(line);
  return result;
  
  }

static int get_int_command(const char * command, gavl_value_t * ret)
  {
  int result = 0;
  char * line = NULL;
  int line_alloc = 0;
  
  bg_subprocess_t * s;
  
  if(!(s = bg_subprocess_create(command, 0, 1, 0)))
    return 0;
  
  bg_subprocess_read_line(s->stdout_fd, &line, &line_alloc, -1);

  if(!bg_subprocess_close(s) && line)
    result = 1;

  /* fprintf(stderr, "get_int_command %s\n", command); */
  /* fprintf(stderr, "got line: %s\n", line); */
  /* fprintf(stderr, "result: %d\n", result); */
  
  if(result)
    gavl_value_set_int(ret, atoi(line));

  if(line)
    free(line);
  return result;
  
  }

static int get_float_command(const char * command, gavl_value_t * ret)
  {
  int result = 0;
  char * line = NULL;
  int line_alloc = 0;
  
  bg_subprocess_t * s;
  
  if(!(s = bg_subprocess_create(command, 0, 1, 0)))
    return 0;
  
  bg_subprocess_read_line(s->stdout_fd, &line, &line_alloc, -1);

  if(!bg_subprocess_close(s) && line)
    result = 1;
  
  if(result)
    {
    gavl_value_set_float(ret, strtod(line, NULL));
    }

  if(line)
    free(line);
 
  return result;
  }

static void run_command(const char * command)
  {
  bg_system(command);
  }


static void load_controls(const char * filename)
  {
  char * line = NULL;
  int line_alloc = 0;
  
  FILE * in;
  gavl_io_t * io;
  control_t * cur = NULL;

  gavl_dictionary_t * item = NULL;
  
  if(!(in = fopen(filename, "r")))
    return;

  io = gavl_io_create_file(in, 0, 1, 1);

  while(gavl_io_read_line(io, &line, &line_alloc, 1024))
    {
    line = gavl_strtrim(line);
    
    if(*line == '#')
      continue;
    
    if(!strcmp(line, "Control"))
      {
      if(num_controls == controls_alloc)
        {
        controls_alloc += 32;
        controls = realloc(controls, controls_alloc*sizeof(*controls));
        }
      cur = controls + num_controls;
      }
    else if(!strcmp(line, "EndControl"))
      {
      if(!gavl_dictionary_get(&cur->dict, META_NAME))
        gavl_dictionary_set_string_nocopy(&cur->dict, META_NAME, bg_sprintf("control_%d", num_controls+1));
      
      //      fprintf(stderr, "Loaded control:\n");
      //      gavl_dictionary_dump(&cur->dict, 2);

      cur = NULL;
      num_controls++;
      }
    else if(!strcmp(line, "Item"))
      {
      gavl_array_t * items;
      gavl_value_t val;

      if(!cur)
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Item must appear between Control and EndControl");
        return;
        }
      gavl_value_init(&val);
      item = gavl_value_set_dictionary(&val);

      items = gavl_dictionary_get_array_create(&cur->dict, META_ITEMS);
      gavl_array_splice_val_nocopy(items, -1, 0, &val);
      }
    else if(!strcmp(line, "EndItem"))
      {
      item = NULL;
      }
    else
      {
      if(!cur)
        continue;
      
      if(item)
        gavl_http_parse_vars_line(item, line);
      else
        gavl_http_parse_vars_line(&cur->dict, line);
      }
    
    }
    
  gavl_io_destroy(io);

  
  }

static void opt_c(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Option -c requires an argument");
    exit(-1);
    }

  load_controls((*_argv)[arg]);
  
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
    .help_before = TRS("Remote control panel"),
    .args = (bg_cmdline_arg_array_t[]) { { TRS("Global options"), global_options },
                                         {  } },
  };

/* Manifest */


static const char * manifest_file =
  "{\n"
    "\"short_name\": \"%s\",\n"
    "\"name\": \"%s\",\n"
    "\"display\": \"standalone\",\n"
    "\"icons\": [ \n"
      "{\n"
      "\"src\": \"static/icons/remote_16.png\",\n"
      "\"sizes\": \"16x16\",\n"
      "\"type\": \"image/png\",\n"
      "\"density\": 1.0\n"
      "},\n"
      "{\n"
      "\"src\": \"static/icons/remote_48.png\",\n"
      "\"sizes\": \"48x48\",\n"
      "\"type\": \"image/png\",\n"
      "\"density\": 1.0\n"
      "},\n"
      "{\n"
      "\"src\": \"static/icons/remote_96.png\",\n"
      "\"sizes\": \"96x96\",\n"
      "\"type\": \"image/png\",\n"
      "\"density\": 1.0\n"
      "} ],\n"
  "\"start_url\": \"%s\"\n"
  "}\n";

static int server_handle_manifest(bg_http_connection_t * conn, void * data)
  {
  const char * var;
  int result = 0;

  char * start_url = NULL;
  
  char * protocol = NULL;
  char * host = NULL;
  int port = 0;
  char * m = NULL;
  int len = 0;

  
  if(strcmp(conn->path, "/manifest.json"))
    return 0; // 404
  
  if(!(var = gavl_dictionary_get_string(&conn->req, "Referer")))
    goto fail;
  
  bg_http_connection_check_keepalive(conn);
      
  //      fprintf(stderr, "Referer: %s\n", var);
  gavl_url_split(var, &protocol, NULL, NULL, &host, &port, NULL);

  start_url = bg_sprintf("%s://%s:%d", protocol, host, port);

  m = bg_sprintf(manifest_file, label, label, start_url);
  len = strlen(m);
      
  bg_http_connection_init_res(conn, conn->protocol, 200, "OK");
  gavl_dictionary_set_string(&conn->res, "Content-Type", "application/manifest+json");
  gavl_dictionary_set_long(&conn->res, "Content-Length", len);
      
  if(!bg_http_connection_write_res(conn))
    {
    bg_http_connection_clear_keepalive(conn);
    goto cleanup;
    }
  result = 1;
  
  if(result && !gavl_socket_write_data(conn->fd, (const uint8_t*)m, len))
    bg_http_connection_clear_keepalive(conn);
  
  fail:

  if(!result)
    {
    bg_http_connection_init_res(conn, conn->protocol, 400, "Bad Request");
    if(!bg_http_connection_write_res(conn))
      bg_http_connection_clear_keepalive(conn);
    }
  
  cleanup:
  
  if(protocol)
    free(protocol);
  if(host)
    free(host);
  
  //      fprintf(stderr, "Got manifest:\n%s\n", m);
  
  if(m)
    free(m);
  
  return 1;
  }

/*
 *  Set a value:
 *  http://host:port/command?c=set&var=variable_name&val=1
 *
 *  Set a value relative:
 *  http://host:port/command?c=setrel&var=variable_name&val=1
 *
 *  Return value (as text/plain)
 *  http://host:port/command?c=get&var=variable_name
 *  
 */

static int get_arg_value(const char * var, const char * val, gavl_value_t * ret)
  {
  control_t * c;

  gavl_value_init(ret);

  if(!(c = control_by_name(var)))
    return 0;
  
  switch(c->type)
    {
    case TYPE_SLIDER:
      if(!val)
        return 0;
      break;
    case TYPE_VOLUME:
      if(!val)
        return 0;
      break;
    case TYPE_METER:
      if(!val)
        return 0;
      break;
    case TYPE_POWER:
      if(!val)
        return 0;
      break;
    case TYPE_BUTTON:
      /* We don't even care if the value is given or not */
      return 1;
      break;
    case TYPE_RADIOBUTTONS:
      if(!val)
        return 0;
      break;
    }
  return 1;
  }

static int handle_command_uri(bg_http_connection_t * conn, void * data)
  {
  char * answer = NULL;
  const char * var;
  const char * val_str;
  const char * cmd;
  
  if(!(cmd = gavl_dictionary_get_string(&conn->url_vars, "c")))
    return 0;
  
  if(!strcmp(cmd, "set"))
    {
    gavl_msg_t * msg;
    gavl_value_t val;
    control_t * c;
    
    if(!(var = gavl_dictionary_get_string(&conn->url_vars, "var")))
      return 0;
    
    if(!(c = control_by_name(var)))
      return 0;
    
    gavl_value_init(&val);

    if(c->type != TYPE_BUTTON)
      {
      val_str = gavl_dictionary_get_string(&conn->url_vars, "val");
      if(!get_arg_value(var, val_str, &val))
        return 0;
      } 
    
    msg = bg_msg_sink_get(ctrl.cmd_sink);
    gavl_msg_set_state(msg, BG_CMD_SET_STATE, 1, var, META_VALUE, &val);
    bg_msg_sink_put(ctrl.cmd_sink);
    
    bg_http_connection_init_res(conn, conn->protocol, 200, "OK");

    }
  else if(!strcmp(cmd, "setrel"))
    {
    gavl_msg_t * msg;
    gavl_value_t val;

    if(!(var = gavl_dictionary_get_string(&conn->url_vars, "var")))
      return 0;

    val_str = gavl_dictionary_get_string(&conn->url_vars, "val");

    if(!get_arg_value(var, val_str, &val))
      {
      return 0;
      }

    msg = bg_msg_sink_get(ctrl.cmd_sink);
    gavl_msg_set_state(msg, BG_CMD_SET_STATE_REL, 1, var, META_VALUE, &val);
    bg_msg_sink_put(ctrl.cmd_sink);

    
    bg_http_connection_init_res(conn, conn->protocol, 200, "OK");
    
    }
  else if(!strcmp(cmd, "get"))
    {
    char * val_str = NULL;
    const gavl_value_t * val;
    
    control_t * c;
    
    if(!(var = gavl_dictionary_get_string(&conn->url_vars, "var")))
      return 0;

    if(!(c = control_by_name(var)))
      return 0;

    switch(c->type)
      {
      case TYPE_SLIDER:
        if(!(val = bg_state_get(&state, var, META_VALUE)) ||
           !(val_str = gavl_value_to_string(val)))
          return 0;
        break;
      case TYPE_VOLUME:
        if(!(val = bg_state_get(&state, var, META_VALUE)) ||
           !(val_str = gavl_value_to_string(val)))
          return 0;
        break;
      case TYPE_POWER:
        if(!(val = bg_state_get(&state, var, META_VALUE)) ||
           !(val_str = gavl_value_to_string(val)))
          return 0;
        break;
      case TYPE_METER:
        if(!(val = bg_state_get(&state, var, META_VALUE)) ||
           !(val_str = gavl_value_to_string(val)))
          return 0;
        break;
      case TYPE_BUTTON:
        return 0;
        break;
      case TYPE_RADIOBUTTONS:
        if(!(val = bg_state_get(&state, var, META_VALUE)) ||
           !(val_str = gavl_value_to_string(val)))
          return 0;
        break;
      }
    
    bg_http_connection_init_res(conn, conn->protocol, 200, "OK");

    if(val_str)
      {
      answer = bg_sprintf("%s\n", val_str);
    
      gavl_dictionary_set_string(&conn->res, "Content-Type", "text/plain");
      gavl_dictionary_set_string_nocopy(&conn->res, "Content-Length", bg_sprintf("%d", (int)strlen(answer)));
      }
    }

  if(!bg_http_connection_write_res(conn))
    {
    bg_http_connection_clear_keepalive(conn);
    }
  else if(answer)
    {
    if(!gavl_socket_write_data(conn->fd, (uint8_t*)answer, strlen(answer)))
      bg_http_connection_clear_keepalive(conn);
    }
  
  return 1;
  }

static int update_control(int idx)
  {
  gavl_value_t val;
  const char * cmd;
  
  control_t * c = &controls[idx];

  if(!(cmd = gavl_dictionary_get_string(&c->dict, META_COMMAND_STATUS)))
    return 0;

  gavl_value_init(&val);
  
  
  switch(c->type)
    {
    case TYPE_POWER:
      if(!get_int_command(cmd, &val))
        return 0;
      break;
    case TYPE_SLIDER:
    case TYPE_VOLUME:
    case TYPE_METER:
      if(!get_float_command(cmd, &val))
        return 0;
      break;
    case TYPE_RADIOBUTTONS:
      if(!get_string_command(cmd, &val))
        return 0;
      break;
    default:
      return 0;
    }
  
  if(gavl_value_compare(&c->val, &val))
    {
    gavl_value_copy(&c->val, &val);
    bg_state_set(&state, 1,
                 gavl_dictionary_get_string(&c->dict, META_NAME),
                 META_VALUE, &val, ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    return 1;
    }
  gavl_value_free(&val);
  return 0;
  }

static void init_state()
  {
  const gavl_value_t * val;
  const char * val_str;
  int i;
  const char * ctrl_name;

  gavl_dictionary_init(&state);

  for(i = 0; i < num_controls; i++)
    {
    controls[i].update_interval = 10 * GAVL_TIME_SCALE;
    controls[i].next_update_time = (i+1) * GAVL_TIME_SCALE;
    
    if((val = gavl_dictionary_get(&controls[i].dict, META_TYPE)))
      {
      val_str = gavl_value_get_string(val);

      controls[i].type = type_by_name(val_str);

      ctrl_name = gavl_dictionary_get_string(&controls[i].dict, META_NAME);
      
      bg_state_set(&state, 0, ctrl_name, META_TYPE, val, NULL, 0);
      
      switch(controls[i].type)
        {
        case TYPE_POWER:
          {
          }
          break;
        case TYPE_VOLUME:
          {
          gavl_value_t val_f;
          if((val = gavl_dictionary_get(&controls[i].dict, META_MIN)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MIN, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_MAX)))
            {
            val_str = gavl_value_get_string(val);
            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MAX, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          }
          break;
        case TYPE_METER:
          {
          gavl_value_t val_f;
          if((val = gavl_dictionary_get(&controls[i].dict, META_UNIT)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_string(&val_f, val_str);
            
            bg_state_set(&state, 0, ctrl_name, META_UNIT, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_MIN)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MIN, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_MAX)))
            {
            val_str = gavl_value_get_string(val);
            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MAX, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_LOW)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_LOW, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_HIGH)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_HIGH, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_OPTIMUM)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_OPTIMUM, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          controls[i].update_interval = 1 * GAVL_TIME_SCALE;
          }
          break;
        case TYPE_SLIDER:
          {
          gavl_value_t val_f;
          
          if((val = gavl_dictionary_get(&controls[i].dict, META_MIN)))
            {
            val_str = gavl_value_get_string(val);

            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MIN, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          if((val = gavl_dictionary_get(&controls[i].dict, META_MAX)))
            {
            val_str = gavl_value_get_string(val);
            gavl_value_init(&val_f);
            gavl_value_set_float(&val_f, strtod(val_str, NULL));

            bg_state_set(&state, 0, ctrl_name, META_MAX, &val_f, NULL, 0);
            gavl_value_free(&val_f);
            }
          
          break;
          }
        case TYPE_RADIOBUTTONS:
          {
          val = gavl_dictionary_get(&controls[i].dict, META_ITEMS);
          bg_state_set(&state, 0, ctrl_name, META_ITEMS, val, NULL, 0);
          }
        }

      if((val = gavl_dictionary_get(&controls[i].dict, META_LABEL)))
        {
        bg_state_set(&state, 0, ctrl_name, META_LABEL, val, NULL, 0);
        }

      if((val = gavl_dictionary_get(&controls[i].dict, META_NAME)))
        bg_state_set(&state, 0, ctrl_name, META_NAME, val, NULL, 0);
     
      }
    }
  }


static int handle_msg(void * data, gavl_msg_t * msg)
  {
  switch(msg->NS)
    {
    case BG_MSG_NS_STATE:

      switch(msg->ID)
        {
        case BG_CMD_SET_STATE:
        case BG_CMD_SET_STATE_REL:
          {
          
          gavl_value_t val;
          int last = 0;
          const char * ctx = NULL;
          const char * var = NULL;
          control_t * c;

          gavl_value_t val_new;

          gavl_value_init(&val);
          
          gavl_msg_get_state(msg, &last, &ctx, &var, &val, NULL);

          //          fprintf(stderr, "set state %p %s %s", c, var, ctx);
          
          if(strcmp(var, META_VALUE))
            {
            gavl_value_free(&val);
            break;
            }

          c = control_by_name(ctx);
          
          //          fprintf(stderr, "set state %p\n", c);

          gavl_value_init(&val_new);

          switch(c->type)
            {
            case TYPE_SLIDER:
              {
              char * command;
              const char * cmd_prefix;
              double f;
              //              fprintf(stderr, "set state %p\n", c);

              if(msg->ID == BG_CMD_SET_STATE)
                {

                gavl_value_get_float(&val, &f);
              
                fprintf(stderr, "set state slider %f\n", f);

                cmd_prefix = gavl_dictionary_get_string(&c->dict, META_COMMAND_SET);
                command = bg_sprintf("%s %.1f", cmd_prefix, f);
              
                gavl_value_init(&val_new);
                get_float_command(command, &val_new);
                bg_state_set(&state, 1, ctx, META_VALUE, &val, ctrl.evt_sink, BG_MSG_STATE_CHANGED);

#if 0
                gavl_value_get_float(&val_new, &f);
                fprintf(stderr, "New Value: %f\n", f);
#endif

                
                }

              }
              break;
            case TYPE_POWER:
              {
              int i;
              const char * command;

              if(msg->ID == BG_CMD_SET_STATE)
                {
                gavl_value_get_int(&val, &i);
                //                fprintf(stderr, "set state power %d\n", i);
              
                if(i)
                  command = gavl_dictionary_get_string(&c->dict, META_COMMAND_ON);
                else
                  command = gavl_dictionary_get_string(&c->dict, META_COMMAND_OFF);

              
                gavl_value_init(&val_new);
                get_int_command(command, &val_new);
                bg_state_set(&state, 1, ctx, META_VALUE, &val_new, ctrl.evt_sink, BG_MSG_STATE_CHANGED);

#if 0
                gavl_value_get_int(&val_new, &i);
                fprintf(stderr, "New Value: %d\n", i);
#endif
                
                }
              
              
              }
              break;
            case TYPE_VOLUME:
              {

              if(msg->ID == BG_CMD_SET_STATE_REL)
                {
                int i;
                const char * command;

                gavl_value_get_int(&val, &i);


                
                if(i > 0)
                  {
                  command = gavl_dictionary_get_string(&c->dict, META_COMMAND_UP);
                  }
                else
                  {
                  command = gavl_dictionary_get_string(&c->dict, META_COMMAND_DOWN);
                  }

                gavl_value_init(&val_new);
                get_float_command(command, &val_new);

                //                fprintf(stderr, "*** Volume changed: %f\n", val_new.v.d);

                bg_state_set(&state, 1, ctx, META_VALUE, &val_new, ctrl.evt_sink, BG_MSG_STATE_CHANGED);

                /*
                gavl_value_copy(&c->val, &val);
                bg_state_set(&state, 1,
                             gavl_dictionary_get_string(&c->dict, META_NAME),
                             META_VALUE, &val, ctrl.evt_sink, BG_MSG_STATE_CHANGED);
                */
                

                }
              break;
              }
            case TYPE_BUTTON:
              {
              const char * command;
              command = gavl_dictionary_get_string(&c->dict, META_COMMAND);
              run_command(command);
              break;
              }
            case TYPE_RADIOBUTTONS:
              {
              if(msg->ID == BG_CMD_SET_STATE)
                {
                char * command;
                
                command = bg_sprintf("%s %s",
                                     gavl_dictionary_get_string(&c->dict, META_COMMAND_SET),
                                     gavl_value_get_string(&val));
                run_command(command);
                free(command);
                }
              }
              break;
              
            }
          }
          break;
        }
      break;
    }
  return 1;
  }

static int update_controls(gavl_timer_t * timer)
  {
  int ret = 0;
  int i;
  gavl_time_t t;
  
  for(i = 0; i < num_controls; i++)
    {
    t = gavl_timer_get(timer);
    
    if(controls[i].next_update_time <= t)
      {
      ret += update_control(i);
      controls[i].next_update_time = gavl_timer_get(timer) + controls[i].update_interval;
      }
    }
  
  return ret;
  }

int main(int argc, char ** argv)
  {
  gavl_time_t delay_time = GAVL_TIME_SCALE / 50; // 20 ms
  gavl_value_t val;
  bg_http_server_t * srv;
  bg_websocket_context_t * ws;

  timer = gavl_timer_create();

  gavl_timer_start(timer);
  
  setlocale(LC_ALL, "");
  setlocale(LC_NUMERIC, "C");

  bg_handle_sigint();
  signal(SIGPIPE, SIG_IGN);
  
  bg_cmdline_init(&app_data);

  if(argc < 2)
    bg_cmdline_print_help(argv[0], 0);

  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);
  
  
  srv = bg_http_server_create();

  bg_http_server_set_default_port(srv, port);

  gavl_value_init(&val);
  gavl_value_set_string_nocopy(&val, addr);
  bg_http_server_set_parameter(srv, "addr", &val);

  /* Don't free here! */
  gavl_value_init(&val);
  gavl_value_set_int(&val, port);
  bg_http_server_set_parameter(srv, "port", &val);

  /* Must be first */
  bg_http_server_set_root_file(srv, "/static2/controlpanel.html");

  bg_http_server_set_static_path(srv, "/static");
  bg_http_server_add_static_path(srv, "/static2", DATA_DIR "/web");
  
  bg_http_server_add_handler(srv, server_handle_manifest, BG_HTTP_PROTO_HTTP, NULL, NULL);

  bg_http_server_add_handler(srv, handle_command_uri, BG_HTTP_PROTO_HTTP, "/command", NULL);
  
  bg_controllable_init(&ctrl,
                       bg_msg_sink_create(handle_msg, NULL, 1),
                       bg_msg_hub_create(1));

  
  ws = bg_websocket_context_create(GAVL_META_CLASS_BACKEND_CONTROLPANEL, NULL, &ctrl);
  
  init_state();

  /* Apply initial state */
  bg_state_apply(&state, ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  
  bg_http_server_start(srv);
  
  while(1)
    {
    
    if(!bg_http_server_iteration(srv) &&
       !bg_websocket_context_iteration(ws) &&
       !update_controls(timer))
      gavl_time_delay(&delay_time);
    
    if(bg_got_sigint())
      {
      fprintf(stderr, "Got sigint");
      break;
      }
    }
  
  bg_http_server_destroy(srv);
  
  bg_websocket_context_destroy(ws);

  gavl_timer_destroy(timer);

  
  }
