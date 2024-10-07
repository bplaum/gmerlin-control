#include <string.h>
#include <ctype.h>


#include <config.h>

#include <gavl/gavlsocket.h>
#include <gavl/utils.h>

#include <gmerlin/translation.h>
#include <gmerlin/bgmsg.h>
#include <gmerlin/state.h>

#include <gmerlin/plugin.h>
#include <control.h>


#define STATUS_OFFLINE 0
#define STATUS_IDLE    1
#define STATUS_LOOKUP  2
#define STATUS_CONNECT 3
// #define STATUS_COMMAND 4

#define BUF_SIZE 256

#define LOOKUP_TIMEOUT (GAVL_TIME_SCALE*3)
#define CONNECT_TIMEOUT (GAVL_TIME_SCALE*3)
#define COMMAND_TIMEOUT (GAVL_TIME_SCALE*3)


typedef struct
  {
  gavl_dictionary_t state;
  gavl_io_t * io;
  int fd;
  bg_controllable_t ctrl;

  char line_buf[BUF_SIZE];
  int line_buf_len;

  gavl_buffer_t send_buf;
  gavl_time_t start_time;

  int status;
  gavl_socket_address_t * addr;
  
  char * host;
  } marantz_t;

static void set_status(marantz_t * m, int status)
  {
  m->start_time = gavl_time_get_monotonic();
  m->status = status;

  if(m->status == STATUS_OFFLINE)
    {
    if(m->fd >= 0)
      {
      gavl_socket_close(m->fd);
      m->fd = -1;
      }
    if(m->io)
      {
      gavl_io_destroy(m->io);
      m->io = NULL;
      }
    gavl_control_set_online(m->ctrl.evt_sink, "/", 0);
    }
  else if(m->status == STATUS_IDLE)
    gavl_control_set_online(m->ctrl.evt_sink, "/", 1);
  }

static void queue_msg(marantz_t * m, const char * cmd)
  {
  //  fprintf(stderr, "Queueing message %s (status: %d)\n", cmd, m->status);
  gavl_buffer_append_data(&m->send_buf, (uint8_t*)cmd, strlen(cmd));
  }

static int flush_msg(marantz_t * m)
  {
  char * end;
  int result;
  if(!m->send_buf.len ||
     !gavl_io_can_write(m->io, 0))
    return 0;

  /* We only flush to the next "\r" */
  if(!(end = strchr((char*)m->send_buf.buf, '\r')))
    {
    gavl_buffer_reset(&m->send_buf);
    return 1;
    }
  end++;
  
  result = gavl_io_write_data_nonblock(m->io, m->send_buf.buf, end - (char*)m->send_buf.buf);

#if 0
  if(result == end - (char*)m->send_buf.buf)
    {
    fprintf(stderr, "Send message:\n");
    gavl_hexdump(m->send_buf.buf, result, 16);
    }
#endif
  
  if(result > 0)
    gavl_buffer_flush(&m->send_buf, result);
  else if(result < 0)
    {
    /* Error */
    }
  
  return 1;
  }

static double parse_volume(const char * pos)
  {
  double ret;
  if(strlen(pos) == 2)
    {
    ret = (double)atoi(pos);
    }
  else if(strlen(pos) == 3)
    {
    ret = (double)atoi(pos) / 10.0;
    }
  else
    ret = -1.0; // Error

  //  fprintf(stderr, "Parse volume: %f\n", ret);
  //  gavl_hexdump((uint8_t*)pos, strlen(pos), 16);
  
  return ret;
  }

