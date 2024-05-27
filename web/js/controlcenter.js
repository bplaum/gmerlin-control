
import * as control from "/static2/js/control.js";

var server_conn = null;

/* Application state and history */
var my_history = new Array();

function adjust_header_footer()
  {
  let header_height = document.getElementsByTagName("header")[0].getBoundingClientRect().height;
  let el = document.getElementById("browser");
  el.style.paddingTop = parseInt(header_height) + "px";
  }

function update_header()
  {
  let el = document.getElementById("header-label");
  clear_element(el);
  append_dom_text(el, "");
  adjust_header_footer();
  }

/* Browser */

function create_browser()
  {
  let browser = new Object();
    
  browser.div = document.getElementById("browser");
    
  browser.add_child = function(el)
    {
    let child = append_dom_element(browser.div, "div");
    control.create(child, el, this, browser.path);
    }
    
  browser.set_children = function(arr)
    {
    let i;
    clear_element(this.div);

    for(i = 0; i < arr.length; i++)
      {
      this.add_child(arr[i].v);
      }
    }

  browser.handle_msg = function(msg)
    {
    if(server_conn)
      msg_send(msg, server_conn);
    };
    
  return browser;
  }

/* Server connection */

function create_connection()
  {
  let ws =  new WebSocket("ws://" +
                      window.location.host + "/ws/" + GAVL_META_CLASS_BACKEND_CONTROLPANEL,
		      ['json']);

  ws.onclose = function(evt)
    {
    console.log("Server websocket closed (code: " + evt.code + " :(");
    let el = document.getElementById("header-icon");
    el.setAttribute("class", "icon-x");

    el = document.getElementById("header-status");
    clear_element(el);
    append_dom_text(el, "Disconnected");
    adjust_header_footer();
    };
  
  ws.onopen = function()
    {
    let msg;
    let path = control.path_decode(window.location.pathname.substring(5));
    console.log("Server websocket open, now browsing to " + path);

    let el = document.getElementById("header-icon");
    el.setAttribute("class", "icon-network");

    el = document.getElementById("header-status");
    clear_element(el);
    append_dom_text(el, "Connected to " + window.location.host);
      
    msg = msg_create(GAVL_FUNC_CONTROL_BROWSE, GAVL_MSG_NS_CONTROL);
    dict_set_string(msg.header, GAVL_MSG_CONTEXT_ID, path);
    msg_send(msg, this);

    el = document.getElementById("header-path");
    clear_element(el);
    append_dom_text(el, path);
    adjust_header_footer();
    };

  ws.onmessage = function(evt)
    {
    var msg = msg_parse(evt.data);

//    console.log("Onmessage " + JSON.stringify(msg));
      
    switch(msg.ns)
      {
      case BG_MSG_NS_STATE:
        switch(msg.id)
          {
          case BG_MSG_STATE_CHANGED:
            {
            let ctrl;
            let ctx = msg.args[1].v;
//            console.log("State changed: " + JSON.stringify(msg.header));

              if((ctrl = document.getElementById(msg.args[1].v + "/" + msg.args[2].v)) &&
		 (ctrl = ctrl.el))
		ctrl.set_value(msg.args[3].v, msg.header[GAVL_CONTROL_TIMESTAMP].v);
//	      else
//		console.log("Cannot find control");
		
            if(window.browser.path != msg.args[1].v)
	      return;

//            var = msg.args[2].v;
	    }
	    break;
	  }
	break;
      case GAVL_MSG_NS_CONTROL:
        switch(msg.id)
          {
          case GAVL_RESP_CONTROL_BROWSE:
//	    console.log("Got browse response " + JSON.stringify(msg.args[0].v));
	    window.browser.path = dict_get_string(msg.header, GAVL_MSG_CONTEXT_ID);
            window.browser.set_children(msg.args[0].v);
	    break;
	  }
	break;
      }
    } 
  server_conn = ws;
  }

window.global_init = function()
  {
  console.log("global_init " + window.location.path + " " + window.location.protocol);
  if(!window.location.pathname || (window.location.pathname == "/"))
    window.location.replace(window.location.protocol + "//" + window.location.host + "/ctrl/");
  /* Create server connection */
  create_connection();

  window.browser = create_browser();
   
  }

