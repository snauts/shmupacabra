dofile("script/actor.lua")
dofile("script/explode.lua")
dofile("script/powerup.lua")

local function Quit()
	eapi.PlaySound(gameWorld, "sound/inv-plop.ogg")
	util.Goto("quit")
end

menu.TitleMusic()
util.BigTextCenter(txt.gameName, staticBody, 0.15)

local maxZ = 10

local function SmallStar(parent, vel)
	vel = vector.Offset(vel, 0, 50)
	local tile = powerup.SmallStar(parent, vel)
	eapi.SetColor(tile, util.Gray(util.Random()))
end

local function EmitStar(arg, x, y, noTimer)
	x = x or util.Random(-432, 432)
	y = y or 256
	local z = util.Random(1, maxZ)
	local star = powerup.Star({ x = x, y = y }, { x = 0, y = -50 }, -z)
	if noTimer == nil then eapi.AddTimer(staticBody, 0.01, EmitStar) end
	eapi.SetAcc(star.body, { x = util.Random(-10, 10), y = -50 })
	eapi.SetColor(star.tile, util.Gray(maxZ))
	local function Animate(Fn, value)
		Fn(star.tile, eapi.ANIM_CLAMP, value, 0.1, 0)
	end
	Animate(eapi.AnimateSize, { x = 16 + 16 / z, y = 16 + 16 / z })
	Animate(eapi.AnimatePos, { x = -8 - 8 / z, y = -8 - 8 / z })
	Animate(eapi.AnimateColor, util.Gray(0.1 * (maxZ - z + 1)))
	eapi.CancelTimer(star.timer)
	actor.DeleteShape(star)
	star.bb = actor.Square(4)
	actor.MakeShape(star)
end
EmitStar()
EmitStar()
EmitStar()

local function SmashStar(_, sShape)
	local star = actor.store[sShape]
	explode.Circular(SmallStar, star.body, 3)
	actor.Delete(star)
end

local width = 0.5 * string.len(txt.gameName) * util.bigFontset.size.x
local height = 0.25 * util.bigFontset.size.y
local bb = { b = -height, t = height, l = -width, r = width }
eapi.NewShape(staticBody, nil, bb, "Smasher")
actor.SimpleCollide("Smasher", "Star", SmashStar)

local motdTiles = nil
local function MOTD()
	eapi.RandomSeed(os.time())
	local motd = util.RandomElement(txt.motd)
	local pos = util.TextCenter(motd, util.defaultFontset)
	motdTiles = util.PrintOrange(vector.Offset(pos, 0, -84), motd)
end
MOTD()

for i = 0, 200, 1 do
	local x = util.Random(-400, 400)
	local y = util.Random(-240, 240)
	if y > 0 or math.abs(x) > width then
		EmitStar(nil, x, y, true)
	end
end

local function Decode(key)
        if type(key) == "number" then
		return eapi.GetKeyName(key)
	else
		return key
	end
end

local function AcquireKeybinding(text, name, Next)
	local acquired = false
	local obj = menu.Blinking(text, { x = 0, y = -84 }, 0.2)
	local function Continue()
		util.MaybeCall(Next)
		obj.Stop()
	end
	local function CaptureKey(key, pressed)
		if pressed and not acquired then
			acquired = true
			eapi.PlaySound(gameWorld, "sound/star.ogg", 0, 0.3)
			Cfg.controls[name] = Decode(key)
			eapi.AddTimer(staticBody, 0.5, Continue)
			eapi.BindKeyboard(util.Noop)			
			obj.SetRate(0.05)
		end
	end
	input.RestoreNormal(CaptureKey)
end

local function DelayAcquire(text, name, Next)
	return function() AcquireKeybinding(text, name, Next) end
end

local actions = { "Up", "Down", "Left", "Right", "Shoot", "Bomb", "Pause" }
local mainMenu = nil

local function Restore()
	input.UnbindAll()
	mainMenu.BindAll()
	input.RestoreNormal()
        local f = io.open(setupFile, "w")
        if f then
		local function Write(var)
			local val = util.Format(Cfg.controls[var])
			f:write("Cfg.controls." .. var .. "=" .. val .. "\n")
		end
		util.Map(Write, actions)
                io.close(f)
        end
	MOTD()
end

local function Setup()
	input.ResetState()
	util.DestroyTable(motdTiles)
	local Pause = DelayAcquire(txt.pressPause, "Pause", Restore)
	local Charge = DelayAcquire(txt.pressCharge, "Bomb", Pause)
	local Shoot = DelayAcquire(txt.pressShoot, "Shoot", Charge)
	local Right = DelayAcquire(txt.pressRight, "Right", Shoot)
	local Left = DelayAcquire(txt.pressLeft, "Left", Right)
	local Down = DelayAcquire(txt.pressDown, "Down", Left)
	AcquireKeybinding(txt.pressUp, "Up", Down)
end

local function Tutorial()
	util.Goto("tutorial")
end

local Main = nil

local function LoadGame()
	dofile(saveGame)
	util.Level(state.levelNum, true)()
end

local function Game()
	menu.Stop()
	menu.Show({ { text = txt.tutorial, 
		      Func = Tutorial },
		    { text = txt.beginning, 
		      Func = util.NewGame },
		    { text = txt.continue,
		      grayOut = not util.FileExists(saveGame),
		      Func = LoadGame }, },
		  false, false, true)

	local function ToMenu()
		eapi.PlaySound(gameWorld, "sound/inv-plop.ogg")
		menu.Stop()
		Main()
	end
	input.Bind("UIPause", false, util.KeyDown(ToMenu))
end

Main = function()
	mainMenu = menu.Show(
		{ { text = txt.start, 
		    Func = Game },
		  { text = txt.setup, 
		    Func = Setup },
		  { text = txt.quit,
		    Func = Quit }, },
		false, false, true)

	input.Bind("UIPause", false, util.KeyDown(Quit))
end

Main()
