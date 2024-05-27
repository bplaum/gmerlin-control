
/* Utility functions */

export function path_encode(path)
  {
  let i;
  let arr = path.split("/");

  for(i = 0; i < arr.length; i++)
    {
    arr[i] = encodeURIComponent(arr[i]);
    }

  return arr.join("/");
  }

export function path_decode(path)
  {
  let i;
  let arr = path.split("/");

  for(i = 0; i < arr.length; i++)
    {
    arr[i] = decodeURIComponent(arr[i]);
    }

  return arr.join("/");
  }

function create_set_state_msg(path, id)
  {
  let pos = path.lastIndexOf("/");
  let msg = msg_create(id, BG_MSG_NS_STATE);

  console.log("create_set_state_msg " + path + " " + pos);
    
  msg_set_arg_int(msg, 0, 1); // Last
  msg_set_arg_string(msg, 1, path.substring(0, pos));
  msg_set_arg_string(msg, 2, path.substring(pos+1));
  return msg;
  }

function control_get_type(dict)
  {
  let str = dict_get_string(dict, GAVL_CONTROL_TYPE);
  if(str)
    return str;
  str = dict_get_string(dict, GAVL_META_CLASS);
  switch(str)
    {
    case GAVL_META_CLASS_CONTROL_PULLDOWN:
      return GAVL_TYPE_STRING;
    case GAVL_META_CLASS_CONTROL_POWERBUTTON:
      return GAVL_TYPE_INT;
    case GAVL_META_CLASS_CONTROL_RGBCOLOR:
      return GAVL_TYPE_COLOR_RGB;
    
    }
  return null;
  }

function control_history_push(arr, length, ts, val)
  {
  let i;
  let start_time;
  let new_element = new Object();
  new_element.ts = ts;
  new_element.val = val;

  start_time = ts - length;

  arr.push(new_element);
    
  for(i = 0; i < arr.length; i++)
    {
    if(arr[i].ts > start_time)
      {
      if(i >= 2)
        arr.splice(0, i-1);
      break;
      }
    }
    
  }

/* Controls */

function create_button(ret)
  {
  ret.button = append_dom_element(ret.parent, "button");
  ret.button.setAttribute("class", "clickable");

  append_dom_text(ret.button, dict_get_string(ret.dict, GAVL_META_LABEL));

  ret.button.el = ret;

  // console.log("create_button " + dict_get_string(ret.dict, GAVL_META_LABEL));
  
  ret.button.onclick = function(evt)
    {
    let msg = msg_create(GAVL_CMD_CONTROL_PUSH_BUTTON, GAVL_MSG_NS_CONTROL);
    dict_set_string(msg.header, GAVL_MSG_CONTEXT_ID, this.el.path);
    this.el.cb.handle_msg(msg);
    }
    
  }


function create_slider(ret)
  {
  let str;

  ret.label = append_dom_element(ret.parent, "div");


    
  ret.input = append_dom_element(ret.parent, "input");
  ret.input.type = "range";
  ret.input.min = ret.dict[GAVL_CONTROL_MIN].v;
  ret.input.max = ret.dict[GAVL_CONTROL_MAX].v;
  ret.input.el = ret;
  ret.input.setAttribute("id", ret.path);

  ret.set_label = function()
    {
    let str;
    let val;
    str = dict_get_string(ret.dict, GAVL_META_LABEL) + ": " + this.input.value;
    
    if((val = dict_get_string(ret.dict, GAVL_CONTROL_UNIT)))
      str += val;

    clear_element(this.label);
    append_dom_text(this.label, str);
    }
    
  ret.input.oninput = function()
    {
    let msg = create_set_state_msg(this.el.path, BG_CMD_SET_STATE);
    msg_set_arg(msg, 3, control_get_type(this.el.dict), this.value);
    this.el.cb.handle_msg(msg);

    this.el.set_label();
    }

    ret.set_value = function(val, ts)
    {
    this.input.value = val;
    this.set_label();
    }
    
  }

