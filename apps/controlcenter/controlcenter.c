#include <glob.h>
#include <string.h>

#include <config.h>
#include "controlcenter.h"
#include <gmerlin/application.h>
#include <gmerlin/utils.h>
#include <gmerlin/bggavl.h>

#include <gavl/log.h>
#define LOG_DOMAIN "controlcenter"

// #define TEST_CONTROLS

#ifdef TEST_CONTROLS
static void init_controls_test(const char * path, gavl_array_t * ret);
#endif

/* Directory structure */

// XDG_CONFIG_DIR/gmerlin-control/controls

/*
 *  http://example.host:8886/get/path/var
 *  http://example.host:8886/set/path/var?v=1&delay=10
 *  http://example.host:8886/setrel/path/var?v=1&delay=10
 */

/*
 *  ID layout:
 *  <path>/<control_plugin>/var
 */

/* Manifest */

static const char * manifest_file =
  "{\n"
    "\"short_name\": \"%s\",\n"
    "\"name\": \"%s\",\n"
    "\"display\": \"standalone\",\n"
    "\"icons\": [ \n"
      "{\n"
      "\"src\": \"static2/icons/controlcenter_48.png\",\n"
      "\"sizes\": \"48x48\",\n"
      "\"type\": \"image/png\",\n"
      "\"density\": 1.0\n"
      "},\n"
      "{\n"
      "\"src\": \"static2/icons/controlcenter_96.png\",\n"
      "\"sizes\": \"96x96\",\n"
      "\"type\": \"image/png\",\n"
      "\"density\": 1.0\n"
      "} ],\n"
  "\"start_url\": \"%s\"\n"
  "}\n";

static int server_handle_manifest(bg_http_connection_t * conn, void * data)
  {
  const char * var;
  int result = 0;

  char * start_url = NULL;
  
  char * protocol = NULL;
  char * host = NULL;
  int port = 0;
  char * m = NULL;
  int len = 0;
  const char * label = bg_app_get_label();
  
  if(strcmp(conn->path, "/manifest.json"))
    return 0; // 404
  
  if(!(var = gavl_dictionary_get_string(&conn->req, "Referer")))
    goto fail;
  
  bg_http_connection_check_keepalive(conn);
      
  //      fprintf(stderr, "Referer: %s\n", var);
  gavl_url_split(var, &protocol, NULL, NULL, &host, &port, NULL);

  start_url = gavl_sprintf("%s://%s:%d", protocol, host, port);

  m = gavl_sprintf(manifest_file, label, label, start_url);
  len = strlen(m);
      
  bg_http_connection_init_res(conn, conn->protocol, 200, "OK");
  gavl_dictionary_set_string(&conn->res, "Content-Type", "application/manifest+json");
  gavl_dictionary_set_long(&conn->res, "Content-Length", len);
      
  if(!bg_http_connection_write_res(conn))
    {
    bg_http_connection_clear_keepalive(conn);
    goto cleanup;
    }
  result = 1;
  
  if(result && !gavl_socket_write_data(conn->fd, (const uint8_t*)m, len))
    bg_http_connection_clear_keepalive(conn);
  
  fail:

  if(!result)
    {
    bg_http_connection_init_res(conn, conn->protocol, 400, "Bad Request");
    if(!bg_http_connection_write_res(conn))
      bg_http_connection_clear_keepalive(conn);
    }
  
  cleanup:
  
  if(protocol)
    free(protocol);
  if(host)
    free(host);
  
  if(m)
    free(m);
  
  return 1;
  }

static control_plugin_t * get_plugin(control_center_t * c, const char * ctx)
  {
  int i, len;
  
  for(i = 0; i < c->num_plugins; i++)
    {
    // fprintf(stderr, "get_plugin %s %s\n", ctx, c->plugins[i].path);
    len = strlen(c->plugins[i].path);
    if(gavl_string_starts_with(ctx, c->plugins[i].path) &&
       ((ctx[len] == '\0') || (ctx[len] == '/') || (ctx[len] == '?')))
      return &c->plugins[i];
    }
  return NULL;
  }

