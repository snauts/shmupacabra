local flowerFile = { "image/flower.png", filter = true }
local flowerImg = eapi.ChopImage(flowerFile, { 128, 128 })

local function Explode(mob)
	explode.Area(vector.null, mob.body, 10, 48, 0.04)
	powerup.ArcOfStars(mob, 7)
	actor.DelayedDelete(mob, 0.25)
	actor.DeleteShape(mob)
end

local function Jerk(tile)
	eapi.AnimateFrame(tile, eapi.ANIM_REVERSE_CLAMP, 23, 19, nil, 0.1)	
end

local function EmitBullets(obj)
	local function Bullet(pos, vel)		
		return bullet.Magenta(pos, vector.Add(vel, obj.velocity))
	end
	local pos = actor.GetPos(obj)
	if pos.x < -300 then return end
	local angle = 30 + obj.angle
	if obj.speed > 0 then
		local time = eapi.GetTime(obj.body)
		angle = angle + 360 * time / obj.speed
	end
	local vel = vector.Rotate(bullet.velocity, angle)
	bullet.FullArc(pos, vel, Bullet, 6)	
end

local function Put(pos, vel, angle, speed, delay)
	speed = speed or 0
	angle = angle or 0
	delay = delay or 0
	local init = { health = 15,
		       class = "Mob",		       
		       sprite = flowerImg,
		       offset = { x = -64, y = -64 },
		       bb = { b = -24, t = 24, l = -24, r = 24 },
		       Shoot = Explode,
		       speed = speed,
		       angle = angle,
		       velocity = vel,
		       pos = pos,
		       z = 1, }
	local obj = actor.Create(init)
	local function Emit()
		EmitBullets(obj)
	end
	local function Attack()
		if actor.GetPos(obj).x > -200 then
			Jerk(obj.tile)
			eapi.AddTimer(obj.body, 1.0, Attack)
			util.Repeater(Emit, 5, 0.05, obj.body)
		end
	end
	eapi.AddTimer(obj.body, delay + 1.2, Attack)	
	local function Open()
		eapi.Animate(obj.tile, eapi.ANIM_CLAMP, 24, 0)
	end
	eapi.AddTimer(obj.body, delay, Open)
	if speed > 0 then
		util.AnimateRotation(obj.tile, speed, angle)
	else
		util.RotateTile(obj.tile, angle)
	end
	return obj
end

local arcVel = { x = 250, y = 0 }
local function BurstBullets(obj)	
	local pos = actor.GetPos(obj)
	bullet.FullArc(pos, arcVel, bullet.Magenta, 6)
	for i = 1, 4, 1 do		
		local vel = vector.Scale(arcVel, 1 - i * 0.05)
		bullet.FullArc(pos, vector.Rotate(vel, i), bullet.Magenta, 6)
		bullet.FullArc(pos, vector.Rotate(vel, -i), bullet.Magenta, 6)
	end
	arcVel = vector.Rotate(arcVel, -20)
	Explode(obj)
end

local pulse = 0.2
local frame = { { 1, 33 }, { 30, 30 } }
util.radial = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)

local sSize = { x = 64, y = 16 }
local sPos =  { x = -32, y = -50 }
local saSize = vector.Offset(sSize, 8, 4)
local saPos = vector.Offset(sPos, -4, -2)
local function AddShadow(obj, aOf)
	local z = obj.z - 0.1
	local tile = eapi.NewTile(obj.body, sPos, sSize, util.radial, z)
	eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, saPos, pulse, aOf)
	eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, saSize, pulse, aOf)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0.8 })
	obj.shadow = tile
end

local fatPos = { x = -64 - 8, y = -64 }
local fatSize = { x = 128 + 16, y = 128 + 4 }
local lightPink = { r = 1, g = 0.8, b = 0.8 }
local function Jump(pos, delay)
	local init = { health = 30,
		       class = "Mob",		       
		       sprite = flowerImg,
		       offset = { x = -64, y = -64 },
		       bb = { b = -24, t = 24, l = -24, r = 24 },
		       velocity = train.velocity,
		       Shoot = BurstBullets,
		       pos = pos,
		       z = 1 - 0.001 * pos.y, }
	local obj = actor.Create(init)
	local function Burst()
		eapi.SetVel(obj.body, vector.null)
		eapi.SetAcc(obj.body, vector.null)
		BurstBullets(obj)
	end
	local function Launch()
		local speed = 500
		explode.Simple({ x = 0, y = -16 }, obj.body, obj.z + 0.0001)
		eapi.Animate(obj.tile, eapi.ANIM_CLAMP, 24, 0)
		eapi.SetVel(obj.body, vector.Offset(obj.velocity, 0, speed))
		local inverse = vector.Scale(train.velocity, -1)
		eapi.SetAcc(obj.body, vector.Offset(inverse, 0, -speed))
		eapi.AddTimer(obj.body, 1, Burst)
		eapi.Destroy(obj.shadow)
	end
	local aOf = pulse * util.Random()
	eapi.AddTimer(obj.body, delay or 0.5, Launch)
	eapi.AnimatePos(obj.tile, eapi.ANIM_REVERSE_LOOP, fatPos, pulse, aOf)
	eapi.AnimateSize(obj.tile, eapi.ANIM_REVERSE_LOOP, fatSize, pulse, aOf)
	eapi.SetColor(obj.tile, lightPink)
	AddShadow(obj, aOf)
end

flower = {
	Put = Put,
	Jump = Jump,
}
return flower
