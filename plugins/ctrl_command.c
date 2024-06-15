
#include <string.h>

#include <config.h>

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/subprocess.h>

#include <gavl/log.h>
#define LOG_DOMAIN "command"
#include <gavl/utils.h>

#include <control.h>


typedef struct
  {
  char * cmd;
  bg_controllable_t ctrl;
  bg_subprocess_t * proc;
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
    return 1;
    }
  return 0;
  }

static int open_command(void * priv, const char * uri)
  {
  const char * pos;
  command_t * s = priv;

  if((pos = strstr(uri, "://")))
    s->cmd = gavl_strdup(pos+3);
  else
    s->cmd = gavl_strdup(uri);
  
  return 1;
  }

static void get_controls_command(void * priv, gavl_dictionary_t * parent)
  {
  gavl_control_add_control(parent,
                           GAVL_META_CLASS_CONTROL_BUTTON,
                           "run",
                           gavl_dictionary_get_string(parent, GAVL_META_LABEL));
  
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
  
  .protocols = "command",

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
