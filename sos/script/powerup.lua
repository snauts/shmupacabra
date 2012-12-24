local starName = { "image/star.png", filter = true }
local starImg = eapi.ChopImage(starName, { 64, 64 })

local size = { x = 16, y = 16 }
local offset = { x = -8, y = -8 }
local function SmallStar(parent, vel)
	local body = actor.CloneBody(parent)
	eapi.SetAcc(body, vector.null)
	eapi.SetVel(body, vector.Rnd(vel, 64))
	local tile = eapi.NewTile(body, offset, size, starImg, 1)	
	eapi.Animate(tile, eapi.ANIM_LOOP, 48, 0)
	eapi.SetAcc(body, { x = 0, y = -350 })
	util.DelayedDestroy(body, 0.5)
	util.RandomRotateTile(tile)
	return tile
end

local function SmallBurst(parent, count)
	explode.Circular(SmallStar, parent, count)
end

local function Follow(star)
	if star.player.dead then return end
	local playerPos = actor.GetPos(star.player)
	local vel = vector.Sub(playerPos, actor.GetPos(star))
	eapi.SetVel(star.body, vector.Normalize(vel, star.speed))
	star.speed = star.speed + 0.1 * player.speed
	eapi.AddTimer(star.body, 0.1, star.Follow)
end

local function GetStar(pShape, sShape)
	local star = actor.store[sShape]
	if star.pulled then
		util.PlaySound(gameWorld, "sound/star.ogg", 0.01, 0, 0.1)
		SmallBurst(star.body, 3)
		powerup.IncStars()
		actor.Delete(star)
	else
		star.player = actor.store[pShape]
		star.Follow = function() Follow(star) end		
		star.speed = player.speed
		actor.DeleteShape(star)
		star.bb = actor.Square(8)
		actor.MakeShape(star)		
		star.pulled = true
		star.Follow()
	end	
end

actor.SimpleCollide("Player", "Star", GetStar)

local function StarHitsStreet(streetShape, starShape)
	local star = actor.store[starShape]
	local vel = eapi.GetVel(star.body)
	vel.y = math.abs(vel.y)
	eapi.SetVel(star.body, vel)
	eapi.SetAcc(star.body, vector.null)
end

actor.SimpleCollide("Street", "Star", StarHitsStreet)

local function RunAway(star)
	return function()
		local pos = actor.GetPos(star)
		if actor.OffScreen(pos) then
			actor.Delete(star)
		else
			eapi.SetAcc(star.body, { x = -400, y = 0 })
		end
       end
end

local function Star(pos, velocity, z)
	local init = { sprite = starImg,
		       class = "Star",
		       offset = { x = -16, y = -16 },
		       spriteSize = { x = 32, y = 32 },
		       bb = actor.Square(64),
		       velocity = velocity,
		       pos = pos,
		       z = z or -1, }
	local star = actor.Create(init)
	eapi.Animate(star.tile, eapi.ANIM_LOOP, 48, util.Random())	
	eapi.SetAcc(star.body, vector.Scale(velocity, -1))
	star.timer = eapi.AddTimer(star.body, 0.5, RunAway(star))
	util.RandomRotateTile(star.tile)
	return star
end

local function StarFromMob(mob)
	local mobVel = eapi.GetVel(mob.body)
	return function(pos, vel)
		vel = vector.Add(vel, mobVel)
		powerup.Star(pos, vel)
	end
end

local textTiles = { }
local livesTiles = { }
local chargeTiles = { }
local textPos = { x = -44, y = 212 }
local livesPos = { x = 64, y = 212 }
local chargePos = { x = -88, y = 212 }

local function PrintText(pos, text)
	return util.PrintOrange(pos, text, 100, nil, 0.0)
end

local star = string.char(143)
local function StarText(count)
	return star .. "  " .. util.PadZeros(count, 5) .. "  " .. star
end

local heart = string.char(155)
local function LivesText(count)
	return (count > 9) and "@_@" or (heart .. " " .. count)
end

local function ToAlpha(alpha)
	return function(tile) util.SetTileAlpha(tile, alpha) end
end

local function ShowCharge(pos, Print, amount)
	local tiles = { }
	pos = vector.Offset(pos, -4, 0)
	local stripes = math.ceil(amount)
	local alpha = 1.0 - (stripes - amount)
	local SetAlpha = ToAlpha(alpha)

	for i = 1, stripes, 1 do
		local newTiles =  Print(pos, "|", 100, nil, 0.0)
		if i == stripes then util.Map(SetAlpha, newTiles) end
		tiles = util.JoinTables(tiles, newTiles)
		pos = vector.Offset(pos, 4, 0)
	end
	return tiles
