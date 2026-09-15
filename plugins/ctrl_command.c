
#include <string.h>
#include <signal.h>

#include <config.h>

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/subprocess.h>
#include <gmerlin/state.h>

#include <gavl/log.h>
#define LOG_DOMAIN "command"
#include <gavl/utils.h>
#include <gavl/state.h>

#include <control.h>


typedef struct
  {
  char * cmd;
  bg_controllable_t ctrl;
  bg_subprocess_t * proc;
  
  int service;
  } command_t;

static int handle_msg(void * priv, gavl_msg_t * msg)
  {
  command_t * s = priv;
  switch(msg->NS)
    {
    case GAVL_MSG_NS_CONTROL:
      switch(msg->ID)
        {
        case GAVL_CMD_CONTROL_PUSH_BUTTON:
          if(s->proc)
            {
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Command already running");
            return 1;
            }
          //   gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Starting process %s", s->cmd);
          s->proc = bg_subprocess_create(s->cmd, 0, 0, 0);
          break;
        }
      break;
    case BG_MSG_NS_STATE:
      switch(msg->ID)
        {
        case BG_CMD_SET_STATE:
          {
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

          if(!strcmp(var, "on"))
            {
            int on = 0;
            gavl_value_get_int(&val, &on);

            // fprintf(stderr, "set_switch: %d\n", on);

            if(s->proc && on)
              {
              gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Service already running");
              return 1;
              }
            if(!s->proc && !on)
              {
              gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Service isn't running");
              return 1;
              }

            if(!s->proc) // Switch on
              {
              s->proc = bg_subprocess_create(s->cmd, 0, 0, 0);
              bg_state_set(NULL, 1, NULL, "on", 
                           &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
              }
            else // Switch off: We kill the process asynchronously
              {
              bg_subprocess_kill(s->proc, SIGTERM);
              }
            
            }

          gavl_value_free(&val);
          break;
          }
        }
      break;



    }
  return 1;
  }

static void * create_command()
  {
  command_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(handle_msg, s, 1),
                       bg_msg_hub_create(1));
  
  return s;
  }

static void destroy_command(void *priv)
  {
  command_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  free(s);
  }

static bg_controllable_t * get_controllable_command(void * priv)
  {
  command_t * s = priv;
  return &s->ctrl;
  }

static int update_command(void * priv)
  {
  command_t * s = priv;
  if(s->proc && bg_subprocess_done(s->proc))
    {
    bg_subprocess_close(s->proc);
    s->proc = NULL;

    if(s->service)
      {
      gavl_value_t val;
      gavl_value_init(&val);
      gavl_value_set_int(&val, 0);

      bg_state_set(NULL, 1, NULL, "on", 
                   &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      
      gavl_value_free(&val);
      };
    
    return 1;
    }
  return 0;
  }

static int open_command(void * priv, const char * uri)
  {
  const char * pos;
  command_t * s = priv;

  if(gavl_string_starts_with(uri, "service://"))
    s->service = 1;
  
  if((pos = strstr(uri, "://")))
    s->cmd = gavl_strdup(pos+3);
  else
    s->cmd = gavl_strdup(uri);
  
  return 1;
  }

static void get_controls_command(void * priv, gavl_dictionary_t * parent)
  {
  command_t * s = priv;

  if(s->service)
    {
    gavl_control_add_control(parent,
                             GAVL_META_CLASS_CONTROL_POWERBUTTON,
                             "on",
                             gavl_dictionary_get_string(parent, GAVL_META_LABEL));
    }
  else
    {
    gavl_control_add_control(parent,
                             GAVL_META_CLASS_CONTROL_BUTTON,
                             "run",
                             gavl_dictionary_get_string(parent, GAVL_META_LABEL));
    }

  
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_command",
    .long_name = TRS("Run commands"),
    .description = TRS("Run commands"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_command,
    .destroy =   destroy_command,
    .get_controllable =   get_controllable_command,
    .priority =         1,
    },
  
  .protocols = "command service",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_command,
  .open   = open_command,
  .get_controls   = get_controls_command,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
