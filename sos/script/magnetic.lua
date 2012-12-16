local magneticFile = { "image/magnetic.png", filter = true }
local magneticImg = eapi.ChopImage(magneticFile, { 64, 64 })

local function Cyan(dir, startVel)
	return function(pos, vel)
		local projectile = bullet.Cyan(pos, vector.Add(vel, startVel))
		eapi.SetAcc(projectile.body, dir)
	end
end

local function Magenta(dir, startVel)
	return function(pos, vel)
		local bulletVel = vector.Add(vel, startVel)
		local obj = bullet.Magenta(pos, bulletVel)
		eapi.SetAcc(obj.body, vector.Scale(dir, -1))
		util.Delay(obj.body, 0.5, util.StopAcc, obj)
		util.Delay(obj.body, 2.0, util.StopAcc, obj, bulletVel)
	end
end

local function Count()
	return util.Random(2, 5)
end

local function Fireworks(pos, vel, speed, Fn)
	local angle = 0
	while angle < 360 do
		local dir = vector.Rotate({ x = speed, y = 0 }, angle)
		bullet.Line(pos, dir, Fn(dir, vel), Count(), 0.9, 1.1)
		angle = angle + util.Random(10, 60)
	end
end

local function Explode(pos, vel, interval)
	interval = interval or 30
	eapi.RandomSeed(10000 + pos.y)
	Fireworks(pos, vel, 100, Cyan)
	Fireworks(pos, vel, 300, Magenta)	
	for angle = 0, 360, interval do
		local distance = (angle % (2 * interval) == 0) and 80 or 48
		local arm = vector.Rotate({ x = distance, y = 0 }, angle)
		powerup.Star(pos, vector.Add(vel, arm))
	end
end

local function ExplodeBomb(obj, interval)	
	obj.Explode(actor.GetPos(obj), obj.velocity, interval)
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	actor.Delete(obj)
end

local function Simple(pos, vel, delay)
	local init = { health = 1,
		       class = "Mob",		       
		       velocity = vel,
		       sprite = magneticImg,
		       offset = { x = -32, y = -32 },
		       bb = actor.Square(24),
		       z = 1 - 0.001 * pos.y,
		       Shoot = ExplodeBomb,
		       pos = pos, }
	local obj = actor.Create(init)
	util.Delay(obj.body, delay or 2.0, ExplodeBomb, obj, 60)
	return obj
end

local function Bomb(pos, vel, delay)
	local obj = Simple(pos, vel, delay)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 64, 0)
	util.AnimateRotation(obj.tile, 2 + 4 * util.Random())
	obj.Explode = Explode
end

local function Poof(pos, vel, interval)
	local function Line(pos, vel)
		local x = util.Random() - 0.5
		bullet.Line(pos, vel, bullet.CyanSpin, 3, 0.95 + x, 1.05 + x)
	end
	local function Emit(Fn, count)
		local angle = util.Random(150, 180)
		bullet.Arc(pos, { x = 0, y = 150 }, Fn, count, angle)
	end
	Emit(powerup.Star, 7)
	Emit(Line, 10)
end

local sSize = { x = 64, y = 16 }
local sPos =  { x = -32, y = -36 }
local function AddShadow(obj)
	local z = obj.z - 0.1
	local tile = eapi.NewTile(obj.body, sPos, sSize, util.radial, z)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 1.0 })
end

local function Chestnut(pos, delay, tint)
	local obj = Simple(pos, train.velocity, delay)
	eapi.SetColor(obj.tile, util.Gray(tint))
	obj.Explode = Poof
	AddShadow(obj)
end

local function Track(pos, vel, acc, duration)
	local body = eapi.NewBody(gameWorld, pos)
	eapi.SetVel(body, vel)
	eapi.SetAcc(body, acc)
	util.DelayedDestroy(body, duration)
	explode.Tail(body, 0.02, 32, 0.5)
end

local function ExplodeBullets(pos)
	local gravity = { x = 0, y = -800 }
	for i = 1, 50, 1 do
		local angle = util.Random(-60, 60)
		local speed = util.Random(100, 500)
		local vel = vector.Rotate({ x = 0, y = speed }, angle)
		local projectile = bullet.MagentaSpin(pos, vel)
		eapi.SetAcc(projectile.body, gravity)
		
	end
	for i = 1, 200, 1 do
		local vel = { x = 0, y = util.Random(128, 256) }
		vel = vector.Rotate(vel, util.Random(-30, 30))
		local starPos = vector.Offset(pos, util.Random(-64, 64), 0)
		local star = powerup.Star(starPos, vel)
		eapi.SetAcc(star.body, vector.null)
	end
end

local function ExplodeTracks(obj)
	local pos = actor.GetPos(obj)
	local gravity = { x = 0, y = -800 }
	Track(pos, { x = 200, y = 600 }, gravity, 1.2)
	Track(pos, { x = -300, y = 500 }, gravity, 1.0)
	Track(pos, { x = -100, y = 700 }, gravity, 1.4)
	Track(pos, { x = 400, y = 0 }, vector.null, 1.2)
	Track(pos, { x = -400, y = 0 }, vector.null, 1.2)
	util.Delay(staticBody, 1, parallax.Delete, parallax.rails)
	player.KillFlash(0.5, { r = 1, g = 1, b = 1, a = 0.75 })
	eapi.PlaySound(gameWorld, "sound/boom.ogg")
	ExplodeBullets(pos)
	actor.Delete(obj)
end

local function Flaming(pos, vel)
	local init = { health = 1000000,
		       class = "Mob",		       
		       velocity = vel,
		       sprite = magneticImg,
		       offset = { x = -32, y = -32 },
		       bb = actor.Square(24),
		       Shoot = ExplodeTracks,
		       pos = pos,
		       z = 1, }
	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 64, 0)
	util.AnimateRotation(obj.tile, 0.7)
	explode.Tail(obj.body, 0.01, 32, 0.5)
end

magnetic = {
	Bomb = Bomb,
	Flaming = Flaming,
	Chestnut = Chestnut,
}
return magnetic