/* Handle message from the web frontend (i.e. from the UI controls) */

static void browse(control_center_t * c, gavl_msg_t * msg)
  {
  const gavl_dictionary_t * parent;
  gavl_dictionary_t * child;
  gavl_array_t arr;
  const gavl_array_t * children;
  gavl_msg_t * res;
  const char * path = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);
  int i;
  gavl_array_init(&arr);
  
  //  fprintf(stderr, "Browse %s\n", path);

  if(!path)
    return;
  
#ifdef TEST_CONTROLS
  init_controls_test(path, &arr);
#else
  /* TODO */
  if(!(parent = gavl_control_get(&c->controls, path)))
    return;
     
  if(!(children = gavl_dictionary_get_array(parent, GAVL_CONTROL_CHILDREN)))
    return;
  
  gavl_array_copy(&arr, children);

  for(i = 0; i < arr.num_entries; i++)
    {
    if((child = gavl_value_get_dictionary_nc(&arr.entries[i])))
      {
      const char * klass;
      if((klass = gavl_dictionary_get_string(child, GAVL_META_CLASS)) &&
         !strcmp(klass, GAVL_META_CLASS_CONTAINER))         
        gavl_dictionary_set(child, GAVL_CONTROL_CHILDREN, NULL);
      }
    
    }
  
#endif

  res = bg_msg_sink_get(c->ctrl.evt_sink);
  gavl_msg_set_id_ns(res, GAVL_RESP_CONTROL_BROWSE, GAVL_MSG_NS_CONTROL);
  
  gavl_msg_set_arg_array_nocopy(res, 0, &arr);
  gavl_msg_set_resp_for_req(res, msg);
  bg_msg_sink_put(c->ctrl.evt_sink);
  }

static void forward_to_slaves(control_center_t * c, control_plugin_t * p,
                              gavl_msg_t * msg)
  {
  int i;

  for(i = 0; i < c->num_plugins; i++)
    {
    if(&c->plugins[i] == p)
      continue;

    if(c->plugins[i].master && !strcmp(p->uri, c->plugins[i].master) &&
       !(c->plugins[i].flags & PLUGIN_FLAG_NOMASTER))
      {
      bg_msg_sink_put_copy(c->plugins[i].ctrl->cmd_sink, msg);
      forward_to_slaves(c, &c->plugins[i], msg);
      }
    }
  
  }

static int handle_websocket_message(void * data, gavl_msg_t * msg)
  {
  gavl_msg_t * forward = NULL;
  control_plugin_t * p = NULL;
  control_center_t * c = data;

  switch(msg->NS)
    {
    case BG_MSG_NS_STATE:
      {
      gavl_value_t val;
      const char * ctx;
      const char * var;

      int last = 0;

      gavl_value_init(&val);
      
      switch(msg->ID)
        {
        case BG_CMD_SET_STATE_REL:
          {
          gavl_msg_t new_msg;
          
          /* Handle powerbutton */
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          //          fprintf(stderr, "Set state rel %s %s\n", ctx, var);
          
          if(ctx && gavl_control_handle_set_rel(&c->controls, ctx, var, &val))
            {
            gavl_msg_set_state(&new_msg,
                               BG_CMD_SET_STATE,
                               last, ctx, var, &val);
            gavl_msg_free(msg);
            memcpy(msg, &new_msg, sizeof(*msg));
            gavl_value_free(&val);
            }
          else
            {
            gavl_value_free(&val);
            break;
            }
          }
          // Fall through
          // break;  
        case BG_CMD_SET_STATE:
          {
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);

          if(ctx && (p = get_plugin(c, ctx)))
            {
            ctx += strlen(p->path);
            if(*ctx == '\0')
              ctx = "/";
            
            forward = bg_msg_sink_get(p->ctrl->cmd_sink);
            gavl_msg_set_state(forward, BG_CMD_SET_STATE, last, ctx, var, &val);

            if(p->uri)
              forward_to_slaves(c, p, forward);
            
            }
          else
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Set state: No plugin found for %s %s", ctx, var);
          }
          //          fprintf(stderr, "Set state %s %s\n", ctx, var);
          break;
        }
      gavl_value_free(&val);
      }
      break;
    case GAVL_MSG_NS_CONTROL:
      switch(msg->ID)
        {
        case GAVL_CMD_CONTROL_PUSH_BUTTON:
          {
          const char * ctx = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);
          fprintf(stderr, "Push button %s\n", ctx);

          if((p = get_plugin(c, ctx)))
            {
            ctx += strlen(p->path);

            if(*ctx == '/')
              ctx++;
            
            if(*ctx == '\0')
              ctx = "/";

            gavl_dictionary_set_string_nocopy(&msg->header, GAVL_MSG_CONTEXT_ID, gavl_strdup(ctx));
            bg_msg_sink_put_copy(p->ctrl->cmd_sink, msg);
            }
          }
          break;
        case GAVL_FUNC_CONTROL_BROWSE:
          browse(c, msg);
          break;
        }
      break;
    }
  
  if(p && forward)
    bg_msg_sink_put(p->ctrl->cmd_sink);
  
  return 1;
  }

