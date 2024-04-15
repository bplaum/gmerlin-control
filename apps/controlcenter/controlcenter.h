#include <gmerlin/plugin.h>
#include <gmerlin/httpserver.h>
#include <gmerlin/websocket.h>

#include "control.h"

#define PLUGIN_FLAG_NOMASTER (1<<0)

typedef struct control_center_s control_center_t;

typedef struct
  {
  int flags;
  
  bg_plugin_handle_t * h;
  bg_control_plugin_t * c;

  char * path;
  char * uri;
  char * master;
  
  bg_controllable_t * ctrl;

  struct control_center_s * center;
  bg_msg_sink_t * sink;
  } control_plugin_t;

struct control_center_s
  {
  control_plugin_t * plugins;
  int num_plugins;
  
  gavl_dictionary_t controls;
  
  bg_http_server_t * srv;
  bg_websocket_context_t * ws;

  bg_controllable_t ctrl;
  
  };

int controlcenter_init(control_center_t * ret);
void controlcenter_cleanup(control_center_t * c);

int controlcenter_iteration(control_center_t * c);
