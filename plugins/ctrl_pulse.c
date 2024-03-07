#include <pulse/pulseaudio.h>


#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <control.h>

#define FLAG_READY       (1<<0)
#define FLAG_ERROR       (1<<1)
#define FLAG_GOT_SOURCES (1<<2)
#define FLAG_GOT_SINKS   (1<<3)


typedef struct
  {
  bg_controllable_t ctrl;
  
  /* Pulseaudio */
  pa_mainloop *pa_ml;
  pa_operation *pa_op;
  pa_context *pa_ctx;
  int flags;

  int num_ops;
  
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

static void get_controls_pulse(void * priv, gavl_dictionary_t * parent)
  {
  pulse_t * pulse = priv;
  }

static int handle_msg_pulse(void * priv, gavl_msg_t * msg)
  {
  pulse_t * pulse = priv;
  
  }

static int update_pulse(void * priv)
  {
  pulse_t * pulse = priv;
  
  }

static void * create_pulse()
  {
  pulse_t * pulse = calloc(1, sizeof(*pulse));
  return pulse;
  }

static void destroy_pulse(void *priv)
  {
  pulse_t * pulse = priv;

  bg_controllable_cleanup(&pulse->ctrl);
  
  free(pulse);
  }

static int open_pulse(void *priv, const char * uri)
  {
  pulse_t * pulse = priv;

  
  
  
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
