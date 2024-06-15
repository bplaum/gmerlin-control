#define _GNU_SOURCE

#include <config.h>
#include <mqtt.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/bggavl.h>
#include <gmerlin/state.h>

#include <gavl/http.h>
#include <gavl/log.h>
#define LOG_DOMAIN "shellybulb"
#include <control.h>
#include <gavl/utils.h>

typedef struct
  {
  //  gavl_io_t * io;
  bg_controllable_t ctrl;
  char * topic;
  } shelly_t;

static int handle_msg(void * data, gavl_msg_t * msg)
  {
  shelly_t * s = data;
  
  switch(msg->NS)
    {
    case GAVL_MSG_NS_MQTT:
      {
      switch(msg->ID)
        {
        case GAVL_MSG_MQTT:
          {
          //          gavl_value_t val;
          const gavl_value_t  * buf_val;
          const gavl_buffer_t * buf;
                    
          const char * id = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);
          /* Got mqtt message */

          if(!(buf_val = gavl_msg_get_arg_c(msg, 0)) ||
             !(buf = gavl_value_get_binary(buf_val)))
            {
            /* Error */
            return 1;
            }

          fprintf(stderr, "Got mqtt message: %s %s\n", id, (char*)buf->buf);
          }
        }
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
          
          break;
          }
        }
      break;
    }
  return 1;
  }


static int open_shellyplug(void * priv, const char * addr)
  {
  //  gavl_buffer_t buf;
  shelly_t * s = priv;

  if(!gavl_url_split(addr, NULL, NULL, NULL, &s->topic, NULL, NULL) ||
     !s->topic)
    return 0;

  s->topic = gavl_strcat(s->topic, "/#");
  
  bg_mqtt_subscribe(s->topic, s->ctrl.cmd_sink);

  /* Request announcement */
#if 0  
  topic = gavl_sprintf("%s/command", s->topic);
  
  gavl_buffer_init(&buf);
  buf.buf = (uint8_t*)"announce";
  buf.len = strlen((const char*)buf.buf);
  
  fprintf(stderr, "Publishing: %s\n", topic);
  
  bg_mqtt_publish(topic, &buf, 1, 0);
#endif
  
  return 1;

  }


static void get_controls_shellyplug(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  gavl_value_t val1, val2;
  //  shelly_t * s = priv;

  gavl_value_init(&val1);
  gavl_value_init(&val2);
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_POWERBUTTON,
                                  "switch",
                                  "Switch");

  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_VALUE, 0);
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_METER,
                                  "power",
                                  "Power");
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "W");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 2500.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 100.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 1000.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);
  }

static void * create_shellyplug()
  {
  shelly_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(handle_msg, s, 1),
                       bg_msg_hub_create(1));
#ifdef USE_RGBCOLOR
  gavl_value_set_color_rgb(&s->color);
#endif  
  return s;
  }


static void destroy_shellyplug(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  //  if(s->web_uri)
  //    free(s->web_uri);
  free(s);
  }




static bg_controllable_t * get_controllable_shellyplug(void * priv)
  {
  shelly_t * s = priv;
  return &s->ctrl;
  }

static int update_shellyplug(void * priv)
  {
  return 0;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_shellypluglus",
    .long_name = TRS("Shelly plug Plus"),
    .description = TRS("Shelly plug Plus (gen-2 RPC via mqtt)"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_shellyplug,
    .destroy =   destroy_shellyplug,
    .get_controllable =   get_controllable_shellyplug,
    .priority =         1,
    },
  
  .protocols = "shellyplug-plus",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_shellyplug,
  .open   = open_shellyplug,
  .get_controls   = get_controls_shellyplug,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
