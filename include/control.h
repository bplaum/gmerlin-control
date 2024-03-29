/* Control definitions */

#include <gavl/gavl.h>
#include <gavl/value.h>

/* Types of control elements */

#define GAVL_META_CLASS_CONTROL_BUTTON      "control.button"    //
#define GAVL_META_CLASS_CONTROL_SLIDER      "control.slider"    //
#define GAVL_META_CLASS_CONTROL_PULLDOWN    "control.pulldown"
#define GAVL_META_CLASS_CONTROL_VOLUME      "control.volume"
// #define GAVL_META_CLASS_CONTROL_SPINBUTTON  "control.spinbutton"
// #define GAVL_META_CLASS_CONTROL_STRING      "control.string"
// #define GAVL_META_CLASS_CONTROL_TEXT        "control.text"
// #define GAVL_META_CLASS_CONTROL_RGBCOLOR    "control.rgbcolor"

#define GAVL_META_CLASS_CONTROL_POWERBUTTON "control.power"     //
#define GAVL_META_CLASS_CONTROL_METER       "control.meter"
#define GAVL_META_CLASS_CONTROL_SEPARATOR   "control.separator"
#define GAVL_META_CLASS_CONTROL_LINK        "control.link"

#define GAVL_META_CLASS_CONTROL_GROUP       "container.controlgroup"
#define GAVL_META_CLASS_CONTAINER_INVISIBLE "container.invisible"


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
#define GAVL_CONTROL_VALUE      "value"
#define GAVL_CONTROL_OPTIMUM    "optimum"
#define GAVL_CONTROL_DIGITS     "digits"

/* Delay an option. To be added to message header to delay an operation */
#define GAVL_CONTROL_DELAY       "delay"

#define GAVL_CONTROL_PATH        "Path"

/* Messsages */

#define GAVL_MSG_NS_CONTROL 300

/*
 *  ContextID: path
 */

#define GAVL_CMD_CONTROL_PUSH_BUTTON     1

/*
 *  ContextID: path
 */

#define GAVL_FUNC_CONTROL_BROWSE         2

/*
 *  ContextID: path
 *
 *  arg0: control elements (array)
 */

#define GAVL_RESP_CONTROL_BROWSE         100

/*
 *  ContextID: path
 *  arg0: dictionary with changed values
 */

#define GAVL_MSG_CONTROL_CHANGED        204

/*
 *  Entering idle state.
 */

#define GAVL_MSG_CONTROL_IDLE            205

void gavl_control_create_root(gavl_dictionary_t * dict);

gavl_dictionary_t * gavl_control_get_section_create(gavl_dictionary_t * parent, const char * id);
const gavl_dictionary_t * gavl_control_get_child(const gavl_dictionary_t * parent, const char * id);
gavl_dictionary_t * gavl_control_get_child_create(gavl_dictionary_t * parent, const char * id);

void gavl_control_init(gavl_dictionary_t * ctrl,
                       const char * klass,
                       const char * id,
                       const char * label);

gavl_dictionary_t * gavl_control_add_control(gavl_dictionary_t * parent,
                                             const char * klass,
                                             const char * id,
                                             const char * label);

void gavl_control_append(gavl_dictionary_t * parent, gavl_value_t * controls);
int gavl_control_num_children(const gavl_dictionary_t * parent);


void gavl_control_set_default(gavl_dictionary_t * ctrl, const gavl_value_t * val);
void gavl_control_set_type(gavl_dictionary_t * ctrl, gavl_type_t type);
gavl_type_t gavl_control_get_type(const gavl_dictionary_t * ctrl);

gavl_dictionary_t * gavl_control_add_option(gavl_dictionary_t * ctrl, const char * id, const char * label);
gavl_dictionary_t * gavl_control_get_option(gavl_dictionary_t * ctrl, const char * tag, const char * val);

void gavl_control_delete_option(gavl_dictionary_t * ctrl, const char * id);


const gavl_dictionary_t * gavl_control_get(const gavl_dictionary_t * root, const char * path);
gavl_dictionary_t * gavl_control_get_create(gavl_dictionary_t * dict, const char * path);

void gavl_control_clamp_value(const gavl_dictionary_t * control,
                              gavl_value_t * val);

int gavl_control_handle_set_rel(const gavl_dictionary_t * controls,
                                const char * ctx, const char * var,
                                gavl_value_t * val);
