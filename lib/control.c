
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <config.h>
#include <control.h>
#include <gavl/metatags.h>
#include <gavl/utils.h>
#include <gavl/log.h>
#define LOG_DOMAIN "control"

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

gavl_dictionary_t * gavl_control_get_child_create(gavl_dictionary_t * parent, const char * id)
  {
  int i;
  gavl_array_t * arr;
  gavl_dictionary_t * ret;
  const char * var;
  gavl_value_t new_val;
  gavl_dictionary_t * new_dict;
  
  if(!(arr = gavl_dictionary_get_array_create(parent, GAVL_CONTROL_CHILDREN)))
    return NULL;
  
  for(i = 0; i < arr->num_entries; i++)
    {
    if((ret = gavl_value_get_dictionary_nc(&arr->entries[i])) &&
       (var = gavl_dictionary_get_string(ret, GAVL_META_ID)) &&
       !strcmp(var, id))
      return ret;
    }
  
  //  if(!id)
  if(!strcmp(id, "(null)"))
    fprintf(stderr, "gavl_control_get_child_create %s\n", id);
  
  gavl_value_init(&new_val);
  new_dict = gavl_value_set_dictionary(&new_val);
  gavl_dictionary_set_string(new_dict, GAVL_META_ID, id);
  gavl_dictionary_set_string(new_dict, GAVL_META_LABEL, id);
  gavl_dictionary_set_string(new_dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTAINER);
  gavl_array_splice_val_nocopy(arr, -1, 0, &new_val);
  return gavl_value_get_dictionary_nc(&arr->entries[arr->num_entries-1]);
  }

void gavl_control_init(gavl_dictionary_t * ctrl,
                       const char * klass,
                       const char * id,
                       const char * label)
  {
  gavl_dictionary_set_string(ctrl, GAVL_META_CLASS, klass);
  gavl_dictionary_set_string(ctrl, GAVL_META_ID, id);
  gavl_dictionary_set_string(ctrl, GAVL_META_LABEL, label);
  
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

  gavl_control_init(new_dict, klass, id, label);
  
  gavl_array_splice_val_nocopy(arr, -1, 0, &new_val);
  return new_dict;
  }

int gavl_control_num_children(const gavl_dictionary_t * parent)
  {
  const gavl_array_t * arr = gavl_dictionary_get_array(parent, GAVL_CONTROL_CHILDREN);
  if(!arr)
    return 0;
  else
    return arr->num_entries;
  }



void gavl_control_append(gavl_dictionary_t * parent, gavl_value_t * controls)
  {
  gavl_array_t * arr = gavl_dictionary_get_array_create(parent, GAVL_CONTROL_CHILDREN);
  gavl_array_splice_val_nocopy(arr, -1, 0, controls);
  }

void gavl_control_set_default(gavl_dictionary_t * ctrl, const gavl_value_t * val)
  {
  gavl_dictionary_set(ctrl, GAVL_CONTROL_DEFAULT, val);
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

gavl_dictionary_t * gavl_control_get_option(gavl_dictionary_t * ctrl, const char * tag, const char * val)
  {
  int i;
  gavl_dictionary_t * dict;
  const char * test_val;
  gavl_array_t * arr = gavl_dictionary_get_array_create(ctrl, GAVL_CONTROL_OPTIONS);

  for(i = 0; i < arr->num_entries; i++)
    {
    if((dict = gavl_value_get_dictionary_nc(&arr->entries[i])) &&
       (test_val = gavl_dictionary_get_string(dict, tag)) &&
       !strcmp(val, test_val))
      return dict;
    }
  return NULL;
  }

void gavl_control_delete_option(gavl_dictionary_t * ctrl, const char * id)
  {
  int i;
  const gavl_dictionary_t * dict;
  const char * test_id;
  gavl_array_t * arr = gavl_dictionary_get_array_create(ctrl, GAVL_CONTROL_OPTIONS);

  for(i = 0; i < arr->num_entries; i++)
    {
    if((dict = gavl_value_get_dictionary(&arr->entries[i])) &&
       (test_id = gavl_dictionary_get_string(dict, GAVL_META_ID)) &&
       !strcmp(id, test_id))
      {
      gavl_array_splice_val(arr, i, 1, NULL);
      return;
      }
    }
  
  }

const gavl_dictionary_t * gavl_control_get(const gavl_dictionary_t * dict, const char * path)
  {
  char ** el;
  int idx;
  const gavl_dictionary_t * dict_orig = dict;
  if(!strcmp(path, "/"))
    return dict;

  if(*path == '/')
    path++;
  
  el = gavl_strbreak(path, '/');
  idx = 0;

  if(el)
    {
    while(el[idx])
      {
      if(!(dict = gavl_control_get_child(dict, el[idx])))
        {
        gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "No such child %s path: %s", el[idx], path);
        gavl_dictionary_dump(dict_orig, 2);
        gavl_strbreak_free(el);
        return NULL;
        }
      idx++;
      }
    gavl_strbreak_free(el);
    }
  
  return dict;
  }

