#include <string.h>
#include <unistd.h>

#include <pulse/pulseaudio.h>

#include <config.h>

#include <gavl/gavl.h>
#include <gavl/state.h>
#include <gavl/log.h>
#define LOG_DOMAIN "ctrl_pulse"

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <gmerlin/state.h>
#include <control.h>

#define FLAG_READY       (1<<0)
#define FLAG_ERROR       (1<<1)

#define FLAG_HAVE_SINKS  (1<<2)

/* We store the volumes along with the control options */
#define META_VOLUME        "vol"
#define META_NUM_CHANNELS  "nch"

typedef struct
  {
  bg_controllable_t ctrl;
  
  /* Pulseaudio */
  pa_mainloop *pa_ml;
  pa_operation *pa_op;
  pa_context *pa_ctx;
  int flags;

  int num_ops;

  
  gavl_dictionary_t state;

  gavl_array_t sinks;

  /* Control */
  gavl_dictionary_t * default_sink;
  
  char * default_sink_name;
  
  //  int sink_idx;
  //  int source_idx;

  
  } pulse_t;

// This callback gets called when our context changes state.  We really only
// care about when it's ready or if it has failed
static void pa_state_cb(pa_context *c, void *userdata)
  {
  pa_context_state_t state;
  pulse_t * p = userdata;
  
  state = pa_context_get_state(c);
  switch(state)
    {
    // There are just here for reference
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    default:
      break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
      p->flags |= FLAG_ERROR;
      break;
    case PA_CONTEXT_READY:
      p->flags |= FLAG_READY;
      break;
    }
  }

static void delete_device(pulse_t * pulse, const char * id)
  {
  
  }

/* Callbacks for volume and stuff */

static void pa_sink_cb(pa_context *c, const pa_sink_info *l, int eol, void *userdata)
  {
  gavl_dictionary_t * opt;
  pulse_t * pulse = userdata;

  
  if(l)
    {
    double volume;
    char * id = gavl_sprintf("%d", l->index);
    
    if(!(opt = gavl_control_get_option(pulse->default_sink, GAVL_META_ID, id)))
      {
      //  fprintf(stderr, "Sink added %s\n", l->description);
      opt = gavl_control_add_option(pulse->default_sink, id, l->description);
      gavl_dictionary_set_string(opt, GAVL_META_URI, l->name);
      gavl_dictionary_set_int(opt, META_NUM_CHANNELS, l->volume.channels);
      }

    volume = 100.0 * pa_sw_volume_to_linear(pa_cvolume_avg(&l->volume));
    gavl_dictionary_set_float(opt, META_VOLUME, volume);

    if(pulse->default_sink_name &&
       !strcmp(pulse->default_sink_name, l->name))
      {
      bg_state_set(&pulse->state, 1, NULL, "master-volume",
                   gavl_dictionary_get(opt, META_VOLUME),
                   pulse->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
      }
    
    fprintf(stderr, "Sink volume: %f\n",
            100.0 * pa_sw_volume_to_linear(pa_cvolume_avg(&l->volume)));

    free(id);
    }
  }

static void pa_server_cb(pa_context *c, const pa_server_info *l, void *userdata)
  {
  //  gavl_value_t val;
  const gavl_dictionary_t * opt;
  pulse_t * pulse = userdata;
  
  fprintf(stderr, "Server changed\n");
  fprintf(stderr, "Default sink: %s\n", l->default_sink_name);
  
  pulse->default_sink_name = gavl_strrep(pulse->default_sink_name, l->default_sink_name);
  

  if((opt = gavl_control_get_option(pulse->default_sink, GAVL_META_URI, l->default_sink_name)))
    {
    bg_state_set(&pulse->state, 1, NULL, "default-sink", 
                 gavl_dictionary_get(opt, GAVL_META_ID),
                 pulse->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    }
  
  }

static void get_controls_pulse(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  pulse_t * pulse = priv;
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_SLIDER,
                                  "master-volume",
                                  "Master volume");

  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MIN, 0);
  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_MAX, 100);
  //  gavl_dictionary_set_int(ctrl, GAVL_CONTROL_VALUE, 20);
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);

  pulse->default_sink = gavl_control_add_control(parent,
                                                 GAVL_META_CLASS_CONTROL_PULLDOWN,
                                                 "default-sink",
                                                 "Default ouptut");
  
  }