static char * ctx_to_global(control_plugin_t * p, const char * ctx)
  {
  if(!strcmp(ctx, "/"))
    return gavl_sprintf("%s", p->path);
  else
    return gavl_sprintf("%s%s", p->path, ctx);
  }


/* Messages from the backends */

static int handle_control_message(void * data, gavl_msg_t * msg)
  {
  control_plugin_t * p = data;
  control_center_t * c = p->center;
  
  /* Update context ID and forward to websocket context */
  
  switch(msg->NS)
    {
    case BG_MSG_NS_STATE:
      
      switch(msg->ID)
        {
        case BG_MSG_STATE_CHANGED:
          {
          gavl_value_t val;
          const char * ctx;
          const char * var;
          int last = 0;
          char * new_ctx;
          gavl_dictionary_t * control;
          /* Store state locally */
          gavl_value_init(&val);
          gavl_msg_get_state(msg,
                             &last,
                             &ctx,
                             &var,
                             &val, NULL);
          
          new_ctx = ctx_to_global(p, ctx);
          
          if((control = gavl_control_get_create(&c->controls, new_ctx)) &&
             (control = gavl_control_get_create(control, var)))
            {
            const gavl_value_t * val_old;

            gavl_time_t ts = gavl_time_get_realtime();
            
            if(gavl_control_append_history(control, ts, &val) ||
               !(val_old = gavl_dictionary_get(control, GAVL_CONTROL_VALUE)) ||
               gavl_value_compare(&val, val_old))
              {
              int persistent = 0;
              gavl_msg_t * forward;
              forward = bg_msg_sink_get(c->ctrl.evt_sink);
              gavl_msg_set_state(forward, BG_MSG_STATE_CHANGED, last, new_ctx, var, &val);
              gavl_dictionary_set_long(&forward->header, GAVL_CONTROL_TIMESTAMP, ts);

              /* Save history */
              if(gavl_dictionary_get_int(control, GAVL_CONTROL_HISTORY_PERSISTENT, &persistent) &&
                 persistent)
                {
                char * filename;
                /* Save history */
                gavl_array_t * history;

                filename = get_history_file(new_ctx, var);
                history = gavl_control_get_history(control, NULL);

                if(!history)
                  {
                  fprintf(stderr, "History not available\n");
                  gavl_dictionary_dump(control, 2);
                  }
                
                bg_array_save_xml(history, filename, "HISTORY");
                free(filename);
                }
              
              bg_msg_sink_put(c->ctrl.evt_sink);
#if 0
              fprintf(stderr, "Controlcenter state changed: %s %s %s %s\n", ctx, new_ctx, p->path, var);
              gavl_value_dump(&val, 2);
              fprintf(stderr, "\n");
#endif         
              gavl_dictionary_set(control, GAVL_CONTROL_VALUE, &val);
              
              }
            }
          free(new_ctx);
          gavl_value_free(&val);
          }
          break;
        }
      break;
    case GAVL_MSG_NS_CONTROL:
      switch(msg->ID)
        {
        case GAVL_MSG_CONTROL_CHANGED:
          {
          const char * ctx_id;
          char * ctx_id_new = NULL;
          gavl_dictionary_t * ctrl;

          ctx_id = gavl_dictionary_get_string(&msg->header, GAVL_MSG_CONTEXT_ID);

          if(!strcmp(ctx_id, "/"))
            ctx_id_new = gavl_strdup(p->path);
          else
            ctx_id_new = gavl_sprintf("%s/%s", p->path, ctx_id);
          
          if((ctrl = gavl_control_get_create(&c->controls, ctx_id_new)))
            {
            const gavl_value_t * val;
            const gavl_dictionary_t * dict;

            /*
            fprintf(stderr, "Control changed: %s\n", ctx_id_new);
            gavl_value_dump(gavl_msg_get_arg_c(msg, 0), 2);
            fprintf(stderr, "\n");
            */
            //            gavl_dictionary_dump(ctrl, 2);
            //            fprintf(stderr, "\n");
            
            if((val = gavl_msg_get_arg_c(msg, 0)) &&
               (dict = gavl_value_get_dictionary(val)))
              gavl_dictionary_update_fields(ctrl, dict);

            //            fprintf(stderr, "Got control %s\n", ctx_id_new);
            //            gavl_dictionary_dump(dict, 2);
            //            fprintf(stderr, "\n");

            /* Forward */
            gavl_dictionary_set_string(&msg->header, GAVL_MSG_CONTEXT_ID, ctx_id_new);
            bg_msg_sink_put_copy(c->ctrl.evt_sink, msg);
            }
          else
            fprintf(stderr, ":(\n");

          if(ctx_id_new)
            free(ctx_id_new);
          
          }
          break;
        }
      break;
      
    }
  return 1;
  }

