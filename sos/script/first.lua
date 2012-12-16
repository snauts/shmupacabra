dofile("config.lua")
dofile("script/util.lua")
dofile("script/vector.lua")
if util.FileExists("setup.lua") then
	dofile("setup.lua")
end

camera     = nil
gameWorld  = nil
staticBody = nil
state	   = { }

util.Preload()
util.Goto("title")
