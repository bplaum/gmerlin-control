#!/usr/bin/wpexec

local default_audio_sink = nil

default_nodes = nil
mixer = nil

node_om = ObjectManager({
  Interest({ type = "node", Constraint({ "media.class", "=", "Audio/Sink", type = "pw-global" }) }),
})

local add_node = function(node)
  print("!AddNode")


  if(node.properties["node.nick"])
  then
    print("Label: ", node.properties["node.description"] .. " [" .. node.properties["node.nick"] .. "]")
  else 
    print("Label: ", node.properties["node.description"])
  end

  print("ID: ", node.properties["object.id"])
  print("")
  
--  Debug.dump_table(node.properties)
 
end

local delete_node = function(node)
  print("!DelNode")
  print("ID: ", node.properties["object.id"])
  print("")
  
--  Debug.dump_table(node.properties)

end

local send_default_audio_sink = function()
  default_audio_sink = default_nodes:call("get-default-node", "Audio/Sink")
  print("!DefaultAudioSink")
  print("ID: ", default_audio_sink)
  print("")
  
end

local send_default_audio_volume = function()
  local volume = mixer:call("get-volume", default_audio_sink)
  print("!DefaultAudioVolume")
  print("Volume: ", volume.volume)
  print("")
  
end


node_om:connect("installed", function(_) 
--  print("installed " .. node_om:get_n_objects()) 

  for node in node_om:iterate() do
    add_node(node)
  end
  
  node_om:connect("object-added", function(_, node)
    add_node(node)
  end)

  send_default_audio_sink()
  send_default_audio_volume()

end)

Core.require_api("mixer", function(mixer_l)

  mixer = mixer_l

  mixer.scale = 'cubic'
 
  mixer:connect("changed", function(_, id) 
    if(id == default_audio_sink)
    then 
      send_default_audio_volume()
    end
  end)

  Core.require_api("default-nodes", function(default_nodes_l)
--    print("Got default-nodes")
    default_nodes = default_nodes_l

    default_nodes:connect("changed", function(arg) 
      send_default_audio_sink()
      send_default_audio_volume()
    end)

    node_om:activate()
  end)

end)