static int load_control(control_center_t * c, int *plugin_idx, const char * filename)
  {
  int result = 0;
  char * line = NULL;
  int line_alloc = 0;
  gavl_dictionary_t dict;
  gavl_io_t * io;
  const char * uri;
  const char * path;
  const char * id;
  const bg_plugin_info_t * info;
  control_plugin_t * ret = &c->plugins[*plugin_idx];

  gavl_dictionary_init(&dict);
  
  if(!(io = gavl_io_from_filename(filename, 0)))
    return 0;
  
  while(gavl_io_read_line(io, &line, &line_alloc, 1024))
    {
    gavl_strtrim(line);
    if((*line == '#') || (*line == '\0'))
      continue;
    gavl_http_parse_vars_line(&dict, line);
    }
  gavl_io_destroy(io);
  
#if 0
  fprintf(stderr, "Read control:\n");
  gavl_dictionary_dump(&dict, 2);
  fprintf(stderr, "\n");
#endif
  if(!(uri = gavl_dictionary_get_string(&dict, GAVL_META_URI)) ||
     !(id = gavl_dictionary_get_string(&dict, GAVL_META_ID)) ||
     !(path = gavl_dictionary_get_string(&dict, GAVL_CONTROL_PATH)))
    goto end;
  
  if(gavl_string_starts_with(uri, "http://") ||
     gavl_string_starts_with(uri, "https://"))
    {
    /* Load link */
    gavl_dictionary_t * parent;
    const char * label;

    parent = gavl_control_get_create(&c->controls, path);

    label = gavl_dictionary_get_string(&dict, GAVL_META_LABEL);
      
    parent = gavl_control_add_control(parent,
                                      GAVL_META_CLASS_CONTROL_LINK,
                                      id,
                                      gavl_dictionary_get_string(parent, GAVL_META_LABEL));

    if(label)
      gavl_dictionary_set_string(parent, GAVL_META_LABEL, label);
    else
      gavl_dictionary_set_string(parent, GAVL_META_LABEL, id);

    gavl_dictionary_set_string(parent, GAVL_META_URI, uri);
    gavl_dictionary_free(&dict);
    return 1;
    }
  else if((info = bg_plugin_find_by_protocol(uri, BG_PLUGIN_CONTROL)) &&
          (ret->h = bg_plugin_load(info)) &&
          (ret->c = (bg_control_plugin_t *)ret->h->plugin))
    {
    if(ret->c->open && !ret->c->open(ret->h->priv, uri))
      {
      gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Opening %s failed\n", uri);
      }
    else
      {
      gavl_dictionary_t * parent;
      const char * label;
      
      result = 1;
      
      /* Create control in the tree */
      
      parent = gavl_control_get_create(&c->controls, path);

      label = gavl_dictionary_get_string(&dict, GAVL_META_LABEL);
      
      parent = gavl_control_add_control(parent,
                                        GAVL_META_CLASS_CONTROL_GROUP,
                                        id,
                                        gavl_dictionary_get_string(parent, GAVL_META_LABEL));

      if(label)
        gavl_dictionary_set_string(parent, GAVL_META_LABEL, label);
      else
        gavl_dictionary_set_string(parent, GAVL_META_LABEL, id);
      
      ret->c->get_controls(ret->h->priv, parent);

      ret->ctrl = ret->c->common.get_controllable(ret->h->priv);
      ret->center = c;
      
      ret->sink = bg_msg_sink_create(handle_control_message, ret, 1);
      
      bg_msg_hub_connect_sink(ret->ctrl->evt_hub, ret->sink);

      if(!strcmp(path, "/"))
        ret->path = gavl_sprintf("/%s", id);
      else
        ret->path = gavl_sprintf("%s/%s", path, id);

      ret->uri = gavl_strdup(uri);

      if((ret->master = gavl_strdup(gavl_dictionary_get_string(&dict, GAVL_CONTROL_MASTER_URI))))
        gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Got Master uri: %s", ret->master);
      
      if(gavl_control_num_children(parent) < 2)
        gavl_dictionary_set_string(parent, GAVL_META_CLASS, GAVL_META_CLASS_CONTAINER_INVISIBLE);
      
      }
    }
  else
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Don't know how to load %s", uri);
    }
  
  end:
  
  gavl_dictionary_free(&dict);

  if(result)
    (*plugin_idx)++;
  
  return result;
  }

