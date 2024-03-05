#include <ctype.h>
#include <string.h>
#include <glob.h>
#include <unistd.h>

#include <config.h>

#include <control.h>
#include <gmerlin/translation.h>

#include <gmerlin/plugin.h>
#include <gmerlin/state.h>
#include <gmerlin/utils.h>


typedef struct
  {
  bg_controllable_t ctrl;
  gavl_time_t last_poll_time;
  gavl_dictionary_t state;
  
  int64_t last_cpu_total;
  int64_t last_cpu_active;

  gavl_dictionary_t temperature_sensors;
  
  } sysinfo_t;

#define SKIP_SPACE() while(isspace(*pos)) pos++

static double get_cpu_load(sysinfo_t * s)
  {
  char * pos, *rest;
  char * line = NULL;
  int alloc = 0;
  int idx;
  int64_t cpu_total = 0;
  int64_t cpu_idle = 0;
  int64_t val;
  double ret;
  gavl_io_t * io = gavl_io_from_filename("/proc/stat", 0);


  if(!io)
    return 0;
  
  while(1)
    {
    if(!gavl_io_read_line(io, &line, &alloc, 0))
      {
      gavl_io_destroy(io);
      if(line)
        free(line);
      return 0.0;
      }
    if(gavl_string_starts_with(line, "cpu "))
      break;
    }

  pos = line + 4;
  SKIP_SPACE();
  
  idx = 0;
  while(1)
    {
    SKIP_SPACE();
    if(*pos == '\0')
      break;
    val = strtoll(pos, &rest, 10);
    if(val < 0)
      break;
    if(pos == rest)
      break;

    if(idx == 3)
      cpu_idle = val;
    cpu_total += val;
    
    idx++;
    pos = rest;
    }
  
  gavl_io_destroy(io);
  if(line)
    free(line);

  /*
    fprintf(stderr, "%"PRId64" %"PRId64" %"PRId64" %"PRId64"\n",
          cpu_total - cpu_idle, s->last_cpu_active,
          cpu_total, s->last_cpu_total);
  */
  
  ret =
    (double)((cpu_total - cpu_idle) - s->last_cpu_active) /
    (double)(cpu_total - s->last_cpu_total);

  s->last_cpu_active = cpu_total - cpu_idle;
  s->last_cpu_total = cpu_total;
  return ret * 100.0;
  }

static double get_memory_usage(void)
  {
  char * line = NULL;
  int alloc = 0;

  int64_t total = 0;
  int64_t available = 0;
  
  gavl_io_t * io = gavl_io_from_filename("/proc/meminfo", 0);
  const char * pos;
  
  if(!io)
    return 0;
  
  while(1)
    {
    if(!gavl_io_read_line(io, &line, &alloc, 0))
      {
      if(line)
        free(line);
      return 0.0;
      }
    if(gavl_string_starts_with(line, "MemTotal:"))
      {
      pos = strchr(line, ':');
      pos++;
      total = strtoll(pos, NULL, 10);
      }
    else if(gavl_string_starts_with(line, "MemAvailable:"))
      {
      pos = strchr(line, ':');
      pos++;
      available = strtoll(pos, NULL, 10);
      }
    if((total > 0) && (available > 0))
      break;
    }

  if(line)
    free(line);
  if(io)
    gavl_io_destroy(io);
  
  return 100.0 * (double)(total - available) / (double)total;
  }

static int read_hwmon(const char * filename, char ** line, int * line_alloc)
  {
  int ret;
  gavl_io_t * io;

  if(access(filename, R_OK) ||
     !(io = gavl_io_from_filename(filename, 0)))
    return 0;
  
  ret = gavl_io_read_line(io, line, line_alloc, 0);
  gavl_io_destroy(io);
  return ret;
  }

