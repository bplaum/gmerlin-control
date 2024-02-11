
#include <stdlib.h>
#include <string.h>

#include <control.h>
#include <gavl/metatags.h>
#include <gavl/utils.h>

void gavl_control_create_root(gavl_dictionary_t * dict)
  {
  gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_ROOT);
  gavl_dictionary_set_string(dict, GAVL_META_ID, "/");
  }

const gavl_dictionary_t * gavl_control_get_child(const gavl_dictionary_t * parent, const char * id)
  {
  int i;
  const gavl_array_t * arr;
  const gavl_dictionary_t * ret;
  const char * var;
  
  if(!(arr = gavl_dictionary_get_array(parent, GAVL_CONTROL_CHILDREN)))
    return NULL;
  
  for(i = 0; i < arr->num_entries; i++)
    {
    if((ret = gavl_value_get_dictionary(&arr->entries[i])) &&
       (var = gavl_dictionary_get_string(ret, GAVL_META_ID)) &&
       !strcmp(var, id))
      return ret;
    }
  return NULL;
  }

gavl_dictionary_t * gavl_control_add_control(gavl_dictionary_t * parent,
                                             const char * klass,
                                             const char * id,
                                             const char * label)
  {
  gavl_value_t new_val;
  gavl_dictionary_t * new_dict;
  gavl_array_t * arr = gavl_dictionary_get_array_create(parent, GAVL_CONTROL_CHILDREN);

  gavl_value_init(&new_val);
  new_dict = gavl_value_set_dictionary(&new_val);
  
  gavl_dictionary_set_string(new_dict, GAVL_META_CLASS, klass);
  gavl_dictionary_set_string(new_dict, GAVL_META_ID, id);
  gavl_dictionary_set_string(new_dict, GAVL_META_LABEL, label);
  
  gavl_array_splice_val_nocopy(arr, -1, 0, &new_val);
  return new_dict;
  }

void gavl_control_set_default(gavl_dictionary_t * ctrl, const gavl_value_t * val)
  {
  gavl_dictionary_set(ctrl, GAVL_CONTROL_DEFAULT, val);
  }

void gavl_control_set_range(gavl_dictionary_t * ctrl, const gavl_value_t * min, const gavl_value_t * max)
  {
  gavl_dictionary_set(ctrl, GAVL_CONTROL_MIN, min);
  gavl_dictionary_set(ctrl, GAVL_CONTROL_MAX, max);
  }

void gavl_control_set_low_high(gavl_dictionary_t * ctrl, const gavl_value_t * low, const gavl_value_t * high)
  {
  gavl_dictionary_set(ctrl, GAVL_CONTROL_LOW, low);
  gavl_dictionary_set(ctrl, GAVL_CONTROL_HIGH, high);
  
  }


gavl_dictionary_t * gavl_control_add_option(gavl_dictionary_t * ctrl, const char * id, const char * label)
  {
  gavl_value_t new_val;
  gavl_dictionary_t * new_dict;
  gavl_array_t * arr = gavl_dictionary_get_array_create(ctrl, GAVL_CONTROL_OPTIONS);

  gavl_value_init(&new_val);
  new_dict = gavl_value_set_dictionary(&new_val);
  
  gavl_dictionary_set_string(new_dict, GAVL_META_ID, id);
  gavl_dictionary_set_string(new_dict, GAVL_META_LABEL, label);
  
  gavl_array_splice_val_nocopy(arr, -1, 0, &new_val);
  return new_dict;
  }

const gavl_dictionary_t * gavl_control_get(const gavl_dictionary_t * dict, const char * path)
  {
  char ** el;
  int idx;
  
  if(!strcmp(path, "/"))
    return dict;

  el = gavl_strbreak(path, '/');
  idx = 0;
  while(el[idx])
    {
    if(!(dict = gavl_control_get_child(dict, el[idx])))
      {
      gavl_strbreak_free(el);
      return NULL;
      }
    idx++;
    }
  gavl_strbreak_free(el);
  return dict;
  }
  
void gavl_control_set_type(gavl_dictionary_t * ctrl, gavl_type_t type)
  {
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_TYPE, gavl_type_to_string(type));
  }

gavl_type_t gavl_control_get_type(const gavl_dictionary_t * ctrl)
  {
  const char * type;
  const char * klass;
  
  if(!(type = gavl_dictionary_get_string(ctrl, GAVL_CONTROL_TYPE)))
    {
    if(!(klass = gavl_dictionary_get_string(ctrl, GAVL_META_CLASS)))
      return GAVL_TYPE_UNDEFINED;

    if(!strcmp(klass, GAVL_META_CLASS_CONTROL_PULLDOWN))
      return GAVL_TYPE_STRING;
    if(!strcmp(klass, GAVL_META_CLASS_CONTROL_STRING))
      return GAVL_TYPE_STRING;
    if(!strcmp(klass, GAVL_META_CLASS_CONTROL_POWERBUTTON))
      return GAVL_TYPE_INT;
    
    }
  
  }
