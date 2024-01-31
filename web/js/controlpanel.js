
var ws; // Websocket
var controls; // Controls (dictionary)
var num_controls = 0;

function find_control(ctx)
  {
  if(!controls[ctx])
    controls[ctx] = new Object();
  return controls[ctx];
  }

function set_control_int(name, value)
  {
  msg = msg_create(BG_CMD_SET_STATE, BG_MSG_NS_STATE);
  msg_set_arg_int(msg, 0, 1); // Last
  msg_set_arg_string(msg, 1, name);
  msg_set_arg_string(msg, 2, "Value");

  msg_set_arg_int(msg, 3, value);

  msg_send(msg, ws);
  }

function set_control_int_rel(name, value)
  {
  msg = msg_create(BG_CMD_SET_STATE_REL, BG_MSG_NS_STATE);
  msg_set_arg_int(msg, 0, 1); // Last
  msg_set_arg_string(msg, 1, name);
  msg_set_arg_string(msg, 2, "Value");

  msg_set_arg_int(msg, 3, value);

  msg_send(msg, ws);
  }

function set_control_float(name, value)
  {
  msg = msg_create(BG_CMD_SET_STATE, BG_MSG_NS_STATE);
  msg_set_arg_int(msg, 0, 1); // Last
  msg_set_arg_string(msg, 1, name);
  msg_set_arg_string(msg, 2, "Value");

  msg_set_arg_float(msg, 3, value);

  msg_send(msg, ws);
  }

function set_control_string(name, value)
  {
//  console.log("set_control_string " + name + " " + value);
  msg = msg_create(BG_CMD_SET_STATE, BG_MSG_NS_STATE);
  msg_set_arg_int(msg, 0, 1); // Last
  msg_set_arg_string(msg, 1, name);
  msg_set_arg_string(msg, 2, "Value");

  msg_set_arg_string(msg, 3, value);

  msg_send(msg, ws);
  }