static void init_foreach_func(void * data, gavl_dictionary_t * control,
                              const char * path)
  {
  int persistent = 0;
  const char * id;

  id = gavl_dictionary_get_string(control, GAVL_META_ID);
  
  //  fprintf(stderr, "Foreach %s %s\n", path, id);

  if(gavl_dictionary_get_int(control, GAVL_CONTROL_HISTORY_PERSISTENT, &persistent) &&
     persistent)
    {
    char * filename = get_history_file(path, id);

    gavl_array_t * history =
      gavl_control_get_history(control, NULL);

    if(bg_array_load_xml(history, filename, "HISTORY"))
      gavl_log(GAVL_LOG_INFO, LOG_DOMAIN, "Loaded history for %s/%s, %d entries",
               path, id, history->num_entries);
    
    free(filename);
    }
  
  }

static void load_controls(control_center_t * c)
  {
  int i;
  glob_t gl;
  char * pattern;
  int idx = 0;
  char * dir = gavl_search_config_dir(PACKAGE, "controls");

  gavl_control_create_root(&c->controls);
  
  pattern = gavl_sprintf("%s/*.control", dir);

  glob(pattern, 0, NULL, &gl);
  c->num_plugins = gl.gl_pathc;
  c->plugins = calloc(c->num_plugins, sizeof(*c->plugins));
  
  for(i = 0; i < gl.gl_pathc; i++)
    {
    load_control(c, &idx, gl.gl_pathv[i]);
    }
#if 0
  fprintf(stderr, "Got controls:\n");
  gavl_dictionary_dump(&c->controls, 2);
  fprintf(stderr, "\n");
#endif
  c->num_plugins = idx;
  
  globfree(&gl);
  
  free(pattern);
  free(dir);

  gavl_control_foreach(&c->controls, init_foreach_func, "/", NULL);
  
  }

