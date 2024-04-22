dofile("script/parallax.lua")
dofile("script/bullet.lua")
dofile("script/powerup.lua")
dofile("script/plane.lua")

local obj = player.Simple({ x = -250, y = -100 }, 1)
state.lastLives = player.startLives
state.lastStars = 0
powerup.SetStats()

local Shoot = player.Shoot(obj)
local Charge = player.Bomb(obj)

local controls = { }
for i = 1, 4, 1 do
	controls[i] = player.Move(obj, i)
end

local function Control(...)
	for i, v in ipairs(arg) do
		controls[i](v)
	end
end

menu.Box({ x = 400, y = 32 }, { x = -200, y = 160 }, nil)

local textTiles = nil
local function Text(str)
	return function()
		local pos = util.TextCenter(str, util.defaultFontset)
		pos = vector.Offset(pos, 0, 176)
		textTiles = util.PrintOrange(pos, str, nil, nil, 0)
	end
end		

local circingTimer = nil
local function StartCircling()
	local index = 0
	local movement = {
		{ false, true,  false, false, },
		{ false, true,  true,  false, },
		{ false, false, true,  false, },
		{ true,  false, true,  false, },
		{ true,  false, false, false, },
		{ true,  false, false, true,  },
		{ false, false, false, true,  },
		{ false, true,  false, true, },
	}
	local function Move()
		index = index % #movement
		Control(unpack(movement[index + 1]))
		circingTimer = eapi.AddTimer(staticBody, 0.25, Move)
		index = index + 1
	end
	Move()
end

local function StopCircling()
	Control(false, false, false, false)
	eapi.CancelTimer(circingTimer)
end

local function WaitForKeySkip(Continue)
	local function Skip(keyDown)
		if keyDown then
			input.Bind("Select")
			input.Bind("Shoot")
			input.Bind("Skip")
			if textTiles then
				util.Map(eapi.Destroy, textTiles)
				textTiles = nil
			end
			Continue()
		end
	end
	input.Bind("Select", false, Skip)
	input.Bind("Shoot", false, Skip)
	input.Bind("Skip", false, Skip)
end

local tappingTimer = nil
local function StartTapping()
	local flip = true
	local function Tap()
		tappingTimer = eapi.AddTimer(staticBody, 0.1, Tap)
		flip = not flip
		Shoot(flip)
	end
	Tap()
end

local function StopTapping()
	eapi.CancelTimer(tappingTimer)
end

local function ShootButton(state)
	return function() Shoot(state) end
end

local planeTimer = nil
local function StartPlanes()
	plane.Kamikaze({ x = 432, y = player.Pos().y })	
	planeTimer = eapi.AddTimer(staticBody, 0.5, StartPlanes)
	if state.stars > 99900 then state.stars = 0 end
end

local function StopPlanes()
	eapi.CancelTimer(planeTimer)
end

local bulletTimer = nil
local retreatHeight = -192

local function BulletPattern(obj, pos)
	local target = bullet.HomingVector(player.Pos(), pos)
	local projectile = bullet.Cyan(pos, target)
	projectile.class = "Sense"
	actor.MakeShape(projectile, { l = -96, r = 16, b = -16, t = 16 })
end

local function HellPattern(obj, pos)
	local target = bullet.HomingVector(player.Pos(), pos)
	for i = 1, 20, 1 do
		local vel = vector.Rotate(target, 60 * (util.Random() - 0.5))
		vel = vector.Scale(vel, 0.8 + 0.4 * util.Random())
		bullet.Cyan(pos, vel)
	end
end

local function BulletSense(ps, bs, resolve)
	if not resolve then
		Control(false, false, false, false)
	else
		local obj = actor.store[bs]
		local playerPos = player.Pos()
		local bulletPos = actor.GetPos(obj)
		local delta = vector.Sub(bulletPos, playerPos)
		if not obj.dodge then
			if math.abs(delta.x) > 2 * math.abs(delta.y)  then
				obj.dodge = playerPos.y
			else
				obj.dodge = delta.y		
			end
		end
		if obj.dodge > 0 then
			Control(false, false, false, true)
		else
			Control(false, false, true, false)
		end
	end
end

eapi.Collide(gameWorld, "Player", "Sense", BulletSense, false, 10)

local hell = false
local function StartBullets()
	local Pattern = hell and HellPattern or BulletPattern
	plane.Retreating({ x = 432, y = retreatHeight }, 0.05, Pattern)
	bulletTimer = eapi.AddTimer(staticBody, 0.7, StartBullets)
	retreatHeight = retreatHeight + 64
	if retreatHeight > 192 then
		retreatHeight = -192
	end
end

local function StopBullets()
	eapi.CancelTimer(bulletTimer)
end

local function DelayCharge(state)
	return function()
		Charge(state)
		hell = false
	end
end

local chargeTimer = nil
local function StartCharge()
	hell = true
	chargeTimer = eapi.AddTimer(staticBody, 30, StartCharge)
	eapi.AddTimer(staticBody, 2, DelayCharge(true))
	eapi.AddTimer(staticBody, 3, DelayCharge(false))
end

local function StopCharge()
	eapi.CancelTimer(chargeTimer)
end

local blinkingText = nil
local function StartBlinkArrow()
	blinkingText = menu.Blinking("->", { x = -110, y = 220 }, 0.3)
end

local function StopBlinkArrow()
	blinkingText.Stop()
end

input.Bind("UIPause", false, util.KeyDown(util.NewGame))

local function MasterScript()
	util.DoEventsRelative({ { 0.0, Text(txt.tutorial0) },
				{ 0.0, WaitForKeySkip },

				{ nil, StartCircling },
				{ 0.0, Text(txt.tutorial1) },
				{ 0.0, WaitForKeySkip },

				{ nil, StartTapping },
				{ 0.0, Text(txt.tutorial2) },
				{ 0.0, WaitForKeySkip },

				{ nil, StopTapping }, 
				{ 0.0, ShootButton(true) },
				{ 0.0, Text(txt.tutorial3) },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial4) },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial5) },
				{ 0.0, StartPlanes },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial6) },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial7) },
				{ 0.0, StopPlanes },
				{ 0.0, StopCircling },
				{ 0.0, StartBullets },
				{ 0.0, ShootButton(false) },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial8) },
				{ 0.0, StartCharge },				
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial8b) },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorial9) },
				{ 0.0, StartBlinkArrow },
				{ 0.0, WaitForKeySkip },

				{ nil, Text(txt.tutorialX) },
				{ 0.0, StopBlinkArrow },
				{ 0.0, StopCharge },	
				{ 0.0, StopBullets },
				{ 0.0, WaitForKeySkip },

				{ nil, util.NewGame }})
end

MasterScript()