function global_init()
  {
  console.log("global init()");
  controls = new Object();      

  ws =  new WebSocket("ws://" +
                      window.location.host + "/ws/" + GAVL_META_MEDIA_CLASS_BACKEND_CONTROLPANEL,
		      ['json']);

  ws.onclose = function(evt)
    {
//    var currentdate = new Date(); 
    console.log("Server websocket closed (code: " + evt.code + " :(");
/*
    alert("Server websocket closed " +
          currentdate.getHours() + ":" 
          + currentdate.getMinutes() + ":" +
          currentdate.getSeconds());
 */

//    setTimeout(server_connection_init, 10);
    };
  
  ws.onopen = function()
    {
    console.log("Server websocket open :)");
    };

  ws.onmessage = function(evt)
    {
    var ctrl;
    var state_ctx;
    var state_var;
    var i;
    var msg = msg_parse(evt.data);
    var old_var;

    var button;			
    var table;
    var tr;
    var td;
    var slider;
    var ctrl;
    var items;
    var label;	
    // console.log("Got message " + msg.ns + " " + msg.id + " " + evt.data);

    switch(msg.ns)
      {
      case BG_MSG_NS_STATE:
        switch(msg.id)
          {
          case BG_MSG_STATE_CHANGED:
	    state_ctx = msg.args[1].v;
	    state_var = msg.args[2].v;
//	    console.log("state changed " + state_ctx + " " + state_var);
            ctrl = find_control(state_ctx);
	    ctrl[state_var] = msg.args[3].v;

            if(msg.args[0].v && !ctrl.div) // Last parameter for this context
              {
              ctrl.div =  append_dom_element(document.body, "div");
              append_dom_element(document.body, "hr");

              switch(ctrl["Type"])
                {
                case "power":
                  {
                  table = append_dom_element(ctrl.div, "table");
                  tr = append_dom_element(table, "tr");
                  td = append_dom_element(tr, "td");

                  table.setAttribute("style", "width: 100%;");

                  append_dom_text(td, ctrl["Label"]);

                  td = append_dom_element(tr, "td");
                  td.setAttribute("style", "width: 2em;");

                  button = append_dom_element(td, "button");
                  button.setAttribute("class", "icon-power");
                  button.setAttribute("type", "button");

                  button.ctrl_id = state_ctx;
		      
                  button.onclick = function(evt)
                    {
                    ctrl = find_control(this.ctrl_id);
                    console.log("clicked");

                    if(ctrl["Value"] == 1)
                      set_control_int(this.ctrl_id, 0);
                    else
                      set_control_int(this.ctrl_id, 1);
                    }
      

                  break;
          	  }
                case "slider":
                  {
                  table = append_dom_element(ctrl.div, "table");
                  tr = append_dom_element(table, "tr");
                  td = append_dom_element(tr, "td");

                  table.setAttribute("style", "width: 100%;");

                  append_dom_text(td, ctrl["Label"]);

                  tr = append_dom_element(table, "tr");
		      
                  td = append_dom_element(tr, "td");
                  td.setAttribute("style", "width: 100%;");

                  slider = append_dom_element(td, "input");
                  slider.setAttribute("type", "range");
                  slider.setAttribute("style", "width: 100%;");

                  if(ctrl["Min"])
                    slider.setAttribute("min", ctrl["Min"]);
                  else
                    slider.setAttribute("min", "0.0");

                  if(ctrl["Max"])
                    slider.setAttribute("max", ctrl["Max"]);
                  else
                    slider.setAttribute("max", "1.0");
			  
                  slider.ctrl_id = state_ctx;
	      
                  slider.oninput = function(evt)
                    {
                    console.log("slider changed "  + this.value);
                    set_control_float(this.ctrl_id, this.value);
                    }

                  ctrl.slider = slider;

                  break;
          	  }
                case "volume":
                  {

                  table = append_dom_element(ctrl.div, "table");
                  tr = append_dom_element(table, "tr");
                  td = append_dom_element(tr, "td");

                  table.setAttribute("style", "width: 100%;");

                  td.setAttribute("colspan", "2");
		      
                  append_dom_text(td, ctrl["Label"]);

// Meter
                  tr = append_dom_element(table, "tr");
                  td = append_dom_element(tr, "td");

                  table.setAttribute("style", "width: 100%;");

                  td.setAttribute("colspan", "2");

                  slider = append_dom_element(td, "meter");
                  slider.setAttribute("style", "width: 100%;");
		    
                  if(ctrl["Min"])
                    slider.setAttribute("min", ctrl["Min"]);
                  else
                    slider.setAttribute("min", "0.0");

                  if(ctrl["Max"])
                    slider.setAttribute("max", ctrl["Max"]);
                  else
                    slider.setAttribute("max", "1.0");

                  slider.ctrl_id = state_ctx;
                  ctrl.meter = slider;
		    
                  tr = append_dom_element(table, "tr");
// Minus button
		      
                  td = append_dom_element(tr, "td");
                  td.setAttribute("style", "width: 50%;");


                  button = append_dom_element(td, "button");

                  append_dom_text(button, "-");

                  button.setAttribute("style", "width: 100%;");
                  button.setAttribute("type", "button");

                  button.ctrl_id = state_ctx;
                  button.onclick = function(evt)
                    {
                    ctrl = find_control(this.ctrl_id);
                    console.log("clicked");
                    set_control_int_rel(this.ctrl_id, -1);
		    }
// Plus button

                  td = append_dom_element(tr, "td");
                  td.setAttribute("style", "width: 50%;");

                  button = append_dom_element(td, "button");

                  append_dom_text(button, "+");

                  button.setAttribute("style", "width: 100%;");
                  button.setAttribute("type", "button");

                  button.ctrl_id = state_ctx;
		      
                  button.onclick = function(evt)
                    {
                    ctrl = find_control(this.ctrl_id);
                    console.log("clicked");
                    set_control_int_rel(this.ctrl_id, 1);
		    }
		   
		  }
                  break;
                case "button":
                  {
                  button = append_dom_element(ctrl.div, "button");
                  button.ctrl_id = state_ctx;
                  button.setAttribute("style", "width: 100%;");
                  append_dom_text(button, ctrl["Label"]);
                  button.onclick = function(evt)
                    {
                    ctrl = find_control(this.ctrl_id);
                    console.log("clicked");
                    set_control_int_rel(this.ctrl_id, 1);
		    }
		  
                  }
		  break;
                case "radiobuttons":
                  {
                  append_dom_text(ctrl.div, ctrl["Label"]);
                  append_dom_element(ctrl.div, "br");
		      
                  items = ctrl["Items"];
                  for(i = 0; i < items.length; i++)
                    {
                    if(i > 0)
                      append_dom_element(ctrl.div, "br");

                    input = append_dom_element(ctrl.div, "input");
                    input.setAttribute("type", "radio");
		    input.setAttribute("name", state_ctx);
                    input.setAttribute("id", state_ctx + dict_get_string(items[i].v, GAVL_META_ID));
                    input.ctrl_id = dict_get_string(items[i].v, GAVL_META_ID);
                    input.state_ctx = state_ctx;
                  		
                    input.onchange = function()
		      {
                      var ctrl = find_control(this.state_ctx);
                      console.log("radio changed " + this.ctrl_id);
                      this.value = input.getAttribute("id");
                      set_control_string(this.state_ctx, this.ctrl_id);
		      }
			
                    label = append_dom_element(ctrl.div, "label");
                    label.setAttribute("for", input.getAttribute("id"));
                    append_dom_text(label, dict_get_string(items[i].v, GAVL_META_LABEL));
//                  ctrl["ID"];
			

		    console.log("Item: " + JSON.stringify(items[i]));
		    }
                  }
		  break;
                case "meter":
                  table = append_dom_element(ctrl.div, "table");
                  tr = append_dom_element(table, "tr");
                  td = append_dom_element(tr, "td");

                  table.setAttribute("style", "width: 100%;");

                  append_dom_text(td, ctrl["Label"]);

	          ctrl.label = append_dom_element(td, "span");
                  ctrl.label.setAttribute("class", "label");
		    
                  tr = append_dom_element(table, "tr");
		      
                  td = append_dom_element(tr, "td");
                  td.setAttribute("style", "width: 100%;");

                  slider = append_dom_element(td, "meter");
                  slider.setAttribute("style", "width: 100%;");
		    
                  if(ctrl["Min"])
                    slider.setAttribute("min", ctrl["Min"]);
                  else
                    slider.setAttribute("min", "0.0");

                  if(ctrl["Max"])
                    slider.setAttribute("max", ctrl["Max"]);
                  else
                    slider.setAttribute("max", "1.0");

                  if(ctrl["Low"])
                    slider.setAttribute("low", ctrl["Low"]);
                  if(ctrl["High"])
                    slider.setAttribute("high", ctrl["High"]);
                  if(ctrl["Optimum"])
                    slider.setAttribute("optimum", ctrl["Optimum"]);

		    
                  slider.ctrl_id = state_ctx;
                  ctrl.meter = slider;
                  break;
	    
	        }

              num_controls++;
	      }

            /* Update value */
            if(state_var == "Value")
              {
              switch(ctrl["Type"])
                {
                case "power":
                  {
                  console.log("Power " + ctrl["Value"])
                  ctrl.div.dataset.power = ctrl["Value"];
//                  if(ctrl.div.dataset.power == "1")
//		    console.trace();
		  }
                  break;
                case "slider":
                  {
                  console.log("Slider " + ctrl["Value"])
                  ctrl.slider.value = ctrl["Value"];
		  }
                  break;
                case "radiobuttons":
                  {
                  input = document.getElementById(state_ctx + ctrl["Value"]);
                  if(input)
                    input.checked = true;
		  
                  console.log("Radiobuttons " + state_ctx + " " + ctrl["Value"] + " " + input)
		  
		      //                  ctrl.slider.value = ctrl["Value"];
		  }
                  break;
                case "meter":
                  {
                  console.log("Update meter " + ctrl["Value"])
                  ctrl.meter.value = ctrl["Value"];
                  if(ctrl["Unit"])
                    ctrl.label.dataset.label = ": " + ctrl["Value"] + ctrl["Unit"];
                  else
                    ctrl.label.dataset.label = ": " + ctrl["Value"];
		  }
                  break;
                case "volume":
                  {
                  console.log("Update volume " + ctrl["Value"])
                  ctrl.meter.value = ctrl["Value"];
		  }
                  break;
		}
	      }
            break;
	  }
        break;
      }
    }
  }
