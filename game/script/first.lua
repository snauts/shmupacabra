dofile("config.lua")
dofile("script/util.lua")
dofile("script/vector.lua")

local linux = false

setupFile  = "/shmupacabra/setup.lua"
saveGame   = "/shmupacabra/saavgaam"

if util.FileExists(setupFile) then
	dofile(setupFile)
end

camera     = nil
gameWorld  = nil
staticBody = nil
state	   = { }

util.Preload()
util.Goto("title")
