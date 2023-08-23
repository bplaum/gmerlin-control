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


#define _XOPEN_SOURCE       /* See feature_test_macros(7) */
#include <time.h>

#include <string.h>

#include <config.h>

#include <gavl/gavl.h>
#include <gavl/gavlsocket.h>
#include <gavl/log.h>
#include <gavl/numptr.h>

#include <gmerlin/http.h>
#include <gmerlin/cmdline.h>
#include <gmerlin/application.h>
#include <gmerlin/utils.h>
#include <gmerlin/translation.h>


#define CMD_ON     1
#define CMD_OFF    2
#define CMD_STATUS 3

static int socket_nr = -1;
static int cmd = -1;
static int delay = 0;

static char * address = NULL;
static char * password = "1";

gavf_io_t * io = NULL;

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

static void opt_password(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -password requires an argument\n");
    exit(-1);
    }

  password = (*_argv)[arg];
  bg_cmdline_remove_arg(argc, _argv, arg);
  }


static void opt_s(void * data, int * argc, char *** _argv, int arg)
  {
  if(arg >= *argc)
    {
    fprintf(stderr, "Option -s requires an argument\n");
    exit(-1);
    }

  socket_nr = atoi((*_argv)[arg]);
  
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
       .arg =         "-password",
       .help_arg =    "<password>",
       .help_string = TRS("Password"),
       .callback    = opt_password,
     },
     {
       .arg =         "-s",
       .help_arg =    "<nr>",
       .help_string = "Set socket Nr. (1-4)",
       .callback    = opt_s,
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
       .help_string = "Switch off",
       .callback    = opt_status,
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
    .package =  "energenie",
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

static int http_send_request(gavl_dictionary_t * request,
                             const char * req_body,
                             gavl_buffer_t * res_body)
  {
  int ret = 0;
  gavl_socket_address_t * addr = NULL;
  int fd;
  gavl_dictionary_t response;
  int body_len = 0;
  
  gavl_dictionary_init(&response);
  
  addr = gavl_socket_address_create();
  
  if(!gavl_socket_address_set(addr, address, 80, SOCK_STREAM))
    goto fail;

  if((fd = gavl_socket_connect_inet(addr, 5000)) < 0)
    goto fail;

  if(!io)
    io = gavf_io_create_socket(fd, 30000, GAVF_IO_SOCKET_DO_CLOSE);
  
  if(req_body)
    {
    body_len = strlen(req_body);
    gavl_dictionary_set_string(request, "Content-Type", "application/x-www-form-urlencoded");
    gavl_dictionary_set_string_nocopy(request, "Content-Length", gavl_sprintf("%d", body_len));
    }

  gavl_dictionary_set_string(request, "Host", address);
  
#ifdef DUMP_REQUESTS
  fprintf(stderr, "Sending request:\n");
  gavl_dictionary_dump(request, 2);
#endif  

  if(!gavl_http_request_write(io, request))
    goto fail;

  if(req_body)
    gavf_io_write_data(io, (const uint8_t*)req_body, body_len);
  
  if(!gavl_http_response_read(io, &response))
    goto fail;

#ifdef DUMP_REQUESTS
  fprintf(stderr, "Read response:\n");
  gavl_dictionary_dump(&response, 2);
#endif  
  
  if(res_body)
    ret = gavl_http_read_body(io, &response, res_body);
  else
    ret = 1;
  
  fail:

  if(addr)
    gavl_socket_address_destroy(addr);

  gavl_dictionary_free(&response);

  
  return ret;
  }

static int login()
  {
  gavl_buffer_t res_body;
  char * req_body;
  int ret = 0;
  gavl_dictionary_t request;

  gavl_dictionary_init(&request);

  req_body = gavl_sprintf("pw=%s", password);

  gavl_http_request_init(&request, "POST", "/login.html", "HTTP/1.1");

  gavl_buffer_init(&res_body);
  
  if(!http_send_request(&request, req_body, &res_body))
    goto fail;

  if(strstr((char*)res_body.buf,
            "there is an active session with this device at this moment"))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Could not log in: Device busy");
    goto fail;
    }
  ret = 1;
  fail:
  
  gavl_buffer_free(&res_body);
  gavl_dictionary_free(&request);
  return ret;
  }

static int logout()
  {
  gavl_buffer_t res_body;
  int ret = 0;
  gavl_dictionary_t request;

  gavf_io_destroy(io);
  io = NULL;

  /* Read the login page */
    
  gavl_dictionary_init(&request);

  gavl_http_request_init(&request, "GET", "/login.html", "HTTP/1.1");
  gavl_dictionary_set_string(&request, "Accept", "*/*");
  gavl_dictionary_set_string_nocopy(&request, "Referer", bg_sprintf("http://%s/", address));
  
  gavl_buffer_init(&res_body);
  
  if(!http_send_request(&request, NULL, &res_body))
    goto fail;
  
  ret = 1;
  fail:
  
  
  gavl_buffer_free(&res_body);
  gavl_dictionary_free(&request);
  return ret;
  }

