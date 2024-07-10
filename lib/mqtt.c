
#include <unistd.h>
#include <string.h>

#include <mosquitto.h>

/* Local includes */

#include <config.h>
#include <mqtt.h>
#include <control.h>

#include <gavl/log.h>
#include <gavl/utils.h>
#define LOG_DOMAIN "mqtt"
#include <gavl/utils.h>

typedef struct
  {
  int actions;
  
  struct mosquitto * m;

  struct
    {
    bg_msg_sink_t * sink;
    char * topic;
    } * subscriptions;

  int num_subscriptions;
  int subscriptions_alloc;

  int queue_len;
  
  } mqtt_t;

static mqtt_t * mqtt = NULL;
static int mqtt_error = 0;

/* Callback when a message is received */
#if 0
static void dump_msg(const struct mosquitto_message *msg)
  {
  fprintf(stderr, "Got message:\n  id: %d\n  topic: %s\n  qos: %d\n  retain: %d\n",
          msg->mid, msg->topic, msg->qos, msg->retain);
  gavl_hexdump(msg->payload, msg->payloadlen, 16);
  }
#endif

static void on_message(struct mosquitto *m, void * priv, const struct mosquitto_message *msg,
                       const mosquitto_property *props)
  {
  int i;

  //  fprintf(stderr, "on_message %d %s\n", mqtt->num_subscriptions, mqtt->subscriptions[0].topic);
  //  dump_msg(msg);

  for(i = 0; i < mqtt->num_subscriptions; i++)
    {
    
    if(gavl_string_starts_with(msg->topic, mqtt->subscriptions[i].topic))
      {
      gavl_value_t val;
      gavl_buffer_t * buf;
      uint8_t terminator = '\0';
      gavl_msg_t * gavl_msg = bg_msg_sink_get(mqtt->subscriptions[i].sink);
      gavl_msg_set_id_ns(gavl_msg, GAVL_MSG_MQTT, GAVL_MSG_NS_MQTT);
      gavl_dictionary_set_string(&gavl_msg->header, GAVL_MSG_CONTEXT_ID, msg->topic + strlen(mqtt->subscriptions[i].topic));

      gavl_value_init(&val);
      buf = gavl_value_set_binary(&val);
      gavl_buffer_append_data(buf, msg->payload, msg->payloadlen);
      /* Zero terminate */
      gavl_buffer_append_data(buf, &terminator, 1);
      gavl_msg_set_arg_nocopy(gavl_msg, 0, &val);
      bg_msg_sink_put(mqtt->subscriptions[i].sink);
      break; /* This must be changed if multiple plugins listen to the same channel */
      }
    }

  mqtt->actions++;
  }

static void on_publish(struct mosquitto *m, void * priv, int mid, int code,
                       const mosquitto_property *props)
  {
  mqtt->actions++;
  mqtt->queue_len--;
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Publish completed\n");
  }

static void on_disconnect(struct mosquitto *m, void * priv, int rc, const mosquitto_property *props)
  {
  mqtt->actions++;
  gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Broker disconnected: %d\n", rc);
  }

/* Message callback */

int bg_mqtt_init()
  {
  int port = 1883;
  int result;
  const char * host = getenv("MQTT_BROKER");

  if(mqtt_error)
    return 0;
  
  mosquitto_lib_init();
  
  if(!host)
    host = "localhost";

  mqtt = calloc(1, sizeof(*mqtt));
  
  mqtt->m = mosquitto_new(NULL, 1, mqtt);

  result = mosquitto_connect_bind_v5(mqtt->m, host, port, 10, NULL, NULL);

  if(result != MOSQ_ERR_SUCCESS)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Connection to %s:%d failed: %s",
             host, port, mosquitto_strerror(result));
    bg_mqtt_cleanup();
    mqtt_error = 1;
    return 0;
    }

  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Connected to broker %s:%d", host, port);

  mosquitto_message_v5_callback_set(mqtt->m, on_message);

  mosquitto_publish_v5_callback_set(mqtt->m, on_publish);

  mosquitto_disconnect_v5_callback_set(mqtt->m, on_disconnect);
  
  //  mosquitto_subscribe(mqtt->m, NULL, "#", 1);
  
  
  return 1;
  }

void bg_mqtt_cleanup()
  {
  if(!mqtt)
    return;
  
  if(mqtt->m)
    mosquitto_destroy(mqtt->m);
  
  mosquitto_lib_cleanup();
  mqtt = NULL;
  }

int bg_mqtt_update()
  {
  if(!mqtt)
    return 0;
  
  mqtt->actions = 0;
  
  mosquitto_loop(mqtt->m, 0, 20);
  
  return mqtt->actions;
  }

int bg_mqtt_subscribe(const char * topic, bg_msg_sink_t * sink)
  {
  char * template;

  if(!mqtt && !bg_mqtt_init())
    return 0;
  
  if(mqtt->num_subscriptions == mqtt->subscriptions_alloc)
    {
    mqtt->subscriptions_alloc += 32;
    mqtt->subscriptions = realloc(mqtt->subscriptions,
                                  mqtt->subscriptions_alloc * sizeof(*mqtt->subscriptions));
    memset(mqtt->subscriptions + mqtt->num_subscriptions, 0,
           (mqtt->subscriptions_alloc - mqtt->num_subscriptions)*sizeof(*mqtt->subscriptions));
    }

  mqtt->subscriptions[mqtt->num_subscriptions].sink = sink;
  mqtt->subscriptions[mqtt->num_subscriptions].topic = gavl_strdup(topic);
  if(!gavl_string_ends_with(mqtt->subscriptions[mqtt->num_subscriptions].topic, "/"))
    mqtt->subscriptions[mqtt->num_subscriptions].topic =
      gavl_strcat(mqtt->subscriptions[mqtt->num_subscriptions].topic, "/");
  
  template = gavl_sprintf("%s#", mqtt->subscriptions[mqtt->num_subscriptions].topic);
  gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Subscribing to: %s", template);
  mosquitto_subscribe(mqtt->m, NULL, template, 1);
  free(template);
  
  mqtt->num_subscriptions++;
  return 1;
  }

void bg_mqtt_unsubscribe_by_sink(bg_msg_sink_t * sink)
  {
  if(!mqtt)
    return;
  }

int bg_mqtt_publish(const char * topic, gavl_buffer_t * payload, int qos, int retain)
  {
  if(!mqtt && !bg_mqtt_init())
    return 0;
  
  mosquitto_publish_v5(mqtt->m,
                       NULL,
                       topic,
                       payload->len,
                       payload->buf,
                       qos, retain,
                       NULL);

  mqtt->queue_len++;
  return 1;
  }

int bg_mqtt_queue_len()
  {
  if(!mqtt)
    return 0;
  return mqtt->queue_len;
  }
