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
#define LOG_DOMAIN "shellybulb"
#include <control.h>
#include <gavl/utils.h>

#define USE_RGBCOLOR


/* Flags */

#define COLOR_CHANGED       (1<<0)
#define HS_CHANGED          (1<<1)
#define TEMPERATURE_CHANGED (1<<2)
#define BRIGHTNESS_CHANGED  (1<<3)
#define GAIN_CHANGED        (1<<4)
#define MODE_CHANGED        (1<<5)
#define EFFECT_CHANGED      (1<<6)
#define SWITCH_CHANGED      (1<<7)
// #define FLAG_INIT           (1<<8)

// #define SWITCH_CHANGED (1<<1)

static const char * mode_color = "color";
static const char * mode_white = "white";

// static void rgb2hsv(float in_r, float in_g, float in_b, float * out_h, float * out_s, float * out_v);
// static void hsv2rgb(float in_h, float in_s, float in_v, float * out_r, float * out_g, float * out_b);

//static void rgb2hsv_i(int in_r, int in_g, int in_b, int * out_h, int * out_s, int * out_v);
// static void hsv2rgb_i(int in_h, int in_s, int in_v, int * out_r, int * out_g, int * out_b);


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
  char * dev;
  
  gavl_dictionary_t state;

  int switch_val;
#ifdef USE_RGBCOLOR
  gavl_value_t color;
#else
  int red;
  int green;
  int blue;
  int gain;
  int hue;
  int saturation;
#endif
  int brightness;
  int temperature;

  int effect;
  const char * mode;

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

            s->mode = bg_json_dict_get_string(obj, "mode");
            
            if(!strcmp(s->mode, "color"))
              s->mode = mode_color;
            else
              s->mode = mode_white;
  
            s->effect = bg_json_dict_get_int(obj, "effect");

#ifdef USE_RGBCOLOR
            s->color.v.color[0] = (double)bg_json_dict_get_int(obj, "red")/255.0;
            s->color.v.color[1] = (double)bg_json_dict_get_int(obj, "green")/255.0;
            s->color.v.color[2] = (double)bg_json_dict_get_int(obj, "blue")/255.0;
#else
            s->red = bg_json_dict_get_int(obj, "red");
            s->green = bg_json_dict_get_int(obj, "green");
            s->blue = bg_json_dict_get_int(obj, "blue");
            s->gain = bg_json_dict_get_int(obj, "gain");
