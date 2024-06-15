#include <time.h>
#include <string.h>

#include <config.h>

#include <gavl/numptr.h>

#include <gmerlin/translation.h>
#include <gmerlin/plugin.h>
#include <control.h>

#include <gmerlin/state.h>

#include <gavl/http.h>
#include <gavl/log.h>
#define LOG_DOMAIN "energenie"
#include <gavl/utils.h>

#define STATE_IDLE      0
#define STATE_POLL      1
#define STATE_LABELS    2
#define STATE_COMMAND   3
#define STATE_OFFLINE   4

#define FLAG_LOGGED_IN   (1<<0)
#define FLAG_HAVE_LABELS  (1<<1)

typedef struct
  {
  gavl_io_t * io;
  bg_controllable_t ctrl;
  gavl_time_t last_poll_time;
  char * addr;
  char * password;
  
  int status;
  gavl_dictionary_t state;
  gavl_msg_t * cmd;

  gavl_buffer_t req_body;
  gavl_buffer_t res_body;

  int flags;
  
  } energenie_t;

static void get_controls_energenie(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * c;

  c = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_POWERBUTTON,
                               "s1", "Socket 1");
  gavl_dictionary_set_int(c, GAVL_CONTROL_VALUE, 0);
  
  c = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_POWERBUTTON,
                               "s2", "Socket 2");
  gavl_dictionary_set_int(c, GAVL_CONTROL_VALUE, 0);
  
  c = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_POWERBUTTON,
                               "s3", "Socket 3");
  gavl_dictionary_set_int(c, GAVL_CONTROL_VALUE, 0);
  
  c = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_POWERBUTTON,
                               "s4", "Socket 4");
  gavl_dictionary_set_int(c, GAVL_CONTROL_VALUE, 0);
  
  }

static void reset_req_body(energenie_t * e)
  {
  if(e->req_body.buf)
    free(e->req_body.buf);
  gavl_buffer_init(&e->req_body);
  }

static void reset_connection(energenie_t * e)
  {
  if(e->io)
    {
    gavl_io_destroy(e->io);
    e->io = NULL;
    }
  
  }


static void ensure_http_client(energenie_t * e)
  {
  if(!e->io)
    {
    e->io = gavl_http_client_create();
    gavl_http_client_set_response_body(e->io, &e->res_body);
    }
  }

static int start_login(energenie_t * e)
  {
  int ret;
  gavl_dictionary_t vars;
  char * uri = gavl_sprintf("%s/login.html", e->addr);

  gavl_dictionary_init(&vars);
  
  reset_req_body(e);
  
  ensure_http_client(e);
  gavl_http_client_set_request_body(e->io, &e->req_body);
  e->req_body.buf = (uint8_t*)gavl_sprintf("pw=%s", e->password);
  e->req_body.len = strlen((char*)e->req_body.buf);
  
  gavl_dictionary_set_string(&vars, "Content-Type", "application/x-www-form-urlencoded");
  gavl_http_client_set_req_vars(e->io, &vars);
  
  ret = gavl_http_client_run_async(e->io, "POST", uri);
  
  gavl_dictionary_free(&vars);
  
  free(uri);
  return ret;
  }

