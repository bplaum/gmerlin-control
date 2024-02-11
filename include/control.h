/* Control definitions */

#include <gavl/gavl.h>
#include <gavl/value.h>

/* Types of control elements */

#define GAVL_META_CLASS_CONTROL_BUTTON      "control.button"
#define GAVL_META_CLASS_CONTROL_SLIDER      "control.slider"
#define GAVL_META_CLASS_CONTROL_PULLDOWN    "control.pulldown"
#define GAVL_META_CLASS_CONTROL_VOLUME      "control.volume"
#define GAVL_META_CLASS_CONTROL_SPINBUTTON  "control.spinbutton"
#define GAVL_META_CLASS_CONTROL_STRING      "control.string"
#define GAVL_META_CLASS_CONTROL_TEXT        "control.text"
#define GAVL_META_CLASS_CONTROL_RGBCOLOR    "control.rgbcolor"
#define GAVL_META_CLASS_CONTROL_POWERBUTTON "control.power"
#define GAVL_META_CLASS_CONTROL_METER       "control.meter"

// #define GAVL_CONTROL_RGBACOLOR  "control.rgbacolor"

/* Read only */

#define GAVL_CONTROL_METER      "control.meter"

#define GAVL_CONTROL_MIN        "min"
#define GAVL_CONTROL_MAX        "max"
#define GAVL_CONTROL_LOW        "low"
#define GAVL_CONTROL_HIGH       "high"
#define GAVL_CONTROL_STEP       "step"
#define GAVL_CONTROL_DEFAULT    "default"
#define GAVL_CONTROL_OPTIONS    "options"
#define GAVL_CONTROL_READONLY   "ro"
#define GAVL_CONTROL_CHILDREN   "children"
#define GAVL_CONTROL_UNIT       "unit"
#define GAVL_CONTROL_TYPE       "type"

/* Delay an option. To be added to message header to delay an operation */
#define GAVL_CONTROL_DELAY       "delay"

/* Messsages */

#define GAVL_MSG_NS_CONTROL 300

#define GAVL_CMD_CONTROL_PUSH_BUTTON    1

/*
 *  ContextID: path
 */

#define GAVL_MSG_CONTROL_ENABLED        2

/*
 *  ContextID: path
 */

#define GAVL_MSG_CONTROL_DISABLED       3

/*
 *  ContextID: path
 *  arg0: id
 *  arg1: dictionary
 */

#define GAVL_CMD_CONTROL_OPTION_ADDED   4

/*
 *  ContextID: path
 *  arg0: id
 */

#define GAVL_CMD_CONTROL_OPTION_REMOVED  5

/*
 *  Entering idle state.
 */

#define GAVL_MSG_CONTROL_IDLE            6

void gavl_control_create_root(gavl_dictionary_t * dict);

gavl_dictionary_t * gavl_control_get_section_create(gavl_dictionary_t * parent, const char * id);
const gavl_dictionary_t * gavl_control_get_child(const gavl_dictionary_t * parent, const char * id);

gavl_dictionary_t * gavl_control_add_control(gavl_dictionary_t * parent,
                                             const char * klass,
                                             const char * id,
                                             const char * label);

void gavl_control_set_default(gavl_dictionary_t * ctrl, const gavl_value_t * val);
void gavl_control_set_type(gavl_dictionary_t * ctrl, gavl_type_t type);
gavl_type_t gavl_control_get_type(const gavl_dictionary_t * ctrl);

void gavl_control_set_range(gavl_dictionary_t * ctrl, const gavl_value_t * min, const gavl_value_t * max);
void gavl_control_set_low_high(gavl_dictionary_t * ctrl, const gavl_value_t * low, const gavl_value_t * high);
gavl_dictionary_t * gavl_control_add_option(gavl_dictionary_t * ctrl, const char * id, const char * label);

const gavl_dictionary_t * gavl_control_get(const gavl_dictionary_t * root, const char * path);
