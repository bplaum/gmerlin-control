#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>

#include <config.h>

#include <gavl/gavl.h>
#include <gavl/state.h>
#include <gavl/log.h>
#define LOG_DOMAIN "ctrl_wireplumber"
#include <gavl/utils.h>
#include <gavl/http.h>

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/state.h>
#include <gmerlin/subprocess.h>
#include <control.h>

#define FLAG_READY       (1<<0)
#define FLAG_ERROR       (1<<1)

#define FLAG_HAVE_SINKS  (1<<2)

/* We store the volumes along with the control options */
#define META_VOLUME        "vol"
#define META_NUM_CHANNELS  "nch"

#define MSG_MAX 1024

typedef struct
  {
  bg_controllable_t ctrl;
  bg_subprocess_t * monitor;
  gavl_io_t * monitor_io;

  //  char * line_buf;
  //  int line_alloc;

  gavl_buffer_t buf;
  
  /* Wireplumber */
  
  int flags;
  int num_ops;
  
  gavl_dictionary_t state;

  gavl_array_t sinks;

  /* Control */
  gavl_dictionary_t * default_sink;
  
  char * default_sink_name;
  
  //  int sink_idx;
  //  int source_idx;

  
  } wireplumber_t;

static void get_controls_wireplumber(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  wireplumber_t * wireplumber = priv;
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_SLIDER,
                                  "master-volume",
                                  "Master volume");

  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MIN, 0);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MAX, 100);
  //  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_VALUE, 20);
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);

  wireplumber->default_sink = gavl_control_add_control(parent,
                                                       GAVL_META_CLASS_CONTROL_PULLDOWN,
                                                       "default-sink",
                                                       "Default ouptut");
  }

static int handle_msg_wireplumber(void * priv, gavl_msg_t * msg)
  {
  wireplumber_t * wireplumber = priv;
  char * command;
  
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
          gavl_dictionary_t * opt;
          int last = 0;
          
          gavl_value_init(&val);
          
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);
          
          if(!strcmp(var, "master-volume"))
            {
            double volume;
            gavl_value_get_float(&val, &volume);
            //            volume = pow(volume / 100.0, 1.0 / 3.0);

            volume /= 100.0;
            
            //            fprintf(stderr, "Set volume %f\n", volume);
            command = gavl_sprintf("wpctl set-volume @DEFAULT_SINK@ %f", volume);
            bg_system(command);
            free(command);
            }
          else if(!strcmp(var, "default-sink"))
            {
#if 1
            if((opt = gavl_control_get_option(wireplumber->default_sink, GAVL_META_ID, gavl_value_get_string(&val))))
              {
              command = gavl_sprintf("wpctl set-default %s", gavl_dictionary_get_string(opt, GAVL_META_ID));
              bg_system(command);
              free(command);
              }
            
#endif
            
            }
          
          gavl_value_free(&val);
          
          }
        }
    }

  return 1;
  }