function create_powerbutton(ret)
  {
  let table = append_dom_element(ret.parent, "table");
  table.style = "width: 100%;"
  let tr = append_dom_element(table, "tr");
  ret.label = append_dom_element(tr, "td");

  let td = append_dom_element(tr, "td");
  td.style = "text-align: right;"  
  ret.button = append_dom_element(td, "button");
  ret.button.setAttribute("class", "icon-power clickable");
  ret.button.el = ret;
  ret.button.setAttribute("id", ret.path);

  if(ret.dict[GAVL_CONTROL_VALUE] && ret.dict[GAVL_CONTROL_VALUE].v)
    ret.parent.dataset.value = ret.dict[GAVL_CONTROL_VALUE].v;
    
  ret.button.onclick = function(evt)
    {
    let msg = create_set_state_msg(this.el.path, BG_CMD_SET_STATE_REL);
    msg_set_arg(msg, 3, control_get_type(this.el.dict), 1);
    this.el.cb.handle_msg(msg);
      // let msg = msg_create(GAVL_CMD_CONTROL_PUSH_BUTTON, GAVL_MSG_NS_CONTROL);
      // this.el.cb.handle_msg(msg);
    }

  ret.update = function(dict)
    {
    let label = dict_get_string(dict, GAVL_META_LABEL);
    if(label)
      {
      clear_element(this.label);
      append_dom_text(this.label, label);
      }
    }
  
  ret.set_value = function(val, ts)
    {
    this.parent.dataset.value = val;
    }

  }

function create_meter(ret)
  {
  ret.label = append_dom_element(ret.parent, "div");

 
  ret.meter = append_dom_element(ret.parent, "meter");
  ret.meter.min  = ret.dict[GAVL_CONTROL_MIN].v;
  ret.meter.max  = ret.dict[GAVL_CONTROL_MAX].v;
  ret.meter.setAttribute("id", ret.path);
  ret.meter.el = ret;

  if(ret.dict[GAVL_CONTROL_DIGITS])
    ret.digits  = ret.dict[GAVL_CONTROL_DIGITS].v;
  else
    ret.digits = 0;

  if(ret.dict[GAVL_CONTROL_LOW])
    ret.meter.low  = ret.dict[GAVL_CONTROL_LOW].v;
  if(ret.dict[GAVL_CONTROL_HIGH])
    ret.meter.high = ret.dict[GAVL_CONTROL_HIGH].v;
  if(ret.dict[GAVL_CONTROL_OPTIMUM])
    ret.meter.optimum = ret.dict[GAVL_CONTROL_OPTIMUM].v;
    
  ret.set_label = function()
    {
    let str;
    let val;
    str = dict_get_string(this.dict, GAVL_META_LABEL) + ": ";

    if(this.digits > -1)
      str += this.meter.value.toFixed(this.digits);
    else
      str += this.meter.value;
    
    if((val = dict_get_string(ret.dict, GAVL_CONTROL_UNIT)))
      str += val;

    clear_element(this.label);
    append_dom_text(this.label, str);
    }
    
  ret.set_value = function(val, ts)
    {
    this.meter.value = val;
    this.set_label();
    }
  }