/* Switch on or off with a delay. We do this by setting a schedule */

/* A schedule is defined by a hex string */

/*

  4 bytes: Current time of client (as time_t)
  1 byte flags: 0x01: switch on
                0x02: periodically
  4 bytes: Time of action (as time_t)
  1 byte: always 0xe5
  4 bytes: Loop time (0 if unused)
*/

#define SCHEDULE_LEN 14

static int set_socket_delay(int on)
  {
  uint8_t flags = 0;
  gavl_dictionary_t request;
  time_t curtime = time(NULL);

  uint8_t schedule_bin[SCHEDULE_LEN];
  char schedule_asc[SCHEDULE_LEN*2+1];
  char zero = 0;
  uint8_t * ptr;
  uint32_t u;
  int i;
  
  char * sch[4];
  char * payload;

  int ret = 0;
  
  if(!login())
    return 0;

  gavl_dictionary_init(&request);
  gavl_http_request_init(&request, "POST", "/", "HTTP/1.1");

  if(on)
    flags |= 0x01;

  ptr = &schedule_bin[0];

  u = (uint32_t)curtime;
  
  GAVL_32LE_2_PTR(u, ptr); ptr+=4;
  *ptr = flags; ptr++;
  u = (uint32_t)(curtime + delay);
  GAVL_32LE_2_PTR(u, ptr); ptr+=4;
  *ptr = 0xe5; ptr++;
  u = 0;
  GAVL_32LE_2_PTR(u, ptr);

  for(i = 0; i < SCHEDULE_LEN; i++)
    {
    snprintf(&schedule_asc[2*i], 3, "%02x", schedule_bin[i]);
    }

  for(i = 0; i < 4; i++)
    {
    if(i + 1 == socket_nr)
      sch[i] = schedule_asc;
    else
      sch[i] = &zero;
    }

  payload = gavl_sprintf("sch1=%s&sch2=%s&sch3=%s&sch4=%s", sch[0],  sch[1], sch[2], sch[3]);
  
  fprintf(stderr, "Payload: %s\n", payload);
  
  if(!http_send_request(&request, payload, NULL))
    goto fail;

  logout();

  ret = 1;
  fail:
  
  free(payload);
  gavl_dictionary_free(&request);
  return ret;
  
  }


static int set_socket(int on)
  {
  char * req_body;
  gavl_dictionary_t request;
  int ret;
  
  gavl_dictionary_init(&request);

  gavl_http_request_init(&request, "POST", "/", "HTTP/1.1");
  
  if(!login())
    return 0;
  
  req_body = gavl_sprintf("cte%d=%d", socket_nr, on);
  
  if(!http_send_request(&request, req_body, NULL))
    goto fail;

  logout();
  
  printf("%d\n", on);
  
  ret = 1;
  fail:
  
  free(req_body);
  gavl_dictionary_free(&request);
  return ret;
  
  }

static void get_socket()
  {
  char * path = NULL;
  gavl_dictionary_t request;
  gavl_buffer_t res_body;
  char * pos;

  int ctl[4];
  
  if(!login())
    return;

  gavl_buffer_init(&res_body);
  
  path = gavl_sprintf("/status.html?sn=%d", socket_nr);
    
  gavl_dictionary_init(&request);

  gavl_http_request_init(&request, "GET", path, "HTTP/1.1");

  if(!http_send_request(&request, NULL, &res_body))
    goto fail;

  //  gavl_hexdump(res_body.buf, res_body.len, 16);
  
  if(!(pos = strstr((char*)res_body.buf, "var ctl = [")))
    goto fail;
  
  pos+=11;

  if(sscanf(pos, "%d,%d,%d,%d", &ctl[0], &ctl[1], &ctl[2], &ctl[3]) < 4)
    goto fail;

  printf("%d\n", ctl[socket_nr - 1]);

  logout();

  fail:

  gavl_dictionary_free(&request);
    
  
  }



int main(int argc, char ** argv)
  {
  bg_app_init("energenie", "Energenie control");
    
  /* Get commandline options */
  bg_cmdline_init(&app_data);
  bg_cmdline_parse(global_options, &argc, &argv, NULL);

  if(!address)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Address not given (use -addr)");
    goto fail;
    }

  
  if(socket_nr == -1)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Socket number not given (use -s)");
    goto fail;
    }
  else if((socket_nr < 1) || (socket_nr > 4))
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Invalid socket number");
    goto fail;
    }
  
  if(cmd == -1)
    {
    gavl_log(GAVL_LOG_ERROR, LOG_DOMAIN, "Command not given (use -on, -off or -status)");
    goto fail;
    }
  
  switch(cmd)
    {
    case CMD_ON:
      if(delay)
        set_socket_delay(1);
      else
        set_socket(1);
      break;
    case CMD_OFF:
      if(delay)
        set_socket_delay(0);
      else
        set_socket(0);
      break;
    case CMD_STATUS:
      get_socket();
      break;
    }

  if(io)
    gavf_io_destroy(io);
  
  return EXIT_SUCCESS;

  fail:
  return EXIT_FAILURE;
  
  }

