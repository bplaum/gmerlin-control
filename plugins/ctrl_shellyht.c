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
#define LOG_DOMAIN "shellyht"
#include <control.h>
#include <gavl/utils.h>

#define FLAG_ONLINE (1<<0)

/* If polling takes longer than this, switch to offline */
#define POLL_TIMEOUT  (5*GAVL_TIME_SCALE)
#define POLL_INTERVAL (5*GAVL_TIME_SCALE)

/*
#define STATE_IDLE     0
#define STATE_POLL     1
#define STATE_SETTINGS 2
#define STATE_COMMAND  3
#define STATE_OFFLINE  4
*/

/* Flags */

#define TEMPERATURE_CHANGED (1<<2)
#define HUMIDITY_CHANGED  (1<<3)

// #define SWITCH_CHANGED (1<<1)

typedef struct
  {
  //  gavl_io_t * io;
  bg_controllable_t ctrl;

  /*
  gavl_time_t last_poll_time;
  char * addr;
  gavl_buffer_t json_buffer;
  int status;
  gavl_msg_t * cmd;
  */

  int flags;
  
  char * topic;
  
  gavl_dictionary_t state;

  float temperature;
  float humididy;
  
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
          gavl_value_t val;
          const gavl_value_t  * buf_val;
          const gavl_buffer_t * buf;
                    
          const char * id = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);
          /* Got mqtt message */
          //          fprintf(stderr, "Got mqtt message: %s\n", id);

          if(!(buf_val = gavl_msg_get_arg_c(msg, 0)) ||
             !(buf = gavl_value_get_binary(buf_val)))
            {
            /* Error */
            return 1;
            }
          
          if(!strcmp(id, "sensor/temperature"))
            {
            gavl_value_init(&val);
            gavl_value_set_float(&val, strtod((char*)buf->buf, NULL));

            bg_state_set(&s->state, 1, NULL, "temperature", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

            }
          else if(!strcmp(id, "sensor/humidity"))
            {
            gavl_value_init(&val);
            gavl_value_set_float(&val, strtod((char*)buf->buf, NULL));
            bg_state_set(&s->state, 1, NULL, "humidity", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            }
          else if(!strcmp(id, "sensor/battery"))
            {
            gavl_value_init(&val);
            gavl_value_set_float(&val, strtod((char*)buf->buf, NULL));
            bg_state_set(&s->state, 1, NULL, "battery", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            }
          else if(!strcmp(id, "online"))
            {
            // gavl_hexdump(buf->buf, buf->len, 16);
            }
          else if(!strcmp(id, "announce"))
            {
            const char * ip;
            json_object * obj;
            //            gavl_hexdump(buf->buf, buf->len, 16);
            obj = json_tokener_parse((const char*)buf->buf);

            ip = bg_json_dict_get_string(obj, "ip");

            if(ip)
              {
              char * addr;
              addr = gavl_sprintf("http://%s", ip);
              fprintf(stderr, "Got shellyht address: %s\n", addr);

              if(!s->web_uri || strcmp(s->web_uri, addr))
                s->web_uri = gavl_strrep(s->web_uri, addr);
              update_web_uri(s);
              free(addr);
              }
            
            if(obj)
              json_object_put(obj);
            
            }
          break;
          }
        }
      }
      break;
    }
  return 1;
  }


static int update_shellyht(void * priv)
  {
  int ret = 0;
  return ret;
  }

static int open_shellyht(void * priv, const char * addr)
  {
  gavl_buffer_t buf;
  char * path = NULL;
  shelly_t * s = priv;
  char * topic;

  if(!gavl_url_split(addr, NULL, NULL, NULL, NULL, NULL, &path) ||
     !path)
    return 0;
  
  s->topic = gavl_strdup(path+1);
  free(path);

  bg_mqtt_subscribe(s->topic, s->ctrl.cmd_sink);

  /* Request announcement */
  
  topic = gavl_sprintf("%s/command", s->topic);
  
  gavl_buffer_init(&buf);
  buf.buf = (uint8_t*)"announce";
  buf.len = strlen((const char*)buf.buf);
  
  fprintf(stderr, "Publishing: %s\n", topic);
  
  bg_mqtt_publish(topic, &buf, 1, 0);
  
  return 1;
  }

  
static void get_controls_shellyht(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  gavl_value_t val1, val2;
  //  shelly_t * s = priv;

  gavl_value_init(&val1);
  gavl_value_init(&val2);
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_CURVE,
                                  "temperature",
                                  "Temperature");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);

  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, (char[]){ 0xC2, 0xB0, 'C', 0x00} );
  gavl_control_init_history(ctrl, 48LL*3600*GAVL_TIME_SCALE);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_HISTORY_PERSISTENT, 1);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_DIGITS, 2);
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_HISTORY_MODE,
                             GAVL_CONTROL_HISTORY_CLOCK_HM);
  gavl_dictionary_set_long(ctrl, GAVL_CONTROL_HISTORY_TIME_STEP,
                           (int64_t)GAVL_TIME_SCALE * 3600 * 6);
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_CURVE,
                                  "humidity",
                                  "Humidity");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);

  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%" );
  gavl_control_init_history(ctrl, 48LL*3600*GAVL_TIME_SCALE);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_HISTORY_PERSISTENT, 1);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_DIGITS, 2);
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_HISTORY_MODE,
                             GAVL_CONTROL_HISTORY_CLOCK_HM);
  gavl_dictionary_set_long(ctrl, GAVL_CONTROL_HISTORY_TIME_STEP,
                           (int64_t)GAVL_TIME_SCALE * 3600 * 6);
 
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_METER,
                                  "battery",
                                  "Battery");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 100.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW, 20.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH, 80.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 100.0);
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%" );
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_LINK,
                                  "web",
                                  "Web interface");
  gavl_dictionary_set_string(parent, GAVL_META_URI, "#");
  
  }

static void * create_shellyht()
  {
  shelly_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(handle_msg, s, 1),
                       bg_msg_hub_create(1));
  
  return s;
  }

static void destroy_shellyht(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  if(s->web_uri)
    free(s->web_uri);
  free(s);
  }

static bg_controllable_t * get_controllable_shellyht(void * priv)
  {
  shelly_t * s = priv;
  return &s->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_shellyht",
    .long_name = TRS("Shelly Humidity and Temperature"),
    .description = TRS("Shellyht"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_shellyht,
    .destroy =   destroy_shellyht,
    .get_controllable =   get_controllable_shellyht,
    .priority =         1,
    },
  
  .protocols = "shellyht",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_shellyht,
  .open   = open_shellyht,
  .get_controls   = get_controls_shellyht,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