function create_curve(ret)
  {
  let i;
  let history;
  ret.label = append_dom_element(ret.parent, "div");
 
  ret.canvas = append_dom_element(ret.parent, "canvas");
  ret.canvas.el = ret;
  ret.canvas.setAttribute("id", ret.path);
      
  if(ret.dict[GAVL_CONTROL_DIGITS])
    ret.digits  = ret.dict[GAVL_CONTROL_DIGITS].v;
  else
    ret.digits= -1;

  if(ret.dict[GAVL_CONTROL_MIN] &&
     ret.dict[GAVL_CONTROL_MAX])
    {
    ret.min = ret.dict[GAVL_CONTROL_MIN].v;
    ret.max = ret.dict[GAVL_CONTROL_MAX].v;
    ret.autoscale = false;
    }
  else
    ret.autoscale = true;
    
  ret.history = new Array();

  /* Import history read so far */
  history = ret.dict[GAVL_CONTROL_HISTORY].v;
  for(i = 0; i < history.length; i++)
    {
    ret.history[i] = new Object();
    ret.history[i].ts  = history[i].v[GAVL_CONTROL_TIMESTAMP].v;
    ret.history[i].val = history[i].v[GAVL_CONTROL_VALUE].v;
    }

  ret.length = ret.dict[GAVL_CONTROL_HISTORY_LENGTH].v;
    
//  console.log("Got History, length: " + ret.length + " " + JSON.stringify(ret.history));
    
  ret.set_label = function(val)
    {
    let str;
    str = dict_get_string(this.dict, GAVL_META_LABEL) + ": ";

    if(this.digits > -1)
      str += val.toFixed(this.digits);
    else
      str += val;
    
    if((val = dict_get_string(ret.dict, GAVL_CONTROL_UNIT)))
      str += val;

    clear_element(this.label);
    append_dom_text(this.label, str);
    }

  ret.ts_to_x = function(ts)
    {
    let ts_norm = (ts - (this.history[this.history.length-1].ts - this.length))/this.length;
    return this.left_margin + ts_norm * (this.canvas.width - this.left_margin - this.right_margin);
    }

  ret.val_to_y = function(val)
    {
    let val_norm = (val - this.val_tic_min) / (this.val_tic_max - this.val_tic_min);
    return this.top_margin + (this.canvas.height - this.top_margin - this.bottom_margin) * (1.0 - val_norm);
    }

  ret.test_value_tics = function(delta)
    {
//    console.log("test_value_tics " + delta);

    this.val_tic_min = Math.floor(this.min / delta);
    this.val_tic_max = Math.ceil(this.max / delta);

    this.num_value_tics = this.val_tic_max - this.val_tic_min + 1;
    
    this.val_tic_min *= delta;
    this.val_tic_max *= delta;
      
    if(this.num_value_tics <= 7) // Maximum number of labels
      return true;
    else
      return false;
    }

  ret.make_value_tics = function(ctx)
    {
    /* This requires this.min and this.max to be set */
    let i;
    let delta;
    let delta_base;

    let span = this.max - this.min;

    delta = Math.pow(10.0, Math.floor(Math.log10(span * 0.3))) / 10;
    
    for(i = 0; i < 3; i++)
      {
      if(this.test_value_tics(delta) ||
	 this.test_value_tics(delta*2.0) ||
	 this.test_value_tics(delta*5.0))
        break;
      delta *= 10.0;
      }

    this.left_margin = ctx.measureText(this.val_tic_min.toFixed(this.digits)).width;
      
    for(i = 1; i < this.num_value_tics; i++)
      {
      let val = this.val_tic_min + i*((this.val_tic_max - this.val_tic_min)/(this.num_value_tics-1));
      let str = val.toFixed(this.digits);
      let w = ctx.measureText(str).width;
      if(w > this.left_margin)
	this.left_margin = w;
      }
    this.left_margin += 3;
    }
   
  ret.draw = function()
    {
    const font_height = 24;
    let i;

    let ctx = this.canvas.getContext("2d");
    this.canvas.width = this.canvas.clientWidth;
    this.canvas.height = this.canvas.clientHeight; 
      
    ctx.font = font_height + "px Arial";
//    console.log("Font: " + ctx.font);
      
    this.top_margin = font_height + 5;
    this.bottom_margin = font_height + 5;
//    this.left_margin = 2;
    this.right_margin = 2;
      

    /* Clear area */
    ctx.beginPath();
    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height); 
      
    if(this.autoscale)
      {
      ret.min = this.history[0].val;
      ret.max = this.history[0].val;
      for(i = 1; i < this.history.length; i++)
	{
        if(ret.min > this.history[i].val)
          ret.min = this.history[i].val;
        if(ret.max < this.history[i].val)
          ret.max = this.history[i].val;
	}
      if(Math.abs(ret.max - ret.min) < 1.0e-6)
        {
        if(ret.max < 1.0)
	  {
          ret.min = 0.0;
          ret.max = 1.0;
	  }
        else
	  {
          ret.min = ret.max * 0.9;
          ret.max *= 1.1;
	  }
	}
      }

      
    let gradient = ctx.createLinearGradient(0, 0, 0, this.canvas.height);
    gradient.addColorStop(1.00, "#00ff00");
    gradient.addColorStop(0.5,  "#ffff00");
    gradient.addColorStop(0.0,  "#ff0000");
    ctx.fillStyle = gradient;
      
    for(i = 1; i < this.history.length; i++)
      {
      if(ret.min > this.history[i].val)
        ret.min = this.history[i].val;
      if(ret.max < this.history[i].val)
        ret.max = this.history[i].val;
      }
    this.make_value_tics(ctx);

    ctx.save();

    ctx.beginPath();
    ctx.moveTo(this.left_margin, this.canvas.width - this.bottom_margin);
    ctx.lineTo(this.canvas.width - this.right_margin, this.canvas.width - this.bottom_margin);
    ctx.lineTo(this.canvas.width - this.right_margin, this.top_margin);
    ctx.lineTo(this.left_margin, this.top_margin);
    ctx.lineTo(this.left_margin, this.canvas.width - this.bottom_margin);
    ctx.clip();
      
    ctx.beginPath();
    ctx.moveTo(this.ts_to_x(this.history[0].ts), this.canvas.height - this.bottom_margin);

    for(i = 0; i < this.history.length; i++)
      {
      ctx.lineTo(this.ts_to_x(this.history[i].ts), this.val_to_y(this.history[i].val));
      }
      
    ctx.lineTo(this.canvas.width, this.canvas.height - this.bottom_margin);

    ctx.fill(); 

    ctx.restore();

      
      //    ctx.fill(); 


//    ctx.restore();
//    console.log("Drawing val grid: " + this.val_tic_min +" " + this.val_tic_max  +" " + this.num_value_tics);

    ctx.fillStyle = "#000000";

      /* Draw grid lines */
    for(i = 0; i < this.num_value_tics; i++)
      {
      let str;
      let val = this.val_tic_min + i*((this.val_tic_max - this.val_tic_min)/(this.num_value_tics-1));
      let y = this.val_to_y(val);
      ctx.beginPath();
      ctx.moveTo(this.left_margin, y);
      ctx.lineTo(this.canvas.width - this.right_margin, y);
      ctx.stroke();
      str = val.toFixed(this.digits);
      ctx.fillText(str, this.left_margin - ctx.measureText(str).width - 3, y + font_height / 2);
      }
    }
    
  /* Set value (as array) */
  ret.set_value = function(val, ts)
    {
    if(ts == 0)
      return;
    console.log("curve_set_value: " + val + " " + ts + " " + this.canvas.width + " " +
                this.canvas.height + " " + this.history.length);
    //    this.value = val;
    this.set_label(val);

    control_history_push(this.history, this.length, ts, val);
    this.draw();
    }

  /* Draw initial curve */
  if(ret.history.length > 0)
    ret.set_label(ret.history[ret.history.length-1].val);
  ret.draw();
    
  }

