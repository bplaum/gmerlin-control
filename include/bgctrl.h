/*
[
 {
 "id":"system",
 "type":"container",
 "label":"System",
 "children":
   [
   ],
 },
 {
 "id":"system",
 "type":"container",
 "label":"System",

 },
]
*/


typedef enum
  {
    BGCTRL_TYPE_NONE       = 0,
    BGCTRL_TYPE_BUTTON     = 1,
    BGCTRL_TYPE_SLIDER     = 2,
    BGCTRL_TYPE_METER      = 3,
    BGCTRL_TYPE_VOLUME     = 4,
    BGCTRL_TYPE_CONTAINER  = 5,
  } bgctrl_type_t;

#define BGCTRL_LABEL GAVL_META_LABEL
#define BGCTRL_NAME       "name"
#define BGCTRL_TYPE       "type"
#define BGCTRL_DESCPITION "desc"

#define BGCTRL_VAL_MIN  "min"
#define BGCTRL_VAL_MIN  "max"
#define BGCTRL_VAL_LOW  "low"
#define BGCTRL_VAL_HIGH "high"

#define BGCTRL_NUM_DIGITS "ndig"
#define BGCTRL_CONTROLS "controls"



gavl_dictionary_t *
bgctrl_info_add_button(gavl_dictionary_t * parent, const char * name, const char * label);

gavl_dictionary_t *
bgctrl_info_add_slider(gavl_dictionary_t * parent, const char * name, const char * label, double val_min, double val_max);

gavl_dictionary_t *
bgctrl_info_add_meter(gavl_dictionary_t * parent, const char * name, const char * label,
                      double val_min,
                      double val_low,
                      double val_high,
                      double val_max);

gavl_dictionary_t *
bgctrl_info_add_container(gavl_dictionary_t * parent, const char * name, const char * label);

/* Control backend */

typedef struct bgctrl_backend_handle_s bgctrl_backend_handle_t;

bgctrl_backend_handle_t * bgctrl_backend_handle_create(const char * uri);
void bgctrl_backend_handle_destroy(bgctrl_backend_handle_t * h);
bg_controllable_t * bgctrl_backend_handle_get_controllable(bgctrl_backend_handle_t * h);


#define BCTRL_MSG_NS 300

#define BCTRL_CMD_SET_STATE_VAL    1
#define BCTRL_CMD_SET_STATE_REL    2
#define BCTRL_CMD_SET_STATE_VOLUME 3
#define BCTRL_CMD_SET_STATE_BUTTON 4 // Button press

#define BCTRL_MSG_STATE_CHANGED    100


#define BCTRL_FUNC_GET_STATE     200
#define BCTRL_RESO_GET_STATE     300
