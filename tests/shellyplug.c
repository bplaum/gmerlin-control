/*****************************************************************
 * gmerlin - a general purpose multimedia framework and applications
 *
 * Copyright (c) 2001 - 2012 Members of the Gmerlin project
 * gmerlin-general@lists.sourceforge.net
 * http://gmerlin.sourceforge.net
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * *****************************************************************/

#include <string.h>

#include <config.h>

#include <gavl/gavl.h>
#include <gavl/log.h>
#include <gavl/http.h>

#include <gmerlin/cmdline.h>
#include <gmerlin/application.h>
#include <gmerlin/utils.h>
#include <gmerlin/bggavl.h>

#include <gmerlin/translation.h>

#define CMD_ON     1
#define CMD_OFF    2
#define CMD_STATUS 3
#define CMD_POWER  4

static int cmd = -1;
static int delay = 0;

static char * address = NULL;

gavl_io_t * io = NULL;

#define LOG_DOMAIN "energenie"

// #define DUMP_REQUESTS

static void opt_addr(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -addr requires an argument\n");
    exit(-1);
    }

  address = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static void opt_on(void * data, int * argc, char *** _argv, int arg)
  {
  cmd = CMD_ON;
  }

static void opt_off(void * data, int * argc, char *** _argv, int arg)
  {
  cmd = CMD_OFF;
  }

static void opt_status(void * data, int * argc, char *** _argv, int arg)
  {
  cmd = CMD_STATUS;
  }

static void opt_power(void * data, int * argc, char *** _argv, int arg)
  {
  cmd = CMD_POWER;
  }

static void opt_delay(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -delay requires an argument\n");
    exit(-1);
    }
  
  delay = atoi((*_argv)[arg]);
  
  bg_cmdline_remove_arg(argc, _argv, arg);
  }

static bg_cmdline_arg_t global_options[] =
  {
     {
       .arg =         "-addr",
       .help_arg =    "<addr>",
       .help_string = TRS("IP Address"),
       .callback    = opt_addr,
     },
     {
       .arg =         "-on",
       .help_string = "Switch on",
       .callback    = opt_on,
     },
     {
       .arg =         "-off",
       .help_string = "Switch off",
       .callback    = opt_off,
     },
     {
       .arg =         "-status",
       .help_string = "Print status",
       .callback    = opt_status,
     },
     {
       .arg =         "-power",
       .help_string = "Get power consumption",
       .callback    = opt_power,
     },
     {
       .arg =         "-delay",
       .help_arg = "<delay>",
       .help_string = "Delay in seconds",
       .callback    = opt_delay,
     },
     { /* */ }
  };

const bg_cmdline_app_data_t app_data =
  {
    .package =  "shellyplug",
    .version =  "1.0.0",
    .synopsis = "[options]\n",
    .help_before = "Energenie control\n",
    .args = (bg_cmdline_arg_array_t[]) { { "Options", global_options },
                                       {  } },
#if 0
    .files = (bg_cmdline_ext_doc_t[])
    { { "~/.gmerlin/plugins.xml",
        TRS("Cache of the plugin registry (shared by all applications)") },
      { "~/.gmerlin/generic/cfg.xml",
        TRS("Default plugin parameters are read from there. Use gmerlin_plugincfg to change them.") },
      { /* End */ }
    },
#endif
  };

static int http_send_request(const char * uri,
                             gavl_buffer_t * res_body)
  {
  int ret;
  gavl_io_t * c = gavl_http_client_create();
  
  gavl_http_client_set_response_body(c, res_body);
  ret = gavl_http_client_open(c, "GET", uri);
  gavl_io_destroy(c);
  return ret;
  }

static void get_power()
  {
  char * uri = gavl_sprintf("http://%s/meter/0", address);
  json_object * obj = bg_json_from_url(uri, NULL);

  if(!(obj = bg_json_from_url(uri, NULL)) ||
     !json_object_is_type(obj, json_type_object))
    goto fail;
  
  printf("%f\n", bg_json_dict_get_double(obj, "power"));
  fail:

  free(uri);
  if(obj)
    json_object_put(obj);
  
  }

static void get_socket()
  {
  char * uri = gavl_sprintf("http://%s/relay/0", address);
  json_object * obj = bg_json_from_url(uri, NULL);

  if(!(obj = bg_json_from_url(uri, NULL)) ||
     !json_object_is_type(obj, json_type_object))
    goto fail;
  
  printf("%d\n", bg_json_dict_get_bool(obj, "ison"));
  
  fail:
  
  free(uri);
  if(obj)
    json_object_put(obj);

  }


static int set_socket(int on)
  {
  int ret;
  char * uri;

  if(delay)
    uri = gavl_sprintf("http://%s/relay/0?turn=%s&timer=%d", address, (on ? "off" : "on"), delay);
  else
    uri = gavl_sprintf("http://%s/relay/0?turn=%s", address, (on ? "on" : "off"));
  ret = http_send_request(uri, NULL);
  free(uri);
  return ret;
  }



int main(int argc, char ** argv)
  {
  bg_app_init("shellyplug", "Client for Shelly Plug S", NULL);
  
  /* Get commandline options */
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  if(!address)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Address not given (use -addr)");
    goto fail;
    }
  
  if(cmd == -1)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Command not given (use -on, -off, -status or power)");
    goto fail;
    }
  
  switch(cmd)
    {
    case CMD_ON:
      set_socket(1);
      break;
    case CMD_OFF:
      set_socket(0);
      break;
    case CMD_STATUS:
      get_socket();
      break;
    case CMD_POWER:
      get_power();
      break;
    }
  
  return EXIT_SUCCESS;

  fail:
  return EXIT_FAILURE;
  
  }