static void handle_marantz_msg(marantz_t * m)
  {
  char * pos;
  gavl_value_t val;
  const char * var = NULL;
  gavl_value_init(&val);

  if(gavl_string_starts_with(m->line_buf, "MV") && isdigit(m->line_buf[2]))
    {
    pos = &m->line_buf[2];
    gavl_value_set_float(&val, parse_volume(pos));
    var = "volume";
    }
  else if(gavl_string_starts_with(m->line_buf, "SI"))
    {
    pos = &m->line_buf[2];
    gavl_value_set_string(&val, pos);
    var = "source";
    }
  else if(gavl_string_starts_with(m->line_buf, "MS"))
    {
    pos = &m->line_buf[2];
    gavl_value_set_string(&val, pos);
    var = "srmode";
    }
  else if(gavl_string_starts_with(m->line_buf, "ZM"))
    {
    pos = &m->line_buf[2];
    
    if(!strcmp(pos, "ON"))
      gavl_value_set_int(&val, 1);
    else
      gavl_value_set_int(&val, 0);
    var = "power";
    }
  
  /* Send message */

  if(var)
    {
    bg_state_set(&m->state, 1, NULL, var,
                 &val, m->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    }
  
  gavl_value_free(&val);
  }
  
static int read_msg(marantz_t * m)
  {
  int ret = 0;
  int bytes;
  char * end;
  
  /* Read as many messages as possible */
  while(1)
    {
    if(!gavl_io_can_read(m->io, 0))
      return ret;

    bytes = gavl_io_read_data_nonblock(m->io, (uint8_t*)&m->line_buf[m->line_buf_len], BUF_SIZE - 1 - m->line_buf_len);

    if(bytes <= 0)
      {
      /* Disconnected */
      return -1;
      }

    ret++;
    
    m->line_buf_len += bytes;
    m->line_buf[m->line_buf_len] = '\0';

    if((end = strchr(m->line_buf, '\r')))
      {
      *end = '\0';
      //      fprintf(stderr, "Got message: %s\n", m->line_buf);
      handle_marantz_msg(m);

#if 0      
      if(m->status == STATUS_COMMAND)
        set_status(m, STATUS_IDLE);
#endif 
      end++;
      if(m->line_buf_len > (end - m->line_buf))
        memmove(&m->line_buf, end, m->line_buf_len - (end - m->line_buf));
      
      m->line_buf_len -= (end - m->line_buf);
      }
    }
  return ret;
  }

static int update_marantz(void * priv)
  {
  int ret = 0;
  int result;
  marantz_t * m = priv;

  switch(m->status)
    {
    case STATUS_OFFLINE:
      if(gavl_time_get_monotonic() - m->start_time > 2 * GAVL_TIME_SCALE)
        {
        /* Try to reconnect */
        m->fd = gavl_socket_connect_inet(m->addr, 0);
        set_status(m, STATUS_CONNECT);
        ret++;
        }
      break;
    case STATUS_IDLE:
      {
      gavl_time_t cur = gavl_time_get_monotonic();
      if(cur - m->start_time > 2 * GAVL_TIME_SCALE)
        {
        queue_msg(m, "MS?\r");
        queue_msg(m, "SI?\r");
        m->start_time = cur;
        }
      
      result = read_msg(m);

      if(result < 0)
        {
        set_status(m, STATUS_OFFLINE);
        return 1;
        }
      
      ret += result;
      
      ret += flush_msg(m);
      }
      break;
    case STATUS_LOOKUP:
      result = gavl_socket_address_set_async_done(m->addr, 0);
      if(!result)
        {
        if(gavl_time_get_monotonic() - m->start_time > LOOKUP_TIMEOUT)
          {
          /* Lookup timed out */
          }
        }
      else if(result > 0)
        {
        /* Lookup complete, connect */
        m->fd = gavl_socket_connect_inet(m->addr, 0);
        set_status(m, STATUS_CONNECT);
        }
      else if(result < 0)
        {
        /* TODO: Error */
        }
      break;
    case STATUS_CONNECT:

      result = gavl_socket_connect_inet_complete(m->fd, 0);

      if(result > 0)
        {
        /* Connected */
        //        fprintf(stderr, "Marantz connected\n");
        m->io = gavl_io_create_socket(m->fd, 0, GAVL_IO_SOCKET_DO_CLOSE);
        m->fd = -1;
        set_status(m, STATUS_IDLE);
        }
      else if(result < 0)
        {
        
        }
      else
        {
        if(gavl_time_get_monotonic() - m->start_time > CONNECT_TIMEOUT)
          {
          /* Connect timed out */
          }
        
        }
      break;
    }
  
  return ret;
  }

static int handle_msg_marantz(void * priv, gavl_msg_t * msg)
  {
  marantz_t * m = priv;

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
          //          int delay = 0;

          int last = 0;
          
          gavl_value_init(&val);
          
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          //          fprintf(stderr, "Set value: %s\n", var);

          if(!strcmp(var, "volume"))
            {
            int val_i;
            if(gavl_value_get_int(&val, &val_i))
              {
              if(val_i > 0)
                queue_msg(m, "MVUP\r");
              else if(val_i < 0)
                queue_msg(m, "MVDOWN\r");
              }
            }
          else if(!strcmp(var, "power"))
            {
            int val_i = 0;
            if(gavl_value_get_int(&val, &val_i))
              {
              fprintf(stderr, "Got power %d\n", val_i);
              if(val_i)
                queue_msg(m, "ZMON\r");
              else
                queue_msg(m, "ZMOFF\r");
              }
            
            }
          else if(!strcmp(var, "source"))
            {
            char * cmd = gavl_sprintf("SI%s\r", gavl_value_get_string((&val)));
            queue_msg(m, cmd);
            free(cmd);
            }
          else if(!strcmp(var, "srmode"))
            {
            char * cmd = gavl_sprintf("MS%s\r", gavl_value_get_string((&val)));
            queue_msg(m, cmd);
            free(cmd);
            }
          }
        }
    }
  return 1;
  }


