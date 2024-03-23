
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
    
    }
  return null;
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

  ret.parent.setAttribute("class", "widget");

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

  ret.set_value = function(val)
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
  ret.label = append_dom_element(table, "td");

  let td = append_dom_element(table, "td");
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
  
  ret.set_value = function(val)
    {
    this.parent.dataset.value = val;
    }

  }

function create_meter(ret)
  {
  ret.label = append_dom_element(ret.parent, "div");
  ret.parent.setAttribute("class", "widget");
 
  ret.meter = append_dom_element(ret.parent, "meter");
  ret.meter.min  = ret.dict[GAVL_CONTROL_MIN].v;
  ret.meter.max  = ret.dict[GAVL_CONTROL_MAX].v;
  ret.meter.setAttribute("id", ret.path);
  ret.meter.el = ret;

  if(ret.dict[GAVL_CONTROL_DIGITS])
    ret.digits  = ret.dict[GAVL_CONTROL_DIGITS].v;
  else
    ret.digits= -1;

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
    
  ret.set_value = function(val)
    {
    this.meter.value = val;
    this.set_label();
    }
  }

function create_volume(ret)
  {
  let table = append_dom_element(ret.parent, "table");
  ret.parent.setAttribute("class", "widget");

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

  ret.set_value = function(val)
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
      this.menu.style.top = parseInt(parent_rect.bottom) + "px";
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
  
  ret.set_value = function(val)
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
  table.setAttribute("class", "widget");
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
  table.setAttribute("class", "widget");
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

  ret.button.onclick = function(evt)
    {
    window.location = dict_get_string(this.el.dict, GAVL_META_URI);
    }

  ret.button.setAttribute("class", "icon-chevron-right clickable");
  ret.button.el = ret;
    
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
    case GAVL_META_CLASS_CONTROL_GROUP:
      create_controlgroup(ret);
      break;
    case GAVL_META_CLASS_CONTAINER_INVISIBLE:
      create_invisible(ret);
      break;
    }

  if(ret.update)
    ret.update(ret.dict);

  if(ret.set_value && ret.dict[GAVL_CONTROL_VALUE] && ret.dict[GAVL_CONTROL_VALUE].v)
    ret.set_value(ret.dict[GAVL_CONTROL_VALUE].v);

    
  }
