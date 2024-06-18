
#include <uuid/uuid.h>
#include <control.h>
#include <gmerlin/bgmsg.h>

typedef struct
  {
  char client_id[37];
  bg_msg_sink_t * sink;
  const char * dev;

  /* Status updates */
  void (*update_status)(void * data, const char * name, const gavl_dictionary_t * dict);
  void * data;
  
  } shelly_rpc_t;

void shelly_rpc_init(shelly_rpc_t * r, bg_controllable_t * ctrl, const char * device);

/* RPC via mqtt for Shelly gen2 devices */

void shellyrpc_call_method(shelly_rpc_t * r,
                           int id,
                           const char * method,
                           const char * args);