static int handle_msg_pulse(void * priv, gavl_msg_t * msg)
  {
  pulse_t * pulse = priv;

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
            int num_channels;
            double volume;
            
            
            if(pulse->default_sink_name &&
               (opt = gavl_control_get_option(pulse->default_sink, GAVL_META_URI,
                                              pulse->default_sink_name)) &&
               gavl_dictionary_get_int(opt, META_NUM_CHANNELS, &num_channels) &&
               gavl_value_get_float(&val, &volume))
              {
              int i;
              pa_cvolume vol;
              pa_operation * op;
              volume /= 100.0;
              
              memset(&vol, 0, sizeof(vol));
              
              vol.channels = num_channels;
              for(i = 0; i < vol.channels; i++)
                {
                vol.values[i] = pa_sw_volume_from_linear(volume);
                }
              op = pa_context_set_sink_volume_by_name(pulse->pa_ctx,
                                                      pulse->default_sink_name,
                                                      &vol,NULL, NULL);
              pa_operation_unref(op);
              
              //              fprintf(stderr, "Master volume\n");
              
              
              }
            
            }
          else if(!strcmp(var, "default-sink"))
            {
            
            if((opt = gavl_control_get_option(pulse->default_sink, GAVL_META_ID, gavl_value_get_string(&val))))
              {
              pa_operation * op =
                pa_context_set_default_sink(pulse->pa_ctx,
                                            gavl_dictionary_get_string(opt, GAVL_META_URI),
                                            NULL, NULL);
              pa_operation_unref(op);
              }
            
            fprintf(stderr, "Default output %s\n", gavl_value_get_string(&val));
            
            }
          
          gavl_value_free(&val);
          
          }
        }
    }

  return 1;
  }


static int update_pulse(void * priv)
  {
  int ret = 0;
  pulse_t * pulse = priv;
  
  pa_mainloop_iterate(pulse->pa_ml, 0, NULL);

  if(pulse->pa_op && (pa_operation_get_state(pulse->pa_op) == PA_OPERATION_DONE))
    {
    pa_operation_unref(pulse->pa_op);
    pulse->pa_op = NULL;
    ret++;

    if(!(pulse->flags & FLAG_HAVE_SINKS))
      {
      pulse->flags |= FLAG_HAVE_SINKS;

      /* Got sinks, now query server */
      pulse->pa_op = pa_context_get_server_info(pulse->pa_ctx,
                                                pa_server_cb,
                                                pulse);
      
      }
    else
      {
      
      }
    
    }
    
  return ret;
  }

static void * create_pulse()
  {
  pulse_t * pulse = calloc(1, sizeof(*pulse));

  bg_controllable_init(&pulse->ctrl,
                       bg_msg_sink_create(handle_msg_pulse, pulse, 1),
                       bg_msg_hub_create(1));

  return pulse;
  }



