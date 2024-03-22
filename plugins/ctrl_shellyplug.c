
#include <string.h>

#include <config.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/bggavl.h>
#include <gmerlin/state.h>

#include <gavl/http.h>
#include <gavl/log.h>
#define LOG_DOMAIN "shellyplug"
#include <control.h>

#define FLAG_ONLINE (1<<0)

/* If polling takes longer than this, switch to offline */
#define POLL_TIMEOUT  (5*GAVL_TIME_SCALE)
#define POLL_INTERVAL (5*GAVL_TIME_SCALE)

#define STATE_IDLE     0
#define STATE_POLL     1
#define STATE_SETTINGS 2
#define STATE_COMMAND  3
#define STATE_OFFLINE  4

typedef struct
  {
  gavl_io_t * io;
  bg_controllable_t ctrl;
  gavl_time_t last_poll_time;
  char * addr;
  gavl_buffer_t json_buffer;
  int status;
  gavl_dictionary_t state;

  gavl_msg_t * cmd;
  
  } shelly_t;

static void reset_connection(shelly_t * s)
  {
  if(s->io)
    {
    gavl_io_destroy(s->io);
    s->io = NULL;
    }
  }

static void set_offline(shelly_t * s)
  {
  reset_connection(s);
  s->status = STATE_OFFLINE;
  s->last_poll_time = gavl_time_get_monotonic();
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Shellyplug at %s seems to be offline", s->addr);
  }

  