function create_volume(ret)
  {
  let table = append_dom_element(ret.parent, "table");


  table.style = "width: 100%;"  

  let tr = append_dom_element(table, "tr");
  ret.label = append_dom_element(tr, "td");

  let td = append_dom_element(tr, "td");
  td.style = "width: 0.1%; white-space: nowrap;"  
  let button = append_dom_element(td, "button");
  button.setAttribute("class", "icon-chevron-up clickable");
  button.el = ret;

  button.onclick = function()
    {
    /* Volume up */
    let msg = create_set_state_msg(this.el.path, BG_CMD_SET_STATE);
    msg_set_arg(msg, 3, GAVL_TYPE_INT, 1);
    this.el.cb.handle_msg(msg);
    }
    
  tr = append_dom_element(table, "tr");
  td = append_dom_element(tr, "td");
  ret.meter = append_dom_element(td, "meter");

  ret.meter.min  = ret.dict[GAVL_CONTROL_MIN].v;
  ret.meter.max  = ret.dict[GAVL_CONTROL_MAX].v;
  ret.meter.setAttribute("id", ret.path);
  ret.meter.el = ret;
  
  td = append_dom_element(tr, "td");
  td.style = "width: 0.1%; white-space: nowrap;"  
  button = append_dom_element(td, "button");
  button.setAttribute("class", "icon-chevron-down clickable");
  button.el = ret;

  button.onclick = function()
    {
    /* Volume down */
    let msg = create_set_state_msg(this.el.path, BG_CMD_SET_STATE);
    msg_set_arg(msg, 3, GAVL_TYPE_INT, -1);
    this.el.cb.handle_msg(msg);
    }
    
  ret.update = function(dict)
    {
    let label = dict_get_string(dict, GAVL_META_LABEL);
    if(label)
      {
      clear_element(this.label);
      append_dom_text(this.label, label);
      }
    }

  ret.set_value = function(val, ts)
    {
    this.meter.value = val;
    }
  }