end

local function UpdateCharge()
	util.Map(eapi.Destroy, chargeTiles)
	local available = state.charge >= player.bombCost
	local amount = 7 * state.charge / player.fullCharge
	local Print = available and util.PrintOrange or util.PrintRed
	chargeTiles = ShowCharge(chargePos, Print, amount)
end

local function UpdateStars()
	util.Map(eapi.Destroy, textTiles)
	textTiles = PrintText(textPos, StarText(state.stars))
end

local function UpdateLives()
	util.Map(eapi.Destroy, livesTiles)
	livesTiles = PrintText(livesPos, LivesText(state.lives))
end

local function DecCharge(amount)
	amount = amount or 1
	local success = state.charge >= amount
	if success then 
		state.charge = state.charge - amount
		UpdateCharge()
	end
	return success
end

local isRecharging = false

local function Recharging()
	if state.charge == player.fullCharge then
		isRecharging = false
	else
		eapi.AddTimer(staticBody, player.bombRecharge, Recharging)
		state.charge = state.charge + 1
		UpdateCharge()
		if state.charge == player.bombCost then
			eapi.PlaySound(gameWorld, "sound/available.ogg")
		end
	end
end

local function Recharge()
	if isRecharging == false then
		isRecharging = true
		Recharging()
	end
end

local function DecLives()
	local alive = (state.lives ~= 0)
	if alive then 
		state.lives = state.lives - 1
		UpdateLives()
		state.charge = player.fullCharge
		UpdateCharge()
	end
	return alive
end

local function IncLives()
	state.lives = state.lives + 1
	eapi.PlaySound(gameWorld, "sound/life.ogg")
	local pos = vector.Offset(livesPos, 20, 10)
	local body = eapi.NewBody(gameWorld, pos)
	SmallBurst(body, 10)
	eapi.Destroy(body)
	UpdateLives()
end

local function LiveAmount(count)
	return math.floor(count / player.extraLife)
end

local function IncStars(amount)
	local old = state.stars
	state.stars = state.stars + (amount or 1)
	if LiveAmount(old) < LiveAmount(state.stars) then IncLives() end
	UpdateStars()
end

local function SetStats(light, alpha)
	local function Box(text, pos)
		local size = { x = 8 * string.len(text) + 12, y = 21 }
		local offset = { x = pos.x - 6, y = pos.y - 2 }
		menu.Box(size, offset, nil, light, alpha)
	end
	Box(StarText(0), textPos)
	Box(LivesText(0), livesPos)
	Box(LivesText(0), chargePos) -- charge should be as wide as lives
	state.lives = state.lastLives or player.startLives
	state.stars = state.lastStars or 0
	state.charge = player.fullCharge
	UpdateCharge()
	UpdateLives()
	UpdateStars()
end

local barTiles = { }
local boxTiles = nil
local barWidth = 180
local size = { x = barWidth + 8, y = 21 }
local pos = { x = -0.5 * barWidth + 1, y = 183 }
local function ShowProgress(obj, light, alpha)
	util.DestroyTable(barTiles)
	if obj.health <= 0 then
		if boxTiles then
			util.DestroyTable(boxTiles)
			barTiles = { }		
			boxTiles = nil
		end
		return
	elseif not boxTiles then
		local offset = vector.Offset(pos, -5, -2)
		boxTiles = menu.Box(size, offset, nil, light, alpha)
	end
	local amount = 0.25 * barWidth * obj.health / obj.maxHealth
	barTiles = ShowCharge(pos, util.PrintRed, amount)
end

local function ArcOfStars(mob, count)
	local vel = vector.Rotate({ x = 100, y = 0 }, 360 * util.Random())
	bullet.FullArc(actor.GetPos(mob), vel, powerup.StarFromMob(mob), count)
end

powerup = {
	Star = Star,
	GetStar = GetStar,
	Recharge = Recharge,
	SmallStar = SmallStar,
	ArcOfStars = ArcOfStars,
	StarFromMob = StarFromMob,
	IncLives = IncLives,
	DecLives = DecLives,
	DecCharge = DecCharge,
	IncStars = IncStars,
	SetStats = SetStats,
	ShowProgress = ShowProgress,
}
return powerup