gavl_dictionary_t * gavl_control_get_create(gavl_dictionary_t * dict, const char * path)
  {
  char ** el;
  int idx;
  
  if(!strcmp(path, "/"))
    return dict;

  if(*path == '/')
    path++;
  
  el = gavl_strbreak(path, '/');
  idx = 0;
  if(el)
    {
    while(el[idx])
      {
      if(!(dict = gavl_control_get_child_create(dict, el[idx])))
        {
        gavl_strbreak_free(el);
        return NULL;
        }
      idx++;
      }
    gavl_strbreak_free(el);
    }
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
  
  if((type = gavl_dictionary_get_string(ctrl, GAVL_CONTROL_TYPE)))
    return gavl_type_from_string(type);
  
  if(!(klass = gavl_dictionary_get_string(ctrl, GAVL_META_CLASS)))
    return GAVL_TYPE_UNDEFINED;
  
  if(!strcmp(klass, GAVL_META_CLASS_CONTROL_PULLDOWN))
    return GAVL_TYPE_STRING;
  //  if(!strcmp(klass, GAVL_META_CLASS_CONTROL_STRING))
  //    return GAVL_TYPE_STRING;
  if(!strcmp(klass, GAVL_META_CLASS_CONTROL_POWERBUTTON))
    return GAVL_TYPE_INT;
  
  
  return GAVL_TYPE_UNDEFINED;
  }

static int get_min_max(const gavl_dictionary_t * dict, const gavl_value_t ** min, const gavl_value_t ** max)
  {
  if((*min = gavl_dictionary_get(dict, GAVL_CONTROL_MIN)) &&
     (*max = gavl_dictionary_get(dict, GAVL_CONTROL_MAX)))
    return 1;
  else
    return 0;
  }

void gavl_control_clamp_value(const gavl_dictionary_t * control,
                              gavl_value_t * val)
  {
  const gavl_value_t * min;
  const gavl_value_t * max;
  
  if(get_min_max(control, &min, &max))
    gavl_value_clamp(val, min, max);
  
  }

int gavl_control_handle_set_rel(const gavl_dictionary_t * control,
                                const char * ctx, const char * var,
                                gavl_value_t * val)
  {
  int ret = 0;
  const char * klass;
  const gavl_value_t * val_cur;
  gavl_value_t val_new;

  gavl_value_init(&val_new);
  
  if(!(control = gavl_control_get(control, ctx)) ||
     !(control = gavl_control_get(control, var)) ||
     !(klass = gavl_dictionary_get_string(control, GAVL_META_CLASS)))
    return 0;

  val_cur = gavl_dictionary_get(control, GAVL_CONTROL_VALUE);

  if(!strcmp(klass, GAVL_META_CLASS_CONTROL_POWERBUTTON))
    {
    if(val_cur && (val_cur->type == GAVL_TYPE_INT))
      gavl_value_set_int(&val_new, !val_cur->v.i);
    else
      gavl_value_set_int(&val_new, 1);
    
    ret = 1;
    }
  else
    {
    if(val_cur)
      {
      /* Add and clamp value */
      gavl_value_copy(&val_new, val_cur);
      gavl_value_addto(val, &val_new);
      gavl_control_clamp_value(control, &val_new);
      ret = 1;
      }
    }
  
  gavl_value_free(val);
  gavl_value_move(val, &val_new);
  return ret;
  }