#endif
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
#ifdef USE_RGBCOLOR
            bg_state_set(&s->state, 0, NULL, "color", 
                         &s->color, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
#else
            gavl_value_set_int(&val, s->red);
            bg_state_set(&s->state, 0, NULL, "red", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->green);
            bg_state_set(&s->state, 0, NULL, "green", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->blue);
            bg_state_set(&s->state, 0, NULL, "blue", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
            gavl_value_set_int(&val, s->gain);
            bg_state_set(&s->state, 0, NULL, "gain", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->hue);
            bg_state_set(&s->state, 0, NULL, "hue", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->saturation);
            bg_state_set(&s->state, 0, NULL, "saturation", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
#endif
  

            gavl_value_set_int(&val, s->temperature);
            bg_state_set(&s->state, 0, NULL, "temperature", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            gavl_value_set_int(&val, s->brightness);
            bg_state_set(&s->state, 0, NULL, "brightness", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
  
            gavl_value_set_string(&val, s->mode);
            bg_state_set(&s->state, 0, NULL, "mode", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);
  
            gavl_value_set_string_nocopy(&val, gavl_sprintf("%d", s->effect));
            bg_state_set(&s->state, 1, NULL, "effect", 
                         &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
            gavl_value_reset(&val);

            //            fprintf(stderr, "Updated complete state\n");
            //            gavl_dictionary_dump(&s->state, 2);
            
            }
          else if(!strcmp(id, "color/0"))
            {
            /* on or off */
            
            }
          else if(!strcmp(id, "online"))
            {
            if(!strcmp((const char*)buf->buf, "true"))
              {
              //              fprintf(stderr, "shellybulb is online\n");
              gavl_control_set_online(s->ctrl.evt_sink, "/", 1);
              }
            else
              {
              //              fprintf(stderr, "shellybulb is offline\n");
              gavl_control_set_online(s->ctrl.evt_sink, "/", 0);
              }
            }
          
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

          //          fprintf(stderr, "Set state shellybulb %s %s\n", ctx, var);
          
          if(!strcmp(var, "switch"))
            {
            s->switch_val = val.v.i;
            s->flags |= SWITCH_CHANGED;
            }
          else if(!strcmp(var, "mode"))
            {
            if(!strcmp(val.v.str, "color"))
              s->mode = mode_color;
            else
              s->mode = mode_white;
            s->flags |= MODE_CHANGED;
            }
#ifdef USE_RGBCOLOR
          else if(!strcmp(var, "color"))
            {
            memcpy(s->color.v.color, val.v.color, 3*sizeof(val.v.color[0]));
            //            fprintf(stderr, "Got color: %f %f %f\n", val.v.color[0], val.v.color[1], val.v.color[2]);
            s->flags |= COLOR_CHANGED;
            }
#else
          else if(!strcmp(var, "red"))
            {
            s->red = val.v.i;
            s->flags |= COLOR_CHANGED;
            }
          else if(!strcmp(var, "green"))
            {
            s->green = val.v.i;
            s->flags |= COLOR_CHANGED;
            }
          else if(!strcmp(var, "blue"))
            {
            s->blue = val.v.i;
            s->flags |= COLOR_CHANGED;
            }
          else if(!strcmp(var, "hue"))
            {
            s->hue = val.v.i;
            s->flags |= HS_CHANGED;
            }
          else if(!strcmp(var, "saturation"))
            {
            s->saturation = val.v.i;
            s->flags |= HS_CHANGED;
            }
          else if(!strcmp(var, "gain"))
            {
            s->gain = val.v.i;
            s->flags |= GAIN_CHANGED;
            }
#endif
          else if(!strcmp(var, "temperature"))
            {
            s->temperature = val.v.i;
            s->flags |= TEMPERATURE_CHANGED;
            }
          else if(!strcmp(var, "brightness"))
            {
            s->brightness = val.v.i;
            s->flags |= BRIGHTNESS_CHANGED;
            }
          else if(!strcmp(var, "effect"))
            {
            s->effect = atoi(val.v.str);
            s->flags |= EFFECT_CHANGED;
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

  if(s->flags & (COLOR_CHANGED | HS_CHANGED | TEMPERATURE_CHANGED | BRIGHTNESS_CHANGED |
                 MODE_CHANGED | EFFECT_CHANGED | GAIN_CHANGED | SWITCH_CHANGED))
    {
    char * json = NULL;
#ifdef USE_RGBCOLOR      
    json = gavl_sprintf("{ \"red\": %d, \"green\": %d, \"blue\": %d, \"gain\": 100, "
                        "\"temp\": %d, \"brightness\": %d, \"mode\": \"%s\", \"effect\": %d, "
                        "\"turn\": \"%s\" }",
                        (int)(s->color.v.color[0]*255.0+0.5),
                        (int)(s->color.v.color[1]*255.0+0.5),
                        (int)(s->color.v.color[2]*255.0+0.5),
                        s->temperature, s->brightness, s->mode, s->effect,
                        (s->switch_val ? "on" : "off"));
    //    fprintf(stderr, "JSON: %f,%f,%f %s\n", s->color.v.color[0], s->color.v.color[1], s->color.v.color[2], json);
#else
    if(s->flags & HS_CHANGED)
      {
      /* HSV -> RGB */
      hsv2rgb_i(s->hue, s->saturation, 100, &s->red, &s->green, &s->blue);
      s->flags |= COLOR_CHANGED;
      }
    json = gavl_sprintf("{ \"red\": %d, \"green\": %d, \"blue\": %d, \"gain\": %d, "
                        "\"temp\": %d, \"brightness\": %d, \"mode\": \"%s\", \"effect\": %d, "
                        "\"turn\": \"%s\" }",
                        s->red, s->green, s->blue, s->gain,
                        s->temperature, s->brightness, s->mode, s->effect,
                        (s->switch_val ? "on" : "off"));
#endif
    
      
    s->flags &= ~(COLOR_CHANGED | HS_CHANGED | TEMPERATURE_CHANGED | BRIGHTNESS_CHANGED |
                  MODE_CHANGED | EFFECT_CHANGED | GAIN_CHANGED | SWITCH_CHANGED);

    if(json)
      {
      gavl_buffer_t buf;
      char * topic = gavl_sprintf("%s/color/0/set", s->topic);
      
      gavl_buffer_init(&buf);
      buf.buf = (uint8_t*)json;
      buf.len = strlen(json);

      //    fprintf(stderr, "Publishing: %s\n%s\n", topic, json);
      
      bg_mqtt_publish(topic, &buf, 1, 0);
      free(json);
      }
    ret++;  
    }
  
  return ret;
  }

static int open_shellybulb(void * priv, const char * addr)
  {
  gavl_buffer_t buf;
  char * path = NULL;
  shelly_t * s = priv;
  char * topic;
  const char * pos;

  if(!gavl_url_split(addr, NULL, NULL, NULL, NULL, NULL, &path) ||
     !path)
    return 0;
  
  s->topic = gavl_strdup(path+1);
  free(path);

  if(!(pos = strrchr(s->topic, '/')))
    return 0;

  s->dev = gavl_strdup(pos+1);
  
  bg_mqtt_subscribe(s->topic, s->ctrl.cmd_sink);
  
  
  /* Request announcement */
  
  topic = gavl_sprintf("%s/command", s->topic);
  
  gavl_buffer_init(&buf);
  buf.buf = (uint8_t*)"announce";
  buf.len = strlen((const char*)buf.buf);
  
  fprintf(stderr, "Publishing: %s\n", topic);
  
  bg_mqtt_publish(topic, &buf, 1, 0);

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
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_PULLDOWN,
                                  "mode",
                                  "Mode");
  gavl_control_add_option(ctrl, mode_color, "Color");
  gavl_control_add_option(ctrl, mode_white, "White");

#ifdef USE_RGBCOLOR
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_RGBCOLOR,
                                  "color",
                                  "Color");
#else
  create_slider(parent, "red", "Red", 0, 255);
  create_slider(parent, "green", "Green", 0, 255);
  create_slider(parent, "blue", "Blue", 0, 255);
  create_slider(parent, "gain", "Gain", 0, 100);

  create_slider(parent, "hue", "Hue", 0, 360);
  create_slider(parent, "saturation", "Saturation", 0, 100);
#endif
  
  create_slider(parent, "temperature", "Temperature", 3000, 6500);
  create_slider(parent, "brightness", "Brightness", 0, 100);
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_PULLDOWN,
                                  "effect",
                                  "Effect");
  gavl_control_add_option(ctrl, "0", "Off");
  gavl_control_add_option(ctrl, "1", "Meteor Shower");
  gavl_control_add_option(ctrl, "2", "Gradual Change");
  gavl_control_add_option(ctrl, "3", "Flash");

  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_LINK,
                                  "web",
                                  "Web interface");
  gavl_dictionary_set_string(parent, GAVL_META_URI, "#");
  
  }

static void * create_shellybulb()
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

static void destroy_shellybulb(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  if(s->web_uri)
    free(s->web_uri);
  if(s->topic)
    free(s->topic);
  if(s->dev)
    free(s->dev);
#ifdef USE_RGBCOLOR
  gavl_value_free(&s->color);
#endif
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
    .name =      "ctrl_shellybulb_m",
    .long_name = TRS("Shelly bulb"),
    .description = TRS("Shelly bulb"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_shellybulb,
    .destroy =   destroy_shellybulb,
    .get_controllable =   get_controllable_shellybulb,
    .priority =         1,
    },
  
  .protocols = "shellybulb-m",

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

#if 0

// https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both

static void rgb2hsv(float in_r, float in_g, float in_b, float * out_h, float * out_s, float * out_v)
  {
  double      min, max, delta;

  min = in_r < in_g ? in_r : in_g;
  min = min  < in_b ? min  : in_b;

  max = in_r > in_g ? in_r : in_g;
  max = max  > in_b ? max  : in_b;

  *out_v = max;                                // v
  delta = max - min;
  if (delta < 0.00001)
    {
    *out_s = 0;
    *out_h = 0; // undefined, maybe nan?
    return;
    }
  if( max > 0.0 ) { // NOTE: if Max is == 0, this divide would cause a crash
  *out_s = (delta / max);                  // s
  } else {
  // if max is 0, then r = g = b = 0              
  // s = 0, h is undefined
  *out_s = 0.0;
  *out_h = NAN;                            // its now undefined
  return;
  }
  if( in_r >= max )                           // > is bogus, just keeps compilor happy
    *out_h = ( in_g - in_b ) / delta;        // between yellow & magenta
  else
    if( in_g >= max )
      *out_h = 2.0 + ( in_b - in_r ) / delta;  // between cyan & yellow
    else
      *out_h = 4.0 + ( in_r - in_g ) / delta;  // between magenta & cyan

  *out_h *= 60.0;                              // degrees

  if( *out_h < 0.0 )
    *out_h += 360.0;

  return;
  }


static void hsv2rgb(float in_h, float in_s, float in_v, float * out_r, float * out_g, float * out_b)
  {
  double      hh, p, q, t, ff;
  long        i;
  
  if(in_s <= 0.0)
    {       // < is bogus, just shuts up warnings
    *out_r = in_v;
    *out_g = in_v;
    *out_b = in_v;
    return;
    }
  hh = in_h;
  if(hh >= 360.0) hh = 0.0;
  hh /= 60.0;
  i = (long)hh;
  ff = hh - i;
  p = in_v * (1.0 - in_s);
  q = in_v * (1.0 - (in_s * ff));
  t = in_v * (1.0 - (in_s * (1.0 - ff)));

  switch(i)
    {
    case 0:
      *out_r = in_v;
      *out_g = t;
      *out_b = p;
      break;
    case 1:
      *out_r = q;
      *out_g = in_v;
      *out_b = p;
      break;
    case 2:
      *out_r = p;
      *out_g = in_v;
      *out_b = t;
      break;

    case 3:
      *out_r = p;
      *out_g = q;
      *out_b = in_v;
      break;
    case 4:
      *out_r = t;
      *out_g = p;
      *out_b = in_v;
      break;
    case 5:
    default:
      *out_r = in_v;
      *out_g = p;
      *out_b = q;
      break;
    }
  return;     
  }

#define FLOAT_TO_INT(f) (int)(f+0.5)

static void rgb2hsv_i(int in_r, int in_g, int in_b, int * out_h, int * out_s, int * out_v)
  {
  float h, s, v;

  rgb2hsv((float)in_r/255.0, (float)in_g/255.0, (float)in_b/255.0, &h, &s, &v);

  *out_h = FLOAT_TO_INT(h);
  *out_s = FLOAT_TO_INT(s*100.0);
  *out_v = FLOAT_TO_INT(v*100.0);
  }
  
static void hsv2rgb_i(int in_h, int in_s, int in_v, int * out_r, int * out_g, int * out_b)
  {
  float r, g, b;
  
  hsv2rgb((float)in_h, (float)in_s/100.0, (float)in_v/100.0, &r, &g, &b);

  *out_r = FLOAT_TO_INT(r*255.0);
  *out_g = FLOAT_TO_INT(g*255.0);
  *out_b = FLOAT_TO_INT(b*255.0);
  
  }

#endif