static void get_temperature_controls(sysinfo_t * s, gavl_dictionary_t * parent)
  {
  int i;
  glob_t g;
  char * line = NULL;
  int alloc = 0;

  glob("/sys/class/hwmon/*", 0, NULL, &g);

  
  for(i = 0; i < g.gl_pathc; i++)
    {
    int j;
    gavl_dictionary_t * ctrl;
    double temp;
    glob_t g1;
    char * filename;
    char * name = NULL;
    
    char * pattern = gavl_sprintf("%s/temp*_input", g.gl_pathv[i]);
    glob(pattern, 0, NULL, &g1);
    free(pattern);

    filename = gavl_sprintf("%s/name", g.gl_pathv[i]);
    if(read_hwmon(filename, &line, &alloc))
      {
      name = line;
      line = NULL;
      alloc = 0;
      }
    free(filename);
    
    for(j = 0; j < g1.gl_pathc; j++)
      {
      char * id;
      char * label = NULL;
      char * prefix = NULL;
      
      
      if(!read_hwmon(g1.gl_pathv[j], &line, &alloc))
        continue;
      temp = (double)atoi(line) / 1000.0;
      
      prefix = gavl_strndup(g1.gl_pathv[j], strstr(g1.gl_pathv[j], "_input"));
      
      id = gavl_sprintf("temp_%d_%d", i, j);

      filename = gavl_sprintf("%s_label", prefix);
      if(read_hwmon(filename, &line, &alloc))
        {
        label = line;
        line = NULL;
        alloc = 0;
        }
      free(filename);

      if(!label)
        {
        if(name)
          {
          if(g1.gl_pathc == 1)
            label = gavl_strdup(name);
          else
            label = gavl_sprintf("%s Sensor %d", name, j+1);
          }
        else
          label = gavl_sprintf("Chip %d Sensor %d", i+1, j+1);
        }
      
      ctrl = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_METER,
                                      id, label);
      
      /* Degree sign in UTF-8: 0xC2 0xB0 */
      gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, (char[]){ 0xC2, 0xB0, 'C', 0x00} );
      gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN,     0.0);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE,   temp);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW,    30.0);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);
      
      /* MAX */
      temp = 100.0;

      filename = gavl_sprintf("%s_crit", prefix);
      if(read_hwmon(filename, &line, &alloc))
        {
        temp = (double)atoi(line) / 1000.0;
        }
      free(filename);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX,   temp);

      /* High */
      temp = 70.0;
      filename = gavl_sprintf("%s_max", prefix);
      if(read_hwmon(filename, &line, &alloc))
        {
        temp = (double)atoi(line) / 1000.0;
        }
      free(filename);
      gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH,   temp);
      
      gavl_dictionary_set_string(&s->temperature_sensors, id, g1.gl_pathv[j]);
      
      if(label)
        free(label);
      if(id)
        free(id);
      if(prefix)
        free(prefix);
      
      
      }

    if(name)
      free(name);
    
    globfree(&g1);
    
    
    //    fprintf(stderr, "Got temp: %f\n", temp);
    
    }

  if(line)
    free(line);
  
  globfree(&g);
  }