static int handle_ctrl(bg_http_connection_t * conn, void * data)
  {
  int ret = 0;
  char * path_decoded;
  control_center_t * c = data;

  path_decoded = bg_uri_to_string(conn->path, -1);

  if(path_decoded)
    {
    if(gavl_control_get(&c->controls, path_decoded))
      {
      bg_http_connection_send_file(conn, DATA_DIR "/web/controlcenter.html");
      ret = 1;
      }
  
    free(path_decoded);
    }
  
  return ret;
  }

static const gavl_dictionary_t *
find_dict_from_conn(const gavl_dictionary_t * controls, const bg_http_connection_t * conn)
  {
  char * real_path;
  char * pos;
    
  const gavl_dictionary_t * ret = NULL;
  if(*conn->path != '/')
    return NULL;

  real_path = gavl_strdup(conn->path);
  if((pos = strchr(real_path, '?')))
    *pos = '\0';

  ret = gavl_control_get(controls, real_path);
  
  free(real_path);
  
  return ret;
  }

static int val_from_conn(gavl_value_t * val, const gavl_dictionary_t * control,
                         const bg_http_connection_t * conn)
  {
  const char * v;
  gavl_value_init(val);
  val->type = gavl_control_get_type(control);

  if(!(v = gavl_dictionary_get_string(&conn->url_vars, "v")) ||
     !gavl_value_from_string(val, v))
    return 0;

  
  return 1;
  }

static int handle_set_internal(bg_http_connection_t * conn, void * data, int rel)
  {
  gavl_value_t val;
  control_center_t * c = data;
  const gavl_dictionary_t * control = find_dict_from_conn(&c->controls, conn);

  if(!control)
    bg_http_connection_init_res(conn, conn->protocol, 404, "Not Found");
  else if(!val_from_conn(&val, control, conn))
    bg_http_connection_init_res(conn, conn->protocol, 400, "Bad Request");
  else
    {
    control_plugin_t * p;

    
    if(!(p = get_plugin(c, conn->path)))
      bg_http_connection_init_res(conn, conn->protocol, 404, "Not Found");
    else
      {
      char * ctx = NULL;
      char * var = NULL;
      char * pos;
      gavl_msg_t * cmd;

      int delay = 0;
      gavl_dictionary_get_int(&conn->url_vars, "delay", &delay);
      
      conn->path += strlen(p->path);
      if(*conn->path != '/')
        {
        /* Error */
        bg_http_connection_init_res(conn, conn->protocol, 404, "Not Found");
        }
      else
        {
        /* /var */
        if((pos = strrchr(conn->path, '/')) == conn->path)
          {
          ctx = gavl_strdup("/");
          var = gavl_strdup(conn->path + 1);
          }
        else
          {
          /* /ctx/var */
          ctx = gavl_strndup(conn->path, pos);
          var = gavl_strdup(pos+1);
          }
        bg_http_connection_init_res(conn, conn->protocol, 200, "OK");

        if((pos = strchr(var, '?')))
          *pos = '\0';
        
        if(rel)
          {
          char * tmp_string;

          if(!strcmp(ctx, "/"))
            tmp_string = gavl_sprintf("%s", p->path);
          else
            tmp_string = gavl_sprintf("%s%s", p->path, ctx);
          
          if(!gavl_control_handle_set_rel(&c->controls, tmp_string, var, &val))
            {
            /* Error */
            gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Set relative failed for path %s ctx %s var %s", p->path, ctx, var);
            }
          }
        
        cmd = bg_msg_sink_get(p->ctrl->cmd_sink);
        gavl_msg_set_state(cmd, BG_CMD_SET_STATE, 1, ctx, var, &val);

        if(delay > 0)
          {
          gavl_dictionary_set_int(&cmd->header, GAVL_CONTROL_DELAY, delay);
          }

        bg_msg_sink_put(p->ctrl->cmd_sink);
        
        }
      if(ctx)
        free(ctx);
      if(var)
        free(var);
      
      }
    
    }

  bg_http_connection_write_res(conn);
  return 1;

  }

