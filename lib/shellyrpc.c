#include <string.h>


#include <shellyrpc.h>
#include <mqtt.h>
#include <gavl/utils.h>
#include <gmerlin/bggavl.h>

#define GET_STATUS_ID 123123

static const char * status_names[] =
  {
    "switch:0",
    "wifi",
    NULL
  };

static void report_status(shelly_rpc_t * r,
                          const gavl_dictionary_t * parent)
  {
  const gavl_dictionary_t * dict;
  int i = 0; 

#if 0
  fprintf(stderr, "Got status:\n");
  gavl_dictionary_dump(parent, 2);
  fprintf(stderr, "\n");
#endif

  
  while(status_names[i])
    {
    if((dict = gavl_dictionary_get_dictionary(parent, status_names[i])))
      {
#if 0
      fprintf(stderr, "Got status: %s\n", status_names[i]);
      gavl_dictionary_dump(dict, 2);
      fprintf(stderr, "\n");
#endif
      
      if(r->update_status)
        r->update_status(r->data, status_names[i], dict);
      }
    
    i++;
    }
  
  }


static int handle_mqtt(void * data, gavl_msg_t * msg)
  {
  gavl_value_t val;
  shelly_rpc_t * r = data;
  const gavl_value_t  * buf_val;
  const gavl_buffer_t * buf;
  const char * topic = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);
  gavl_value_init(&val);
  if(!(buf_val = gavl_msg_get_arg_c(msg, 0)) ||
     !(buf = gavl_value_get_binary(buf_val)))
    {
    /* Error */
    return 1;
    }
  
  if(!strcmp(topic, "online"))
    {
    /* true or false */
    if(!strcmp((const char*)buf->buf, "true"))
      {
      fprintf(stderr, "Device is online\n");
      gavl_control_set_online(r->ctrl->evt_sink, "/", 1);
      }
    else
      {
      fprintf(stderr, "Device is offline\n");
      gavl_control_set_online(r->ctrl->evt_sink, "/", 0);
      }
    }
  else if(gavl_string_starts_with(topic, "status/"))
    {
    if(!bg_value_from_json_string_external(&val, (const char*)buf->buf))
      {
      /* Error */
      fprintf(stderr, "Parsing json failed: %s\n", (const char*)buf->buf);
      }
    //    fprintf(stderr, "Got status:\n");
    //    gavl_value_dump(&val, 2);
    }
  else if(!strcmp(topic, "events/rpc"))
    {
    const char * method;
    const gavl_dictionary_t * dict;
      
    if(!bg_value_from_json_string_external(&val, (const char*)buf->buf) ||
       !(dict = gavl_value_get_dictionary(&val)))
      {
      /* Error */
      fprintf(stderr, "Parsing json failed: %s\n", (const char*)buf->buf);
      }
    
    method = gavl_dictionary_get_string(dict, "method");

    if(!strcmp(method, "NotifyStatus"))
      {
      const gavl_dictionary_t * params;
      params = gavl_dictionary_get_dictionary(dict, "params");
      report_status(r, params);
      }
    
    }
  else if(!strcmp(topic, "rpc"))
    {
    int id;
    const char * src;
    const char * dst;
    const gavl_dictionary_t * dict;
    
    if(!bg_value_from_json_string_external(&val, (const char*)buf->buf))
      {
      /* Error */
      fprintf(stderr, "Parsing json failed: %s\n", (const char*)buf->buf);
      }
        
    if(!(dict = gavl_value_get_dictionary(&val)) ||
       !gavl_dictionary_get_int(dict, "id", &id) ||
       (id != GET_STATUS_ID) ||
       !(src = gavl_dictionary_get_string(dict, "src")) ||
       !(dst = gavl_dictionary_get_string(dict, "dst")) ||
       strcmp(src, r->dev) || strcmp(dst, r->client_id))
      return 1;

    dict = gavl_dictionary_get_dictionary(dict, "result");
    report_status(r, dict);
        
    //  fprintf(stderr, "Got result\n");
    //    gavl_value_dump(&val, 2);
    }
  gavl_value_free(&val);
  
  
  return 1;
  }

void shelly_rpc_init(shelly_rpc_t * r, bg_controllable_t * ctrl, const char * device)
  {
  uuid_t u;
  uuid_generate(u);
  uuid_unparse(u, r->client_id);
  r->sink = bg_msg_sink_create(handle_mqtt, r, 1);
  r->dev = device;
  r->ctrl = ctrl;
  
  bg_mqtt_subscribe(device, r->sink);
  bg_mqtt_subscribe(r->client_id, r->sink);

  shellyrpc_call_method(r, GET_STATUS_ID, "Shelly.GetStatus", NULL);
  
  }


void shellyrpc_call_method(shelly_rpc_t * r,
                           int id,
                           const char * method,
                           const char * args)
  {
  gavl_buffer_t buf;
  char * topic;
  char * cmd = gavl_sprintf("{\"id\":%d,\"src\":\"%s\",\"method\":\"%s\"",
                          id, r->client_id, method);

  if(args)
    {
    char * args_str = gavl_sprintf(", \"params\":%s }", args);
    cmd = gavl_strcat(cmd, args_str);
    free(args_str);
    }
  else
    cmd = gavl_strcat(cmd, "}");

  topic = gavl_sprintf("%s/rpc", r->dev);
  
  gavl_buffer_init(&buf);
  buf.buf = (uint8_t*)cmd;
  buf.len = strlen(cmd);
  
  //  fprintf(stderr, "Publishing: %s %s\n", topic, cmd);
  
  bg_mqtt_publish(topic, &buf, 1, 0);
  
  free(cmd);
  }