static void temperature_foreach_func(void * priv, const char * name,
                                     const gavl_value_t * val)
  {
  gavl_value_t v;
  char * line = NULL;
  int alloc = 0;
  sysinfo_t * s = priv;
  const char * filename = gavl_value_get_string(val);
  
  if(read_hwmon(filename, &line, &alloc))
    {
    gavl_value_init(&v);
    gavl_value_set_float(&v, (double)atoi(line) / 1000.0);
    
    bg_state_set(&s->state, 0, NULL, name, 
                 &v, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
    gavl_value_reset(&v);

    }
  if(line)
    free(line);
  }

static int do_poll(sysinfo_t * s)
  {
  gavl_buffer_t buf;
  gavl_value_t val;
  gavl_buffer_init(&buf);

  /* System load */
  
  gavl_value_init(&val);
  gavl_value_set_float(&val, get_cpu_load(s));
  
  bg_state_set(&s->state, 0, NULL, "load", 
               &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  gavl_value_reset(&val);
  
  /* Memory usage */
  gavl_value_init(&val);
  gavl_value_set_float(&val, get_memory_usage());
  
  bg_state_set(&s->state, 0, NULL, "memory", 
               &val, s->ctrl.evt_sink, BG_MSG_STATE_CHANGED);
  gavl_value_reset(&val);
  
  /* Temperature sensors */
  gavl_dictionary_foreach(&s->temperature_sensors, 
                          temperature_foreach_func,
                          s);
  return 1;
  
  }

static int update_sysinfo(void * priv)
  {
  sysinfo_t * s = priv;
  gavl_time_t cur = gavl_time_get_monotonic();
  
  if((s->last_poll_time == GAVL_TIME_UNDEFINED) ||
     (cur - s->last_poll_time >= 2 * GAVL_TIME_SCALE))
    {
    do_poll(s);
    s->last_poll_time = cur;
    return 1;
    }
  else
    return 0;
  }

static int open_sysinfo(void * priv, const char * uri)
  {
  sysinfo_t * s = priv;
  s->last_poll_time = GAVL_TIME_UNDEFINED;
  return 1; 
  }

static void get_controls_sysinfo(void * priv, gavl_dictionary_t * parent)
  {
  gavl_dictionary_t * ctrl;
  gavl_value_t val1, val2;
  sysinfo_t * s = priv;

  gavl_value_init(&val1);
  gavl_value_init(&val2);
  
  ctrl = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_METER,
                                  "load", "CPU Load");
  
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN,     0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX,   100.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW,    50.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH,   90.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE,   0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);

  ctrl = gavl_control_add_control(parent, GAVL_META_CLASS_CONTROL_METER,
                                  "memory", "Memory usage");
  
  gavl_dictionary_set_string(ctrl, GAVL_CONTROL_UNIT, "%");
  gavl_control_set_type(ctrl, GAVL_TYPE_FLOAT);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MIN,     0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_MAX,   100.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_LOW,    50.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_HIGH,   90.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_VALUE,   0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_OPTIMUM, 0.0);
  gavl_dictionary_set_float(ctrl, GAVL_CONTROL_DIGITS, 2);

  get_temperature_controls(s, parent);
  
  }

static void * create_sysinfo()
  {
  sysinfo_t * s = calloc(1, sizeof(*s));

  bg_controllable_init(&s->ctrl,
                       bg_msg_sink_create(NULL, s, 1),
                       bg_msg_hub_create(1));
  
  return s;
  }

static void destroy_sysinfo(void *priv)
  {
  sysinfo_t * s = priv;
  bg_controllable_cleanup(&s->ctrl);
  free(s);
  }

static bg_controllable_t * get_controllable_sysinfo(void * priv)
  {
  sysinfo_t * s = priv;
  return &s->ctrl;
  }


bg_control_plugin_t the_plugin =
  {
  .common =
    {
    BG_LOCALE,
    .name =      "ctrl_sysinfo",
    .long_name = TRS("System info"),
    .description = TRS("Show system information"),
    .type =     BG_PLUGIN_CONTROL,
    .flags =    0,
    .create =   create_sysinfo,
    .destroy =   destroy_sysinfo,
    .get_controllable =   get_controllable_sysinfo,
    .priority =         1,
    },
  
  .protocols = "sysinfo",

  /* Update the internal state, send messages. A zero return value incicates that
     nothing important happened and the client can savely sleep (e.g. for some 10s of
     milliseconds) before calling this function again. */
  
  .update = update_sysinfo,
  .open   = open_sysinfo,
  .get_controls   = get_controls_sysinfo,
  
  } ;

/* Include this into all plugin modules exactly once
   to let the plugin loader obtain the API version */
BG_GET_PLUGIN_API_VERSION;