static int handle_set(bg_http_connection_t * conn, void * data)
  {
  return handle_set_internal(conn, data, 0);
  }

static int handle_setrel(bg_http_connection_t * conn, void * data)
  {
  return handle_set_internal(conn, data, 1);
  }

static int handle_get(bg_http_connection_t * conn, void * data)
  {
  control_center_t * c = data;
  const gavl_value_t * val;
  const gavl_dictionary_t * control = find_dict_from_conn(&c->controls, conn);
  char * body = NULL;
  int body_len = 0;
  
  if(!control)
    {
    bg_http_connection_init_res(conn, conn->protocol, 404, "Not Found");
    }
  else if(!(val = gavl_dictionary_get(control, GAVL_CONTROL_VALUE)))
    {
    bg_http_connection_init_res(conn, conn->protocol, 400, "Bad Request");

    fprintf(stderr, "No value:\n");
    gavl_dictionary_dump(control, 2);

    }
  else if(!(body = gavl_value_to_string(val)))
    {
    bg_http_connection_init_res(conn, conn->protocol, 400, "Bad Request");
    }
  else
    {
    body_len = strlen(body);
    bg_http_connection_init_res(conn, conn->protocol, 200, "OK");
    gavl_dictionary_set_string(&conn->res, "Content-Type", "text/plain");
    gavl_dictionary_set_int(&conn->res, "Content-Length", body_len);
    }
  
  bg_http_connection_write_res(conn);
  if(body && (gavl_socket_write_data(conn->fd, body, body_len) != body_len))
    bg_http_connection_clear_keepalive(conn);
  
  return 1;
  }

int controlcenter_init(control_center_t * c)
  {
  memset(c, 0, sizeof(*c));
  
  c->srv = bg_http_server_create();
  bg_http_server_set_default_port(c->srv, 8886); 
  
  
  /* Must be first */
  bg_http_server_set_root_file(c->srv, "/static2/controlcenter.html");

  bg_http_server_add_handler(c->srv, handle_ctrl, BG_HTTP_PROTO_HTTP, "/ctrl", c);

  /* Simple handlers for remote scripts (callable also via curl or wget) */
  bg_http_server_add_handler(c->srv, handle_setrel, BG_HTTP_PROTO_HTTP, "/setrel", c);
  bg_http_server_add_handler(c->srv, handle_set, BG_HTTP_PROTO_HTTP, "/set", c);
  bg_http_server_add_handler(c->srv, handle_get, BG_HTTP_PROTO_HTTP, "/get", c);
  
  bg_http_server_set_static_path(c->srv, "/static");
  bg_http_server_add_static_path(c->srv, "/static2", DATA_DIR "/web");
  
  bg_http_server_add_handler(c->srv, server_handle_manifest, BG_HTTP_PROTO_HTTP, NULL, c);

  bg_controllable_init(&c->ctrl,
                       bg_msg_sink_create(handle_websocket_message, c, 1),
                       bg_msg_hub_create(1));

  /* Initialize controls */
  load_controls(c);
  
  c->ws = bg_websocket_context_create(GAVL_META_CLASS_BACKEND_CONTROLPANEL, NULL, &c->ctrl);
  
  bg_http_server_start(c->srv);

  return 1;
  }

void controlcenter_cleanup(control_center_t * c)
  {
  bg_http_server_destroy(c->srv);
  bg_websocket_context_destroy(c->ws);
  }

