#include <gmerlin/plugin.h>
#include <gmerlin/httpserver.h>
#include <gmerlin/websocket.h>

#include "control.h"

typedef struct
  {
  bg_plugin_handle_t * h;
  bg_control_plugin_t * c;
  char * path;
  bg_controllable_t * ctrl;
  } control_plugin_t;

typedef struct
  {
  control_plugin_t * plugins;
  int num_plugins;
  control_plugin_t * cur;
  
  gavl_dictionary_t controls;
  
  bg_http_server_t * srv;
  bg_websocket_context_t * ws;

  bg_controllable_t ctrl;

  bg_msg_sink_t * backend_sink;
  
  } control_center_t;

int controlcenter_init(control_center_t * ret);
void controlcenter_cleanup(control_center_t * c);

int controlcenter_iteration(control_center_t * c);
