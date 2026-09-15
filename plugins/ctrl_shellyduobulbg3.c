#define _GNU_SOURCE

#include <math.h>

#include <string.h>

#include <config.h>
#include <mqtt.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/bggavl.h>
#include <gmerlin/state.h>

#include <gavl/http.h>
#include <gavl/log.h>
#define LOG_DOMAIN "shellyduobulbg3"
#include <control.h>
#include <shellyrpc.h>
#include <gavl/utils.h>

// #define USE_RGBCOLOR

#define RESPONSE_TOPIC "gmerlin-shellyduobulbg3"

/* Flags */

#define TEMPERATURE_CHANGED (1<<0)
#define BRIGHTNESS_CHANGED  (1<<1)
#define SWITCH_CHANGED      (1<<2)
// #define FLAG_INIT           (1<<8)

// #define SWITCH_CHANGED (1<<1)

// shelly_rpc_init(shelly_rpc_t * r, bg_controllable_t * ctrl, const char * device);

typedef struct
  {
  shelly_rpc_t r;
  
  bg_controllable_t ctrl;

  int flags;
  
  char * topic;
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
  shelly_t * s = data;

  if(!strcmp(name, "wifi"))
    {
    const char * ip;
    
    /* Get device IP */
    fprintf(stderr, "Got Wifi config\n");

    ip = gavl_dictionary_get_string(dict, "sta_ip");
    
    if(ip)
      {
      char * addr;
      addr = gavl_sprintf("http://%s", ip);
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got web URI: %s", addr);
      s->web_uri = gavl_strrep(s->web_uri, addr);
      update_web_uri(s);
      free(addr);
      }
    
    }
  else if(!strcmp(name, "cct:0"))
    {
    const gavl_value_t * val;
    
    /* */
    //    fprintf(stderr, "Got Bulb config\n");
    //    gavl_dictionary_dump(dict, 2);
#if 1
    if((val = gavl_dictionary_get(dict, "brightness")))
      {
      bg_state_set(&s->state, 1, NULL, "brightness", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      
      }
    if((val = gavl_dictionary_get(dict, "output")))
      {
      bg_state_set(&s->state, 1, NULL, "switch", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      }
    if((val = gavl_dictionary_get(dict, "apower")))
      {
      bg_state_set(&s->state, 1, NULL, "power", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      }
    if((val = gavl_dictionary_get(dict, "ct")))
      {
      bg_state_set(&s->state, 1, NULL, "temperature", 
                   val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      }
    
#endif
    
    
    
    }
  
  //  fprintf(stderr, "update_status %s\n", name);
  //  gavl_dictionary_dump(dict, 2);
  
  }

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
          fprintf(stderr, "Got mqtt message: %s\n", id);
          
          if(!(buf_val = gavl_msg_get_arg_c(msg, 0)) ||
             !(buf = gavl_value_get_binary(buf_val)))
            {
            /* Error */
            return 1;
            }

          gavl_hexdump(buf->buf, buf->len, 16);
#if 0          
          
          if(!strcmp(id, "light/0/power"))
            {
            float power;
            gavl_value_init(&val);

            power = strtod((const char*)buf->buf, NULL);
            
            gavl_value_set_float(&val, power);
            bg_state_set(&s->state, 0, NULL, "power", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
            
            //            gavl_hexdump(buf->buf, buf->len, 16);
            }
          else if(!strcmp(id, "announce"))
            {
            const char * ip;
            const char * dev;
            json_object * obj;
            //            gavl_hexdump(buf->buf, buf->len, 16);
            obj = json_tokener_parse((const char*)buf->buf);

            if(!(dev = bg_json_dict_get_string(obj, "id")) ||
               strcmp(dev, s->dev))
              return 1;
            
            ip = bg_json_dict_get_string(obj, "ip");

            if(ip)
              {
              char * addr;
              addr = gavl_sprintf("http://%s", ip);

              // fprintf(stderr, "Got shellybulb address: %s\n", addr);

              if(!s->web_uri || strcmp(s->web_uri, addr))
                s->web_uri = gavl_strrep(s->web_uri, addr);
              update_web_uri(s);
              free(addr);
              }
            
            if(obj)
              json_object_put(obj);
            
            gavl_control_set_online(s->ctrl.evt_sink, "/", 1);
            }
          else if(!strcmp(id, "color/0/status"))
            {
            json_object * obj = NULL;
            
            obj = json_tokener_parse((const char*)buf->buf);
            
            s->switch_val = bg_json_dict_get_bool(obj, "ison");
            
            s->brightness = bg_json_dict_get_int(obj, "brightness");
            s->temperature = bg_json_dict_get_int(obj, "temp");
            
            // gavl_hexdump(buf->buf, buf->len, 16);

            // fprintf(stderr, "Color: %d %d %d\n", s->red, s->green, s->blue);
            
            if(obj)
              json_object_put(obj);

            gavl_value_init(&val);
            gavl_value_set_int(&val, s->switch_val);
      
            bg_state_set(&s->state, 0, NULL, "switch", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
            
            gavl_value_set_int(&val, s->temperature);
            bg_state_set(&s->state, 0, NULL, "temperature", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->brightness);
            bg_state_set(&s->state, 0, NULL, "brightness", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
  
            
            }
          else if(!strcmp(id, "online"))
            {
            if(!strcmp((const char*)buf->buf, "true"))
              {
              fprintf(stderr, "shellybulb is online\n");
              gavl_control_set_online(s->ctrl.evt_sink, "/", 1);
              }
            else
              {
              fprintf(stderr, "shellybulb is offline\n");
              gavl_control_set_online(s->ctrl.evt_sink, "/", 0);
              }
            }
#endif
          break;
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
          
          if(!strcmp(var, "switch"))
            {
            gavl_dictionary_set(&s->state, var, &val);
            s->flags |= SWITCH_CHANGED;
            }
          else if(!strcmp(var, "temperature"))
            {
            gavl_dictionary_set(&s->state, var, &val);
            s->flags |= TEMPERATURE_CHANGED;
            }
          else if(!strcmp(var, "brightness"))
            {
            gavl_dictionary_set(&s->state, var, &val);
            s->flags |= BRIGHTNESS_CHANGED;
            }
          break;
          }
        }
      break;
    }
  return 1;
  }


static int update_shellybulb(void * priv)
  {
  int ret = 0;
  shelly_t * s = priv;

  if(s->flags & (TEMPERATURE_CHANGED | BRIGHTNESS_CHANGED | SWITCH_CHANGED))
    {
    char * json = NULL;

    int switch_val = 0;
    int brightness = 0;
    int temperature = 0;

    gavl_dictionary_get_int(&s->state, "switch", &switch_val);
    gavl_dictionary_get_int(&s->state, "brightness", &brightness);
    gavl_dictionary_get_int(&s->state, "temperature", &temperature);
    
    json = gavl_sprintf("{\"id\":1,\"src\":\""RESPONSE_TOPIC"\",\"method\":\"CCT.Set\",\"params\":{\"id\":0,"
                        "\"on\":%s,\"brightness\":%d,\"ct\":%d}}",
                        (switch_val ? "true" : "false"),
                        brightness, temperature);
    
    s->flags &= ~(TEMPERATURE_CHANGED | BRIGHTNESS_CHANGED | SWITCH_CHANGED);
    
    if(json)
      {
      gavl_buffer_t buf;
      
      gavl_buffer_init(&buf);
      buf.buf = (uint8_t*)json;
      buf.len = strlen(json);

      bg_mqtt_publish(s->topic, &buf, 1, 0);
      free(json);
      }
    ret++;  
    }
  
  return ret;
  }


static int open_shellybulb(void * priv, const char * addr)
  {
  char * path = NULL;
  shelly_t * s = priv;

  if(!gavl_url_split(addr, NULL, NULL, NULL, NULL, NULL, &path) ||
     !path)
    return 0;
  
  
  s->topic = gavl_sprintf("%s/rpc", path+1);
  s->dev = gavl_strdup(path+1);
  free(path);

  fprintf(stderr, "%s %s\n", s->topic, s->dev);
  
  shelly_rpc_init(&s->r, &s->ctrl, s->dev);

  s->r.update_status = update_status;
  s->r.data = s;
  
  /* Request status */
#if 0  
  gavl_buffer_init(&buf);

  buf.buf = (uint8_t*)"{\"id\":1,\"src\":\"user_1\",\"method\":\"Wifi.GetStatus\"}";
  buf.len = strlen((const char*)buf.buf);
  
  fprintf(stderr, "Publishing: %s\n", s->topic);
  bg_mqtt_publish(s->topic, &buf, 1, 0);
#endif
  //  gavl_control_set_online(s->ctrl.evt_sink, "/", 0);

  
  
  
  return 1;
  }

static gavl_dictionary_t * create_slider(gavl_dictionary_t * parent,
                                         const char * id, const char * label, int min, int max)
  {
  gavl_dictionary_t * 
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_SLIDER,
                                  id, label);
  
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MIN, min);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MAX, max);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_VALUE, min);
  gavl_control_set_type(ctrl, GAVL_TYPE_INT);
  return ctrl;
  }
  