static int handle_msg(shelly_t * s)
  {
  switch(s->cmd->NS)
    {
    case BG_MSG_NS_STATE:
      switch(s->cmd->ID)
        {
        case BG_CMD_SET_STATE:
          {
          gavl_value_t val;
          const char * ctx;
          const char * var;
          int delay = 0;

          int last = 0;
          
          gavl_value_init(&val);
          
          gavl_msg_get_state(s->cmd,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          fprintf(stderr, "Set state shelly %s %s\n", ctx, var);
          
          gavl_dictionary_get_int(&s->cmd->header, 
                                  GAVL_CONTROL_DELAY, &delay);
          if(!strcmp(var, "switch"))
            {
            char * uri;
            if(delay > 0)
              uri = gavl_sprintf("%s/relay/0?turn=%s&timer=%d", s->addr, (val.v.i ? "off" : "on"), delay);
            else
              uri = gavl_sprintf("%s/relay/0?turn=%s", s->addr, (val.v.i ? "on" : "off"));

            //            fprintf(stderr, "URI: %s\n", uri);

            if(s->io)
              {
              fprintf(stderr, "** Set state: I/O already exists %d\n", s->status);
              }
            
            s->io = gavl_http_client_create();
            gavl_buffer_reset(&s->json_buffer);
            gavl_http_client_set_response_body(s->io, &s->json_buffer);

            if(!gavl_http_client_run_async(s->io, "GET", uri))
              set_offline(s);
            s->status = STATE_COMMAND;
            free(uri);
            }
          
          break;
          }
        }
      break;
    }
  return 1;
  }

static void start_poll(shelly_t * s, gavl_time_t cur)
  {
  char * uri;
  /* Start polling */
  if(s->io)
    {
    fprintf(stderr, "** start_poll: I/O already exists %d\n", s->status);
    }
  
  s->io = gavl_http_client_create();
  gavl_buffer_reset(&s->json_buffer);
  gavl_http_client_set_response_body(s->io, &s->json_buffer);

  uri = gavl_sprintf("%s/status", s->addr);
    
  if(!gavl_http_client_run_async(s->io, "GET", uri))
    set_offline(s);
  s->status = STATE_POLL;
  //  fprintf(stderr, "Polling\n");
  s->last_poll_time = cur;
  }

static void handle_poll_result(shelly_t * s)
  {
  /* Handle poll result */
  json_object * child;
  json_object * obj = NULL;
  double power;
  int ison;
  gavl_value_t val;
  
  obj = json_tokener_parse((const char*)s->json_buffer.buf);

  if(!(child = bg_json_dict_get_array(obj, "relays")) ||
     !(child = json_object_array_get_idx(child, 0)))
    {
    set_offline(s);
    return;
    }
      
  ison = bg_json_dict_get_bool(child, "ison");
      
  if(!(child = bg_json_dict_get_array(obj, "meters")) ||
     !(child = json_object_array_get_idx(child, 0)))
    {
    set_offline(s);
    return;
    }
  
  power = bg_json_dict_get_double(child, "power");
  
  if(obj)
    json_object_put(obj);

  gavl_value_init(&val);
  gavl_value_set_int(&val, ison);
      
  bg_state_set(&s->state, 0, NULL, "switch", 
               &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  gavl_value_reset(&val);

  gavl_value_set_float(&val, power);
  bg_state_set(&s->state, 1, NULL, "power", 
               &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  gavl_value_reset(&val);
  
  s->status = STATE_IDLE;
  
  }

static int update_shellyplug(void * priv)
  {
  int result;
  int ret = 0;
  shelly_t * s = priv;
  gavl_time_t cur = gavl_time_get_monotonic();
  int old_status = s->status;
  gavl_msg_t * evt;

  //  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Update shellyplug %d", s->status);
    
  /* Check if running operations got completed */
  
  switch(s->status)
    {
    case STATE_IDLE:
    case STATE_OFFLINE:
      /* Initial poll */
      if(s->last_poll_time == GAVL_TIME_UNDEFINED)
        start_poll(s, cur);
      break;
    case STATE_POLL:
      if(cur - s->last_poll_time > POLL_TIMEOUT)
        {
        set_offline(s);
        ret++;
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Polling timed out");
        goto end;
        }
      result = gavl_http_client_run_async_done(s->io, 0);
      if(result > 0)
        {
        ret++;
        handle_poll_result(s);
        
        /* Adjust the poll timer to the *end* of the polling. This ensures constant intervals
         * between the end of one and the start of the next poll operation */
        s->last_poll_time = cur;
        
        reset_connection(s);
        //        fprintf(stderr, "Polling done\n");
        }
      else if(result < 0) /* Error */
        {
        set_offline(s);
        }
      break;
    case STATE_COMMAND:
      result = gavl_http_client_run_async_done(s->io, 0);
      if(result > 0)
        {
        ret++;
        reset_connection(s);
        
        if(s->cmd)
          {
          bg_msg_sink_done_read(s->ctrl.cmd_sink, s->cmd);
          s->cmd = NULL;
          }
        /* Force polling */
        s->last_poll_time = GAVL_TIME_UNDEFINED;
        
        s->status = STATE_IDLE;
        }
      else if(result < 0)
        {
        set_offline(s);
        }
      
      break;
    }

  /* Check for new commands */

  if(s->status == STATE_IDLE)
    {
    if((s->cmd = bg_msg_sink_get_read(s->ctrl.cmd_sink)))
      handle_msg(s);
    }
  
  if((s->status == STATE_IDLE) ||
     (s->status == STATE_OFFLINE))
    {
    /* Check for polling */
    if(cur - s->last_poll_time > POLL_INTERVAL)
      start_poll(s, cur);
    }
  end:

  if((old_status == STATE_OFFLINE) &&
     (s->status == STATE_IDLE))
    {
    gavl_dictionary_t dict;
    gavl_dictionary_init(&dict);
    
    /* Device became online */
    evt = bg_msg_sink_get(s->ctrl.evt_sink);
    gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
    gavl_dictionary_set_string(&evt->header, GAVL_MSG_CONTEXT_ID, "switch");
    gavl_msg_set_arg_dictionary(evt, 0, &dict);
    bg_msg_sink_put(s->ctrl.evt_sink);

    evt = bg_msg_sink_get(s->ctrl.evt_sink);
    gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
    gavl_dictionary_set_string(&evt->header, GAVL_MSG_CONTEXT_ID, "power");
    gavl_msg_set_arg_dictionary(evt, 0, &dict);
    bg_msg_sink_put(s->ctrl.evt_sink);
    
    gavl_dictionary_free(&dict);
    
    }

  if((old_status != STATE_OFFLINE) &&
     (s->status == STATE_OFFLINE))
    {
    gavl_dictionary_t dict;
    gavl_dictionary_init(&dict);
    
    /* Device became offline */
    evt = bg_msg_sink_get(s->ctrl.evt_sink);
    gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
    gavl_dictionary_set_string(&evt->header, GAVL_MSG_CONTEXT_ID, "switch");
    gavl_msg_set_arg_dictionary(evt, 0, &dict);
    bg_msg_sink_put(s->ctrl.evt_sink);

    evt = bg_msg_sink_get(s->ctrl.evt_sink);
    gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
    gavl_dictionary_set_string(&evt->header, GAVL_MSG_CONTEXT_ID, "power");
    gavl_msg_set_arg_dictionary(evt, 0, &dict);
    bg_msg_sink_put(s->ctrl.evt_sink);
    
    gavl_dictionary_free(&dict);
    }

  if((old_status != STATE_IDLE) &&
     (s->status == STATE_IDLE))
    {
    /* Device became idle */
    evt = bg_msg_sink_get(s->ctrl.evt_sink);
    gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_IDLE, GAVL_MSG_NS_CONTROL);
    bg_msg_sink_put(s->ctrl.evt_sink);
    }
  
  return ret;
  }

static int open_shellyplug(void * priv, const char * addr)
  {
  const char * pos;
  shelly_t * s = priv;

  if(!(pos = strstr(addr, "://")))
    return 0;

  s->addr = gavl_sprintf("http%s", pos);

  s->last_poll_time = GAVL_TIME_UNDEFINED;

  s->status = STATE_OFFLINE;
  
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
                       bg_msg_sink_create(NULL, s, 0),
                       bg_msg_hub_create(1));
  
  return s;
  }

static void destroy_shellyplug(void *priv)
  {
  shelly_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  free(s);
  }

static bg_controllable_t * get_controllable_shellyplug(void * priv)
  {
  shelly_t * s = priv;
  return &s->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_shellyplug",
    .long_name = TRS("Shelly plug"),
    .description = TRS("Shelly Plug"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_shellyplug,
    .destroy =   destroy_shellyplug,
    .get_controllable =   get_controllable_shellyplug,
    .priority =         1,
    },
  
  .protocols = "shellyplug",

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