static void pa_subscribe_callback(pa_context *c,
                                  pa_subscription_event_type_t type,
                                  uint32_t idx, void *userdata)
  {
  pulse_t * pulse = userdata;
  //  else if((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK)
  // return; // We handle only sources and sinks

  switch(type & PA_SUBSCRIPTION_EVENT_TYPE_MASK)
    {
#if 1
    case PA_SUBSCRIPTION_EVENT_NEW:
      {
      /* Sink added */

      if((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK)
        {
        /* Sink removed */
        fprintf(stderr, "Sink removed\n");
        }
      
      break;
      }
    case PA_SUBSCRIPTION_EVENT_REMOVE:
      {

      if((type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK)
        {
        /* Sink removed */
        char * id;
        fprintf(stderr, "Sink removed\n");

        id = gavl_sprintf("%d", idx);
        gavl_control_delete_option(pulse->default_sink, id);
        free(id);
        }
      
      break;
      }
#endif
    case PA_SUBSCRIPTION_EVENT_CHANGE:
      {
      pa_operation *op = NULL;

      switch(type & PA_SUBSCRIPTION_EVENT_FACILITY_MASK)
        {
#if 0
        case PA_SUBSCRIPTION_EVENT_SOURCE:
          //          fprintf(stderr, "Source...");
          op = pa_context_get_source_info_by_index(c, idx, pa_source_cb, userdata);
          break;
#endif
        case PA_SUBSCRIPTION_EVENT_SINK:
          //          fprintf(stderr, "Sink...");
          op = pa_context_get_sink_info_by_index(c, idx, pa_sink_cb, userdata);
          break;
        case PA_SUBSCRIPTION_EVENT_SERVER:
          //          fprintf(stderr, "Server...");
          op = pa_context_get_server_info(c, pa_server_cb, userdata);
          break;
        }

      if(op)
        pa_operation_unref(op);
      
      break;
      }
    default:
      {
      fprintf(stderr, "unknown action %d\n", type & PA_SUBSCRIPTION_EVENT_TYPE_MASK);
      break;
      }
    }

  
  
  }

static void destroy_pulse(void *priv)
  {
  pulse_t * pulse = priv;

  if(pulse->pa_op)
    pa_operation_unref(pulse->pa_op);

  if(pulse->pa_ctx)
    {
    pa_context_disconnect(pulse->pa_ctx);
    pa_context_unref(pulse->pa_ctx);
    }
  if(pulse->pa_ml)
    pa_mainloop_free(pulse->pa_ml);
 
  bg_controllable_cleanup(&pulse->ctrl);

  if(pulse->default_sink_name)
    free(pulse->default_sink_name);
  
  free(pulse);
  }

static int open_pulse(void *priv, const char * uri)
  {
  pulse_t * pulse = priv;
  
  pa_mainloop_api *pa_mlapi;
  
    // Create a mainloop API and connection to the default server
  pulse->pa_ml = pa_mainloop_new();
  
  pa_mlapi = pa_mainloop_get_api(pulse->pa_ml);
  pulse->pa_ctx = pa_context_new(pa_mlapi, "gmerlin-pulseaudio-devices");
  
  // This function connects to the pulse server
  if(pa_context_connect(pulse->pa_ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL) < 0)
    {
    pulse->flags |= FLAG_ERROR;
    }
  else
    {
    // This function defines a callback so the server will tell us it's state.
    // Our callback will wait for the state to be ready.  The callback will
    // modify the variable to 1 so we know when we have a connection and it's
    // ready.
    // If there's an error, the callback will set pa_ready to 2
    pa_context_set_state_callback(pulse->pa_ctx, pa_state_cb, pulse);

    // Now we'll enter into an infinite loop until we get the data we receive
    // or if there's an error
    for (;;)
      {
      // We can't do anything until PA is ready, so just iterate the mainloop
      // and continue
      if(!(pulse->flags & (FLAG_READY | FLAG_ERROR)))
        {
        pa_mainloop_iterate(pulse->pa_ml, 1, NULL);
        continue;
        }
      // We couldn't get a connection to the server, so exit out
      else
        break;
      }
    }

  if(!(pulse->flags &FLAG_READY))
    {
    gavl_log(GAVL_LOG_WARNING, LOG_DOMAIN, "Connection to pulseaudio failed");
    //    pa_context_disconnect(ret->pa_ctx);
    pa_context_unref(pulse->pa_ctx);
    pa_mainloop_free(pulse->pa_ml);
    pulse->pa_ctx = NULL;
    pulse->pa_ml  = NULL;
    return 0;
    }

  pulse->pa_op = pa_context_get_sink_info_list(pulse->pa_ctx, pa_sink_cb, pulse);

  pa_context_set_subscribe_callback(pulse->pa_ctx, pa_subscribe_callback, pulse);
  pa_context_subscribe(pulse->pa_ctx,
                       PA_SUBSCRIPTION_MASK_SINK |
                       PA_SUBSCRIPTION_MASK_SERVER , NULL, NULL);
  
  
  
  return 1;
  
  }

static bg_controllable_t * get_controllable_pulse(void * priv)
  {
  pulse_t * pulse = priv;
  
  return &pulse->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_pulse",
    .long_name = TRS("Pulseaudio"),
    .description = TRS("Control the pulseaudio server"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_pulse,
    .destroy =   destroy_pulse,
    .get_controllable =   get_controllable_pulse,
    .priority =         1,
    },
  
  .protocols = "pulse",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_pulse,
  .open   = open_pulse,
  .get_controls   = get_controls_pulse,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