function create_pulldown(ret)
  {
  let td;
  let button;
  let table = append_dom_element(ret.parent, "table");
  table.style = "width: 100%;"  
  table.el = ret;
  table.setAttribute("id", ret.path);

  console.log("Create pulldown");
  let tr = append_dom_element(table, "tr");

  ret.label = append_dom_element(tr, "td");
  ret.label.style = "width: 30%;"  

  ret.menu = append_dom_element(document.body, "div");
  ret.menu.setAttribute("class", "pulldown-menu");
  ret.menu.dataset.active = false;
    
  td = append_dom_element(tr, "td");
  td.style = "width: 69%;"  

    
  table = append_dom_element(td, "table");
  ret.button = table;
  ret.button.setAttribute("class", "clickable");
    
  table.style = "width: 100%;"  
  table.el = ret;
      
  tr = append_dom_element(table, "tr");
  ret.value_label = append_dom_element(tr, "td");

  td = append_dom_element(tr, "td");
  td.style = "text-align: right;"
  td.setAttribute("class", "icon-chevron-down");

  ret.show_menu = function()
    {
    let parent_rect;
    let menu_rect;
    this.menu.dataset.active = "true";
    console.log("Show menu");
    /* Rectangle relative to the *viewport* */
    parent_rect = this.button.getBoundingClientRect();
    this.menu.style.top = "";
    this.menu.style.bottom = "";

    console.log("parent_rect: " + JSON.stringify(parent_rect));
      
    /* Vertical pos */
    if(parent_rect.top < window.innerHeight - parent_rect.bottom)
      this.menu.style.top = parseInt(parent_rect.bottom + window.pageYOffset) + "px";
    else
      this.menu.style.bottom = parseInt(window.innerHeight - parent_rect.top - window.pageYOffset) + "px";
      
    /* Horizontal pos */
    if(parent_rect.left < window.innerWidth / 2)
      this.menu.style.left = parseInt(parent_rect.left) + "px";
    else
      this.menu.style.right = parseInt(window.innerWidth - parent_rect.right) + "px";

    menu_rect = this.menu.getBoundingClientRect();

    if(menu_rect.bottom > window.innerHeight)
      this.menu.style.top = "0px";

    if(menu_rect.bottom > window.innerHeight)
      this.menu.style.bottom = "0px";
      
    }

  ret.hide_menu = function()
    {
    this.menu.dataset.active = "false";
    console.log("hide menu");
    
    }

  ret.toggle_menu = function()
    {
    if(this.menu.dataset.active === "true")
      this.hide_menu();
    else
      this.show_menu();
    }
    
  table.onclick = function()
    {
    this.el.toggle_menu();
    }

  ret.fire = function(str)
    {
    console.log("Menu fire: %s", str);
    this.hide_menu();

    let msg = create_set_state_msg(this.path, BG_CMD_SET_STATE);
    msg_set_arg_string(msg, 3, str);
    this.cb.handle_msg(msg);
    }
    
  ret.update = function(dict)
    {
    let i;
    let label = dict_get_string(dict, GAVL_META_LABEL);
    if(label)
      {
      clear_element(this.label);
      append_dom_text(this.label, label);
      }
    /* Options */
    let options = dict_get_array(dict, GAVL_CONTROL_OPTIONS);
    if(options)
      {
      clear_element(this.menu);
      for(i = 0; i < options.length; i++)
	{
        let div;
        let label = dict_get_string(options[i].v, GAVL_META_LABEL);

        if(label)
          {
          div = append_dom_element(this.menu, "div");
          div.setAttribute("class", "menu-item");
          append_dom_text(div, label);
          div.dataset.id = dict_get_string(options[i].v, GAVL_META_ID);

          div.menu = this;
   
          div.onclick = function()
	    {
//            console.log("Hit menu item: %s", this.dataset.id);
            this.menu.fire(this.dataset.id);
	    }

	    
	  }
        
 	  
	}
      }

      
      
    }
  
  ret.set_value = function(val, ts)
    {
    let i;
    let found = false;
    let options = dict_get_array(this.dict, GAVL_CONTROL_OPTIONS);
    clear_element(this.value_label);
    console.log("pulldown set_value: " + val);
    for(i = 0; i < options.length; i++)
      {
      if(dict_get_string(options[i].v, GAVL_META_ID) == val)
        {
        append_dom_text(this.value_label, dict_get_string(options[i].v, GAVL_META_LABEL));
        this.menu.children[i].dataset.active = "true";
        found = true;  
	}
      else
	{
        this.menu.children[i].dataset.active = "false";
	}
      }
    if(!found)
      console.log("Not found " + JSON.stringify(options));
      
    }

  }


