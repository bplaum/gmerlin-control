#define _GNU_SOURCE

#include <string.h>


#include <config.h>
#include <mqtt.h>
#include <shellyrpc.h>

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
  bg_controllable_t ctrl;
  shelly_rpc_t r;
  char * dev;
  gavl_dictionary_t state;
  char * web_uri;
  } shelly_t;

static void update_web_uri(shelly_t * s)
  {
  gavl_msg_t * msg;
  gavl_dictionary_t dict;
  gavl_dictionary_init(&dict);
  gavl_dictionary_set_string(&dict, GAVL_META_URI, s->web_uri);
  msg = bg_msg_sink_get(s->ctrl.evt_sink);
  gavl_msg_set_id_ns(msg, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
  gavl_dictionary_set_string(&msg->header, GAVL_MSG_CONTEXT_ID, "web");
  gavl_msg_set_arg_dictionary(msg, 0, &dict);
  bg_msg_sink_put(s->ctrl.evt_sink);
  gavl_dictionary_free(&dict);
  }

static void update_status(void * data, const char * name, const gavl_dictionary_t * dict)
  {
  const gavl_value_t * val;
  shelly_t * s = data;
  
  if(!strcmp(name, "switch:0"))
    {
    if((val = gavl_dictionary_get(dict, "output")))
      bg_state_set(&s->state, 1, NULL, "switch", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

    if((val = gavl_dictionary_get(dict, "apower")))
      bg_state_set(&s->state, 1, NULL, "power", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

    if((val = gavl_dictionary_get(dict, "voltage")))
      bg_state_set(&s->state, 1, NULL, "voltage", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

    if((val = gavl_dictionary_get(dict, "current")))
      bg_state_set(&s->state, 1, NULL, "current", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    
    }
  else if(!strcmp(name, "wifi"))
    {
    const char * ip;

    char * addr;

    if((ip = gavl_dictionary_get_string(dict, "sta_ip")))
      {
      addr = gavl_sprintf("http://%s", ip);

      // fprintf(stderr, "Got shellyplugplus address: %s\n", addr);
      
      if(!s->web_uri || strcmp(s->web_uri, addr))
        s->web_uri = gavl_strrep(s->web_uri, addr);
      update_web_uri(s);
        
      free(addr);
      }
    
    }
  
  }

static int handle_msg(void * data, gavl_msg_t * msg)
  {
  shelly_t * s = data;
  
  switch(msg->NS)
    {
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

          if(!strcmp(var, "switch"))
            {
            int on = 0;
            int delay = 0;
            char * args = NULL;
            
            gavl_value_get_int(&val, &on);

            // fprintf(stderr, "set_switch: %d\n", on);

            gavl_dictionary_get_int(&msg->header, 
                                    GAVL_CONTROL_DELAY, &delay);

            if(delay > 0)
              {
              args = gavl_sprintf("{ \"id\": 0, \"on\":%s, \"toggle_after\":%d}",
                                  (on ? "false" : "true"), delay);
              }
            else
              {
              args = gavl_sprintf("{ \"id\": 0, \"on\":%s}", (on ? "true" : "false"));
              }
            
            shellyrpc_call_method(&s->r, 0, "Switch.Set", args);
            free(args);
            }

          gavl_value_free(&val);
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

  if(!gavl_url_split(addr, NULL, NULL, NULL, &s->dev, NULL, NULL) ||
     !s->dev)
    return 0;

  shelly_rpc_init(&s->r, &s->ctrl, s->dev);

  s->r.update_status = update_status;
  s->r.data = s;
  
  return 1;

  }

static void get_controls_shellyplug(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
    
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

  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 2500.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 100.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 1000.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);

  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_METER,
                                  "voltage",
                                  "Voltage");
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "V");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);

  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 250.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 220.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 240.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 230.0);

  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_METER,
                                  "current",
                                  "Current");
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "A");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);

  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 16.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 1.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 10.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);

  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_LINK,
                                  "web",
                                  "Web interface");
  gavl_dictionary_set_string(parent, GAVL_META_URI, "#");
  }

static void * create_shellyplug()
  {
  shelly_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(handle_msg, s, 1),
                       bg_msg_hub_create(1));

  return s;
  }

static void destroy_shellyplug(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  if(s->web_uri)
    free(s->web_uri);
  if(s->dev)
    free(s->dev);
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