static void handle_message(wireplumber_t * wireplumber,
                           const char * msg,
                           const gavl_dictionary_t * dict)
  {
#if 0
  fprintf(stderr, "handle_message: %s\n", msg);
  gavl_dictionary_dump(dict, 2);
  fprintf(stderr, "\n");
#endif

  if(!strcmp(msg, "!DelNode"))
    {
    const char * id = gavl_dictionary_get_string(dict, GAVL_META_ID);
    gavl_control_delete_option(wireplumber->default_sink, id);
    }
  else if(!strcmp(msg, "!AddNode"))
    {
    const char * id = gavl_dictionary_get_string(dict, GAVL_META_ID);
    const char * label = gavl_dictionary_get_string(dict, GAVL_META_LABEL);
    gavl_control_add_option(wireplumber->default_sink, id, label);
    }
  else if(!strcmp(msg, "!DefaultAudioSink"))
    {
    bg_state_set(&wireplumber->state, 1, NULL, "default-sink", 
                 gavl_dictionary_get(dict, GAVL_META_ID),
                 wireplumber->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    }
  else if(!strcmp(msg, "!DefaultAudioVolume"))
    {
    double volume = 0.0;
    gavl_value_t volume_val;

    gavl_dictionary_get_float(dict, "Volume", &volume);

    gavl_value_init(&volume_val);
    gavl_value_set_float(&volume_val, volume * 100.0);
      
    bg_state_set(&wireplumber->state, 1, NULL, "master-volume",
                 &volume_val,
                 wireplumber->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    gavl_value_free(&volume_val);
    }


  }

static int update_wireplumber(void * priv)
  {
  int ret = 0;
  int result;
  wireplumber_t * wireplumber = priv;
  gavl_dictionary_t dict;
  
  gavl_dictionary_init(&dict);
  
  while(1)
    {
    result = gavl_io_read_data_nonblock(wireplumber->monitor_io,
                                        wireplumber->buf.buf + wireplumber->buf.len,
                                        MSG_MAX - 1 - wireplumber->buf.len);

    if(result < 0)
      {
      /* Error */
      fprintf(stderr, "Read failed\n%s", strerror(errno));
      return ret;
      }
    else if(!result)
      {
      /* Nothing to read */
      return ret;
      }
    else if(result > 0)
      {
      /* Got data */
      wireplumber->buf.len += result;
      wireplumber->buf.buf[wireplumber->buf.len] = '\0';
      }

    while(1)
      {
      int i;
      /* Got message */
      char ** lines;
      char * end;
      
      if(!(end = strstr((char*)wireplumber->buf.buf, "\n\n")) &&
         !(end = strstr((char*)wireplumber->buf.buf, "\r\n\r\n")))
        return ret;
      *end = '\0';
      
      //      fprintf(stderr, "Got message:\n%s", wireplumber->buf.buf);

      lines = gavl_strbreak((char*)wireplumber->buf.buf, '\n');
      i = 0;
      while(lines[i])
        {
        gavl_strtrim(lines[i]);

        if(i > 0)
          gavl_http_parse_vars_line(&dict, lines[i]);
        i++;
        }

      handle_message(wireplumber, lines[0], &dict);
    
      gavl_strbreak_free(lines);
      gavl_dictionary_reset(&dict);

      end++;

      while(isspace(*end) && (*end != '\0'))
        end++;

      gavl_buffer_flush(&wireplumber->buf, end - (char*)wireplumber->buf.buf);
      wireplumber->buf.buf[wireplumber->buf.len] = '\0';
      }
    
#if 0
    while(1)
      {
      /* Read variables */
      if(!gavl_io_read_line(wireplumber->monitor_io, &wireplumber->line_buf,
                            &wireplumber->line_alloc, 1024))
        break;

      if(*wireplumber->line_buf == '\0')
        break;
      
      if(!gavl_http_parse_vars_line(&dict, wireplumber->line_buf))
        break;
      }

#endif
    
    //    free(msg);
    gavl_dictionary_reset(&dict);
    ret++;
    }
  
  return ret;
  }

static void * create_wireplumber()
  {
  wireplumber_t * wireplumber = calloc(1, sizeof(*wireplumber));

  bg_controllable_init(&wireplumber->ctrl,
                       bg_msg_sink_create(handle_msg_wireplumber, wireplumber, 1),
                       bg_msg_hub_create(1));

  
  gavl_buffer_alloc(&wireplumber->buf, MSG_MAX);
  wireplumber->buf.buf[0] = '\0';
  
  return wireplumber;
  }

static void destroy_wireplumber(void *priv)
  {
  wireplumber_t * wireplumber = priv;

  if(wireplumber->monitor_io)
    gavl_io_destroy(wireplumber->monitor_io);

  // TODO: Kill?
  if(wireplumber->monitor)
    bg_subprocess_close(wireplumber->monitor);
  
  bg_controllable_cleanup(&wireplumber->ctrl);
  
  if(wireplumber->default_sink_name)
    free(wireplumber->default_sink_name);
  
  free(wireplumber);
  }
  
static int open_wireplumber(void *priv, const char * uri)
  {
  char * command;
  wireplumber_t * wireplumber = priv;

  command = gavl_sprintf("wpexec "DATA_DIR"/wireplumber_monitor.lua");
  
  if(!(wireplumber->monitor = bg_subprocess_create(command, 0, 1, 0)))
    {
    free(command);
    return 0;
    }

  free(command);
  if(!(wireplumber->monitor_io = gavl_io_create_fd(wireplumber->monitor->stdout_fd, 0, 0)))
    return 0;
  
  return 1;
  }

static bg_controllable_t * get_controllable_wireplumber(void * priv)
  {
  wireplumber_t * wireplumber = priv;
  
  return &wireplumber->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_wireplumber",
    .long_name = TRS("Wireplumber"),
    .description = TRS("Control volume and default audio sink via wireplumber"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_wireplumber,
    .destroy =   destroy_wireplumber,
    .get_controllable =   get_controllable_wireplumber,
    .priority =         1,
    },
  
  .protocols = "wireplumber",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_wireplumber,
  .open   = open_wireplumber,
  .get_controls   = get_controls_wireplumber,
  
  };

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
