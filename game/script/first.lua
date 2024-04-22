dofile("config.lua")
dofile("script/util.lua")
dofile("script/vector.lua")

local linux = false

setupFile  = "setup.lua"
saveGame   = "saavgaam"

if linux then
	local home = os.getenv("HOME")
	local dir = home .. "/.config/shmupacabra/"
	os.execute("mkdir -p " .. dir)
	setupFile = dir .. setupFile
	saveGame = dir .. saveGame
end

if util.FileExists(setupFile) then
	dofile(setupFile)
end

camera     = nil
gameWorld  = nil
staticBody = nil
state	   = { }

util.Preload()
util.Goto("title")