static void * create_marantz()
  {
  marantz_t * m;
  m = calloc(1, sizeof(*m));
  m->fd = -1;

  bg_controllable_init(&m->ctrl,
                       bg_msg_sink_create(handle_msg_marantz, m, 1),
                       bg_msg_hub_create(1));
  
  return m;
  }

static void destroy_marantz(void *priv)
  {
  marantz_t * m = priv;

  if(m->io)
    gavl_io_destroy(m->io);

  if(m->fd >= 0)
    gavl_socket_close(m->fd);
  
  gavl_dictionary_free(&m->state);

  if(m->host)
    free(m->host);
  
  free(m);
  }

static int open_marantz(void *priv, const char * uri)
  {
  marantz_t * m = priv;

  m->addr = gavl_socket_address_create();

  if(!gavl_url_split(uri, NULL, NULL, NULL, &m->host, NULL, NULL))
    return 0;
  
  if(!gavl_socket_address_set_async(m->addr, m->host,
                                    23, SOCK_STREAM))
    return 0;
  
  /* Queue initial poll commands */
  queue_msg(m, "ZM?\r"); // Zone 1 power
  queue_msg(m, "MV?\r"); // Master volume
  queue_msg(m, "SI?\r"); // Source
  queue_msg(m, "MS?\r"); // Surround mode?
  
  set_status(m, STATUS_LOOKUP);  
  return 1;
  }

static void get_controls_marantz(void * priv, gavl_dictionary_t * parent)
  {
  marantz_t * m = priv;
  gavl_dictionary_t * ctrl;
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_POWERBUTTON,
                                  "power",
                                  "Power");
  
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_VOLUME,
                                  "volume",
                                  "Volume");
  
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);

  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX, 98.0);

  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_PULLDOWN,
                                  "source",
                                  "Source");

  gavl_control_add_option(ctrl, "SAT/CBL", "Cable/SAT");
  gavl_control_add_option(ctrl, "DVD", "DVD");
  gavl_control_add_option(ctrl, "BD", "Bluray");

  gavl_control_add_option(ctrl, "GAME", "Game");
  gavl_control_add_option(ctrl, "AUX1", "AUX1");
  gavl_control_add_option(ctrl, "MPLAY", "Media player");

  gavl_control_add_option(ctrl, "TV", "TV");
  gavl_control_add_option(ctrl, "AUX2", "AUX2");
  gavl_control_add_option(ctrl, "TUNER", "Tuner");

  gavl_control_add_option(ctrl, "CD", "CD");
  gavl_control_add_option(ctrl, "BT", "Bluetooth");
  //  gavl_control_add_option(ctrl, "CD", "CD");
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_PULLDOWN,
                                  "srmode",
                                  "Surround mode");

  gavl_control_add_option(ctrl, "DIRECT",        "Direct");
  gavl_control_add_option(ctrl, "PURE DIRECT",   "Pure Direct");
  gavl_control_add_option(ctrl, "STEREO",        "Stereo");
  gavl_control_add_option(ctrl, "MULTI CH IN",   "Multichannel input");
  gavl_control_add_option(ctrl, "MCH STEREO",    "Multichannel Stereo");
  gavl_control_add_option(ctrl, "M CH IN+DS",    "Multi in + Dolby Surround");

  gavl_control_add_option(ctrl, "M CH IN+NEURAL:X", "5/7 Channel + Neural:X");
  gavl_control_add_option(ctrl, "VIRTUAL",       "Virtual");
  gavl_control_add_option(ctrl, "MOVIE",         "Movie");
  gavl_control_add_option(ctrl, "MUSIC",         "Music");
  gavl_control_add_option(ctrl, "ROCK ARENA",    "Rock Arena");
  gavl_control_add_option(ctrl, "JAZZ CLUB",     "Jazz Club");
  gavl_control_add_option(ctrl, "MONO MOVIE",    "Mono Movie");
  gavl_control_add_option(ctrl, "STANDARD",      "Standard");
  gavl_control_add_option(ctrl, "MATRIX",        "Matrix");

  /* Web interface */
  
  ctrl = gavl_control_add_control(parent,
                                  GAVL_META_CLASS_CONTROL_LINK,
                                  "web",
                                  "Web interface");
  gavl_dictionary_set_string_nocopy(ctrl, GAVL_META_URI, gavl_sprintf("http://%s", m->host));
  
  }

  
static bg_controllable_t * get_controllable_marantz(void * priv)
  {
  marantz_t * m = priv;
  return &m->ctrl;
  
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_marantz",
    .long_name = TRS("Marantz Amplifier"),
    .description = TRS("Control a marantz or denon amplifier"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_marantz,
    .destroy =   destroy_marantz,
    .get_controllable =   get_controllable_marantz,
    .priority =         1,
    },
  
  .protocols = "marantz",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_marantz,
  .open   = open_marantz,
  .get_controls   = get_controls_marantz,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