static int update_controls(control_center_t * c)
  {
  int i;
  int ret = 0;
  for(i = 0; i < c->num_plugins; i++)
    {
    ret += c->plugins[i].c->update(c->plugins[i].h->priv);    
    }
  return ret;
  }

int controlcenter_iteration(control_center_t * c)
  {
  int ret = 0;
  
  ret += bg_http_server_iteration(c->srv);
  ret += bg_websocket_context_iteration(c->ws);
  ret += update_controls(c);
  
  return ret;
  }

char * get_history_file(const char * ctx, const char * var)
  {
  char * ret;
  char * path;
  char md5[GAVL_MD5_LENGTH];
  const char * home = getenv("HOME");
  if(!home)
    return NULL;
  
  path = gavl_sprintf("%s/%s", ctx, var);
  
  gavl_md5_buffer_str(path, strlen(path), md5);

  ret = gavl_sprintf("%s/.local/%s/history", home, PACKAGE);
  gavl_ensure_directory(ret, 0);

  ret = gavl_strcat(ret, "/");
  ret = gavl_strcat(ret, md5);
  
  free(path);
  
  return ret;
  }


/* Test mode */

#ifdef TEST_CONTROLS

static void init_controls_test(const char * path, gavl_array_t * arr)
  {
  gavl_value_t val;
  gavl_dictionary_t * dict;
  gavl_value_t def;
  double * rgb;
  if(!strcmp(path, "/"))
    {

    /* Button */
    gavl_value_init(&val);
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_BUTTON);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "/button");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Button");
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_free(&val);

    /* Slider */
    gavl_value_init(&val);
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_SLIDER);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "slider");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Slider");
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MIN, 0);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MAX, 100);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_VALUE, 20);
    gavl_dictionary_set_string(dict, GAVL_CONTROL_UNIT, "%");
    gavl_control_set_type(dict, GAVL_TYPE_INT);
    
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);
    
    /* Power */
    gavl_value_init(&val);
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_POWERBUTTON);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "power");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Power button");
    gavl_dictionary_set_int(dict, GAVL_CONTROL_VALUE, 1);
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);

    /* Meter */
    gavl_value_init(&val);
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_METER);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "meter");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Meter");
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MIN, 0);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MAX, 100);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_LOW, 30);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_HIGH, 70);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_VALUE, 50);

    gavl_dictionary_set_string(dict, GAVL_CONTROL_UNIT, "%");
    gavl_control_set_type(dict, GAVL_TYPE_INT);
    
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);

    /* Menu */
    gavl_value_init(&val);
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_PULLDOWN);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "pulldown");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Pulldown");

    gavl_control_add_option(dict, "option1", "Option 1");
    gavl_control_add_option(dict, "option2", "Option 2");
    gavl_dictionary_set_string(dict, GAVL_CONTROL_VALUE, "option2");
    
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);

    /* Volume */
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_VOLUME);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "volume");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Volume");
        
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MIN, 0);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_MAX, 100);
    gavl_dictionary_set_int(dict, GAVL_CONTROL_VALUE, 50);
    gavl_control_set_type(dict, GAVL_TYPE_INT);
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);

    /* Color */
    dict = gavl_value_set_dictionary(&val);
    gavl_dictionary_set_string(dict, GAVL_META_CLASS, GAVL_META_CLASS_CONTROL_RGBCOLOR);
    gavl_dictionary_set_string(dict, GAVL_META_ID,    "color");
    gavl_dictionary_set_string(dict, GAVL_META_LABEL, "Color");
    gavl_value_init(&def);
    rgb = gavl_value_set_color_rgb(&def);
    rgb[0] = 0.0;
    rgb[1] = 1.0;
    rgb[2] = 0.0;
    gavl_dictionary_set(dict, GAVL_CONTROL_VALUE, &def);
    gavl_value_free(&def);
    
    
    gavl_array_splice_val_nocopy(arr, -1, 0, &val);
    gavl_value_reset(&val);

    
    }
  
  
  }

#endif