static void get_controls_shellybulb(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  
  gavl_dictionary_set_int(parent, GAVL_CONTROL_OFFLINE, 1);

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
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 10.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 1.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 5.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_DIGITS, 2);
  
  create_slider(parent, "temperature", "Temperature", 2700, 6500);
  create_slider(parent, "brightness", "Brightness", 0, 100);
  
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_LINK,
                                  "web",
                                  "Web interface");
  gavl_dictionary_set_string(ctrl, GAVL_META_URI, "#");
  
  }

static void * create_shellybulb()
  {
  shelly_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(handle_msg, s, 1),
                       bg_msg_hub_create(1));
  return s;
  }

static void destroy_shellybulb(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  if(s->web_uri)
    free(s->web_uri);
  if(s->topic)
    free(s->topic);
  free(s);
  }

static bg_controllable_t * get_controllable_shellybulb(void * priv)
  {
  shelly_t * s = priv;
  return &s->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_shellyduobulbg3",
    .long_name = TRS("Shelly Duo Bulb G3"),
    .description = TRS("Shelly Duo Bulb G3"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_shellybulb,
    .destroy =   destroy_shellybulb,
    .get_controllable =   get_controllable_shellybulb,
    .priority =         1,
    },
  
  .protocols = "shellybulbduobulbg3",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_shellybulb,
  .open   = open_shellybulb,
  .get_controls   = get_controls_shellybulb,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;