function create_container(ret)
  {
  let table = append_dom_element(ret.parent, "table");
  table.style = "width: 100%;"  
  table.el = ret;
  table.ondblclick = function(evt)
    {
    window.location.pathname = "/ctrl" + path_encode(this.el.path);
    }


  let tr = append_dom_element(table, "tr");
  let td;

  td = append_dom_element(tr, "td");
  td.setAttribute("class", "icon-folder");
    
  td = append_dom_element(tr, "td");
  append_dom_text(td, dict_get_string(ret.dict, GAVL_META_LABEL));

  td = append_dom_element(tr, "td");
  td.style = "text-align: right;"  
  ret.button = append_dom_element(td, "button");
  ret.button.setAttribute("class", "icon-chevron-right clickable");
  ret.button.el = ret;
  ret.button.onclick = function(evt)
    {
//    console.log("link: " + "/ctrl" + path_encode(this.el.path));

    window.location.pathname = "/ctrl" + path_encode(this.el.path);
    }
    
  }

function create_link(ret)
  {
  let table = append_dom_element(ret.parent, "table");
  table.style = "width: 100%;"  
  table.el = ret;
  table.ondblclick = function(evt)
    {
    window.location = dict_get_string(this.el.dict, GAVL_META_URI);
    }

    
  let tr = append_dom_element(table, "tr");
  let td;

  td = append_dom_element(tr, "td");
  td.setAttribute("class", "icon-globe");
    
  td = append_dom_element(tr, "td");
  append_dom_text(td, dict_get_string(ret.dict, GAVL_META_LABEL));

  td = append_dom_element(tr, "td");
  td.style = "text-align: right;"  
  ret.button = append_dom_element(td, "button");
  ret.button.el = ret;

  ret.button.onclick = function(evt)
    {
    window.location = this.el.uri;
    }

  ret.update = function(dict)
    {
    this.uri = dict_get_string(dict, GAVL_META_URI);
    }
    
  ret.button.setAttribute("class", "icon-chevron-right clickable");
  ret.uri = dict_get_string(ret.dict, GAVL_META_URI);
    
  }

function create_rgbcolor(ret)
  {
  let table = append_dom_element(ret.parent, "table");
  table.style = "width: 100%;"

  let tr = append_dom_element(table, "tr");
  let td = append_dom_element(tr, "td");
  td.style = "width: 30%;";
  append_dom_text(td, dict_get_string(ret.dict, GAVL_META_LABEL));

  td = append_dom_element(tr, "td");
  td.style = "width: 69%;";  
    
  ret.input = append_dom_element(td, "input");
  ret.input.type = "color";
  ret.input.setAttribute("id", ret.path);
  ret.input.el = ret;

  /* Returns js array */
  ret.hex2rgb = function(hex)
    {
    let ret = new Array();
    ret[0] = parseInt(hex.slice(1, 3), 16)/255.0;
    ret[1] = parseInt(hex.slice(3, 5), 16)/255.0;
    ret[2] = parseInt(hex.slice(5, 7), 16)/255.0;
    console.log("hex2rgb " + JSON.stringify(ret));

    return ret;
    }

  ret.rgb2hex = function(rgb)
    {
    console.log("rgb2hex " + JSON.stringify(rgb));
    let r = Math.round(rgb[0]*255.0);
    let g = Math.round(rgb[1]*255.0);
    let b = Math.round(rgb[2]*255.0);
    return "#" + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1);
    }

  ret.changed = function()
    {
    let msg = create_set_state_msg(this.path, BG_CMD_SET_STATE);
    msg_set_arg(msg, 3, GAVL_TYPE_COLOR_RGB, this.hex2rgb(this.input.value));
    this.cb.handle_msg(msg);
    }
    
  ret.input.onchange = function()
    {
    console.log("Change: " + this.value);
    this.el.changed();
    }

  ret.input.oninput = function()
    {
    console.log("Input: " + this.value);
    this.el.changed();
    }

  ret.set_value = function(val, ts)
    {
    this.input.value = this.rgb2hex(val);
    }

  }