static void set_state(energenie_t * e, int * values)
  {
  gavl_value_t val;
  gavl_value_init(&val);
  
  gavl_value_set_int(&val, values[0]);
  bg_state_set(&e->state, 0, NULL, "s1",
               &val, e->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

  gavl_value_set_int(&val, values[1]);
  bg_state_set(&e->state, 0, NULL, "s2",
               &val, e->ctrl.evt_sink, BG_MSG_STATE_CHANGED);

  gavl_value_set_int(&val, values[2]);
  bg_state_set(&e->state, 0, NULL, "s3",
               &val, e->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  
  gavl_value_set_int(&val, values[3]);
  bg_state_set(&e->state, 1, NULL, "s4",
               &val, e->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  gavl_value_free(&val);
  e->last_poll_time = gavl_time_get_monotonic();
  
  }

static void set_offline(energenie_t * e)
  {
  /* Error */
  e->status = STATE_OFFLINE;
  reset_connection(e);
  reset_req_body(e);
  e->flags &= ~FLAG_LOGGED_IN;
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Energenie %s is now offline", e->addr);
  e->last_poll_time = gavl_time_get_monotonic();
  }

static int login_complete(energenie_t * e)
  {
  int result = gavl_http_client_run_async_done(e->io, 0);
  if(result > 0)
    {
    /* Extract the values */
    const char * pos = strstr((char*)e->res_body.buf, "sockstates = [");
    if(pos)
      {
      int states[4];
      pos += 14;

      if(sscanf(pos, "%d,%d,%d,%d", &states[0], &states[1], &states[2], &states[3]) == 4)
        {
        set_state(e, states);
        e->last_poll_time = gavl_time_get_monotonic();
        }
      }
    
    e->flags |= FLAG_LOGGED_IN;
    reset_req_body(e);
    return 1;
    }
  else if(result < 0)
    {
    set_offline(e);
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Login failed");
    return 1;
    }
  else
    return 0;
  }

static int start_logout(energenie_t * e)
  {
  int ret;
  gavl_dictionary_t vars;
  char * uri = gavl_sprintf("%s/login.html", e->addr);

  gavl_dictionary_init(&vars);
  
  ensure_http_client(e);
  gavl_http_client_set_request_body(e->io, NULL);

  gavl_dictionary_set_string(&vars, "Accept", "*/*");
  gavl_dictionary_set_string_nocopy(&vars, "Referer", gavl_sprintf("%s/", e->addr));
  
  gavl_http_client_set_req_vars(e->io, &vars);
  
  ret = gavl_http_client_run_async(e->io, "GET", uri);

  gavl_dictionary_free(&vars);
  
  free(uri);
  return ret;
  
  }

/* A schedule is defined by a hex string */

/*

  4 bytes: Current time of client (as time_t)
  1 byte flags: 0x01: switch on
                0x02: periodically
  4 bytes: Time of action (as time_t)
  1 byte: always 0xe5
  4 bytes: Loop time (0 if unused)
*/

#define SCHEDULE_LEN 14


static int start_set_socket(energenie_t * e, int socket, int state, gavl_time_t delay)
  {
  gavl_dictionary_t vars;

  gavl_dictionary_init(&vars);
  
  /* Switch immediately */
  if(delay <= 0)
    {
    gavl_http_client_set_request_body(e->io, &e->req_body);
    e->req_body.buf = (uint8_t*)gavl_sprintf("cte%d=%d", socket, state);
    e->req_body.len = strlen((char*)e->req_body.buf);
    gavl_dictionary_set_string(&vars, "Content-Type", "application/x-www-form-urlencoded");
    gavl_http_client_set_req_vars(e->io, &vars);

    }
  /* Switch with delay */
  else
    {
    uint8_t schedule_bin[SCHEDULE_LEN];
    char schedule_asc[SCHEDULE_LEN*2+1];
    time_t curtime = time(NULL);
    uint8_t * ptr;
    uint32_t u;
    char zero = 0;
    uint8_t flags = 0;
    int i;
    char * sch[4];
    
    ptr = &schedule_bin[0];

    u = (uint32_t)curtime;

    if(state)
      flags |= 0x01;
    
    
    GAVL_32LE_2_PTR(u, ptr); ptr+=4;
    *ptr = flags; ptr++;
    u = (uint32_t)(curtime + delay);
    GAVL_32LE_2_PTR(u, ptr); ptr+=4;
    *ptr = 0xe5; ptr++;
    u = 0;
    GAVL_32LE_2_PTR(u, ptr);

    for(i = 0; i < SCHEDULE_LEN; i++)
      {
      snprintf(&schedule_asc[2*i], 3, "%02x", schedule_bin[i]);
      }

    for(i = 0; i < 4; i++)
      {
      if(i + 1 == socket)
        sch[i] = schedule_asc;
      else
        sch[i] = &zero;
      }

    e->req_body.buf = (uint8_t*)gavl_sprintf("sch1=%s&sch2=%s&sch3=%s&sch4=%s", sch[0],  sch[1], sch[2], sch[3]);
    e->req_body.len = strlen((char*)e->req_body.buf);
    
    gavl_http_client_set_req_vars(e->io, &vars);
    }
  
  gavl_dictionary_free(&vars);
  return gavl_http_client_run_async(e->io, "POST", e->addr);
  }

static void set_socket(energenie_t * e, int socket, int state, gavl_time_t delay)
  {
  if(!(e->flags & FLAG_LOGGED_IN))
    {
    start_login(e);
    return;
    }
  
  start_set_socket(e, socket, state, delay);
  
  }
  
static int handle_msg(energenie_t * s)
  {
  s->status = STATE_COMMAND;
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

          int last = 0;
          
          gavl_value_init(&val);
          
          gavl_msg_get_state(s->cmd,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          if(*var == 's')
            {
            int state = 0;
            int delay = 0;
            int sock = atoi(var+1);

            if((sock < 0) || (sock > 4))
              return 1;
            
            if(gavl_value_get_int(&val, &state))
              {
              gavl_dictionary_get_int(&s->cmd->header, 
                                      GAVL_CONTROL_DELAY, &delay);
              set_socket(s, sock, state, delay);
              }
            }
          gavl_value_free(&val);
          break;
          }
        }
      break;
    }
  return 1;
  }

static const char * next_label(const char * pos, char ** ret)
  {
  const char * end;
  
  if((pos = strstr(pos, "name=\"sockname")) &&
     (pos = strstr(pos, "value=\"")) &&
     (pos = strchr(pos, '"')))
    {
    pos++;
    end = strchr(pos, '"');
    *ret = gavl_strndup(pos, end);
    return end+1;
    }
  else
    {
    *ret = NULL;
    return NULL;
    }
  }

static void update_label(energenie_t * e, const char * id, char * label)
  {
  gavl_msg_t * msg;
  gavl_dictionary_t dict;
  gavl_dictionary_init(&dict);
  gavl_dictionary_set_string_nocopy(&dict, GAVL_META_LABEL, label);
  msg = bg_msg_sink_get(e->ctrl.evt_sink);
  gavl_msg_set_id_ns(msg, GAVL_MSG_CONTROL_CHANGED, GAVL_MSG_NS_CONTROL);
  gavl_dictionary_set_string(&msg->header, GAVL_MSG_CONTEXT_ID, id);
  gavl_msg_set_arg_dictionary(msg, 0, &dict);
  bg_msg_sink_put(e->ctrl.evt_sink);
  gavl_dictionary_free(&dict);
  }

static int start_get_labels(energenie_t * e)
  {
  int result;
  gavl_dictionary_t vars;
  char * uri = gavl_sprintf("%s/name_settings.html", e->addr);

  gavl_dictionary_init(&vars);
  gavl_dictionary_set_string(&vars, "Accept", "*/*");

  gavl_http_client_set_req_vars(e->io, &vars);
  result = gavl_http_client_run_async(e->io, "GET", uri);
  
  gavl_dictionary_free(&vars);
  free(uri);
  return result;
  }

static int update_energenie(void * priv)
  {
  int result;
  int ret = 0;
  energenie_t * e = priv;
  
  switch(e->status)
    {
    case STATE_POLL:
      if(!(e->flags & FLAG_LOGGED_IN))
        {
        if(login_complete(e))
          {
          if(e->flags & FLAG_LOGGED_IN)
            {
            if(!(e->flags & FLAG_HAVE_LABELS))
              {
              e->status = STATE_LABELS;
              start_get_labels(e);
              }
            else
              e->status = STATE_IDLE;
            ret++;
            break;
            }
          else
            {
            /* Error */
            set_offline(e);
            ret++;
            break;
            }
          }
        }
      break;
    case STATE_COMMAND:
      if(!(e->flags & FLAG_LOGGED_IN))
        {
        if(login_complete(e))
          {
          if(e->flags & FLAG_LOGGED_IN)
            {
            handle_msg(e);
            ret++;
            }
          else
            {
            /* Error */
            set_offline(e);
            ret++;
            break;
            }
          }
        
        else
          {
          break;
          }
        }

      if(e->flags & FLAG_LOGGED_IN)
        {
        result = gavl_http_client_run_async_done(e->io, 0);
        
        if(result && e->cmd)
          {
          bg_msg_sink_done_read(e->ctrl.cmd_sink, e->cmd);
          e->cmd = NULL;
          reset_req_body(e);
          ret++;
          }
        
        if(result > 0)
          {
          /* Command finished, get next command or become idle */
          ret++;

          /* Check if there is a command */
          if((e->cmd = bg_msg_sink_get_read(e->ctrl.cmd_sink)))
            handle_msg(e);
          else
            {
            e->status = STATE_POLL;

            e->flags &= ~FLAG_LOGGED_IN;
            start_login(e);
            }
          }
        else if(result < 0)
          {
          /* Error */
          set_offline(e);
          ret++;
          break;
          }
        else
          {
          break;
          }
        }
      break;
    case STATE_IDLE:
      /* Check if there is a command */
      if((e->cmd = bg_msg_sink_get_read(e->ctrl.cmd_sink)))
        {
        ret++;
        handle_msg(e);
        }
      /* No command left but still logged in: Log off to give other clients a chance */
      else if(e->flags & FLAG_LOGGED_IN)
        {
        result = gavl_http_client_run_async_done(e->io, 0);
        if(result > 0)
          {
          gavl_msg_t * evt;

          e->flags &= ~FLAG_LOGGED_IN;
          
          //          e->logged_in = 0; // Logout complete
          //          fprintf(stderr, "Logout complete\n");
          reset_connection(e);
          reset_req_body(e);
          gavl_buffer_init(&e->req_body);
          ret++;

          //          fprintf(stderr, "Now idle\n");
      
          evt = bg_msg_sink_get(e->ctrl.evt_sink);
          gavl_msg_set_id_ns(evt, GAVL_MSG_CONTROL_IDLE, GAVL_MSG_NS_CONTROL);
          bg_msg_sink_put(e->ctrl.evt_sink);
          
          }
        else if(result < 0)
          {
          ret++;
          /* Handle error */
          set_offline(e);
          }
        else
          break;
        }
      else
        {
        gavl_time_t cur = gavl_time_get_monotonic();
        if(cur - e->last_poll_time >= 2*GAVL_TIME_SCALE)
          {
          e->status = STATE_POLL;
          e->flags &= ~FLAG_LOGGED_IN;
          start_login(e);
          }
        }
      break;
    case STATE_OFFLINE:
      {
      gavl_time_t cur = gavl_time_get_monotonic();
      if(cur - e->last_poll_time >= 2*GAVL_TIME_SCALE)
        {
        e->status = STATE_POLL;
        e->flags &= ~FLAG_LOGGED_IN;
        start_login(e);
        }
      }
      break;
    case STATE_LABELS:
      if(!(e->flags & FLAG_LOGGED_IN))
        {
        if(login_complete(e))
          {
          ret++;
          
          if(e->flags & FLAG_LOGGED_IN)
            {
            start_get_labels(e);
            }
          else
            {
            
            }
          }
        }

      if(e->flags & FLAG_LOGGED_IN)
        {
        result = gavl_http_client_run_async_done(e->io, 0);
        if(result > 0)
          {
          const char * pos;
          char * label;
          
          pos = (char*)e->res_body.buf;

          if((pos = next_label(pos, &label)) &&
             label)
            {
            update_label(e, "s1", label);

            if((pos = next_label(pos, &label)) &&
               label)
              update_label(e, "s2", label);
            
            if((pos = next_label(pos, &label)) &&
               label)
              update_label(e, "s3", label);
            
            if((pos = next_label(pos, &label)) &&
               label)
              update_label(e, "s4", label);

            e->flags |= FLAG_HAVE_LABELS;
            }
          
          /* Check if there is a command */
          if((e->cmd = bg_msg_sink_get_read(e->ctrl.cmd_sink)))
            {
            // fprintf(stderr, "Got labels, now handling command\n");
            handle_msg(e);
            }
          else
            {
            e->status = STATE_IDLE;
            start_logout(e);
            // fprintf(stderr, "Got labels, got no command\n");
            }
          }
        }
      
      break;
    }

  return ret;
  }

static void * create_energenie()
  {
  energenie_t * e = calloc(1, sizeof(*e));

  bg_controllable_init(&e->ctrl,
                       bg_msg_sink_create(NULL, e, 0),
                       bg_msg_hub_create(1));
  
  return e;
  }

static void destroy_energenie(void *priv)
  {
  energenie_t * e = priv;
  if(e->addr)
    free(e->addr);
  if(e->password)
    free(e->password);
  bg_controllable_cleanup(&e->ctrl);
  free(e);
  }

static int open_energenie(void *priv, const char * uri)
  {
  char * host = NULL;
  int port = 0;
  energenie_t * e = priv;

  gavl_url_split(uri, NULL, NULL, &e->password, &host, &port, NULL);
  

  if(port < 1)
    e->addr = gavl_sprintf("http://%s", host);
  else
    e->addr = gavl_sprintf("http://%s:%d", host,port);

  // fprintf(stderr, "open_energenie: %s %s\n", e->addr, e->password);

  /* Load the labels */
  
  e->status = STATE_POLL;
  start_login(e);
  
  free(host);
  
  return 1;
  }

static bg_controllable_t * get_controllable_energenie(void * priv)
  {
  energenie_t * e = priv;
  return &e->ctrl;
  }

bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_energenie",
    .long_name = TRS("Energenie"),
    .description = TRS("Energenie"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_energenie,
    .destroy =   destroy_energenie,
    .get_controllable =   get_controllable_energenie,
    .priority =         1,
    },
  
  .protocols = "energenie",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_energenie,
  .open   = open_energenie,
  .get_controls   = get_controls_energenie,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
