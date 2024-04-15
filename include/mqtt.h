
#include <gmerlin/bgmsg.h>


int bg_mqtt_init();

void bg_mqtt_cleanup();

int bg_mqtt_update();

int bg_mqtt_queue_len();


int bg_mqtt_subscribe(const char * topic, bg_msg_sink_t * sink);
void bg_mqtt_unsubscribe_by_sink(bg_msg_sink_t * sink);
int bg_mqtt_publish(const char * topic, gavl_buffer_t * payload, int qos, int retain);