function create_controlgroup(ret)
  {
  let el;
  let children;
  let i; 
  let cg = append_dom_element(ret.parent, "fieldset");
  el = append_dom_element(cg, "legend");
  append_dom_text(el, dict_get_string(ret.dict, GAVL_META_LABEL));


  if(!(children = dict_get_array(ret.dict, GAVL_CONTROL_CHILDREN)))
    return;
  
//  console.log("create_controlgroup " + JSON.stringify(children));
  
  for(i = 0; i < children.length; i++)
    {
//    console.log("Add child " + JSON.stringify(children[i].v));
//    console.trace();
    el = append_dom_element(cg, "div");
    create(el, children[i].v, ret.cb, ret.path);
    }
  }

function create_invisible(ret)
  {
  let el;
  let children;
  let i; 

  if(!(children = dict_get_array(ret.dict, GAVL_CONTROL_CHILDREN)))
    return;
  
//  console.log("create_controlgroup " + JSON.stringify(children));
  
  for(i = 0; i < children.length; i++)
    {
//    console.log("Add child " + JSON.stringify(children[i].v));
//    console.trace();
    el = append_dom_element(ret.parent, "div");
    create(el, children[i].v, ret.cb, ret.path);
    }
  }

/*
 *  parent: parent DOM object
 *  dict:   gavl dictionary describing the control
 *  cb:     an Object with a handle_msg() method
 */

export function create(parent, dict, cb, path)
  {
  let ret = new Object();
  let klass = dict_get_string(dict, GAVL_META_CLASS);

//  console.log("Create control: " + path + " " + JSON.stringify(dict));

  parent.dataset.klass = dict_get_string(dict, GAVL_META_CLASS);
  
  ret.parent = parent;
  ret.dict   = dict;
  ret.cb     = cb;
  if(path != "/")  
    ret.path   = path + "/" + dict_get_string(dict, GAVL_META_ID);
  else
    ret.path   = "/" + dict_get_string(dict, GAVL_META_ID);

  if((klass != GAVL_META_CLASS_CONTROL_GROUP) &&
     (klass != GAVL_META_CLASS_CONTAINER_INVISIBLE))
    ret.parent.setAttribute("class", "widget");
      
  switch(klass)
    {
    case GAVL_META_CLASS_CONTROL_BUTTON:
      create_button(ret);
      break;
    case GAVL_META_CLASS_CONTROL_SLIDER:
      create_slider(ret);
      break;
    case GAVL_META_CLASS_CONTROL_PULLDOWN:
      create_pulldown(ret);
      break;
    case GAVL_META_CLASS_CONTROL_VOLUME:
      create_volume(ret);
      break;
    case GAVL_META_CLASS_CONTROL_POWERBUTTON:
      create_powerbutton(ret);
      break;
    case GAVL_META_CLASS_CONTROL_METER:
      create_meter(ret);
      break;
    case GAVL_META_CLASS_CONTAINER:
      create_container(ret);
      break;
    case GAVL_META_CLASS_CONTROL_LINK:
      create_link(ret);
      break;
    case GAVL_META_CLASS_CONTROL_VOLUME:
      create_volume(ret);
      break;
    case GAVL_META_CLASS_CONTROL_CURVE:
      create_curve(ret);
      break;
    case GAVL_META_CLASS_CONTROL_RGBCOLOR:
      create_rgbcolor(ret);
      break;
    case GAVL_META_CLASS_CONTROL_GROUP:
      create_controlgroup(ret);
      break;
    case GAVL_META_CLASS_CONTAINER_INVISIBLE:
      create_invisible(ret);
      break;
    }

  if(ret.update)
    ret.update(ret.dict);

  if(ret.set_value && ret.dict[GAVL_CONTROL_VALUE])
    ret.set_value(ret.dict[GAVL_CONTROL_VALUE].v, 0);
   
  }
