local defaultSize = { x = 256, y = 128 }
local defaultVelocity = { x = 200, y = 0 }

local gatlingFile = { "image/gatling.png", filter = true }
local gatlingImg = eapi.ChopImage(gatlingFile, defaultSize)

local function Explode(mob)
	local dir = vector.Normalize(mob.velocity)
	local radius = 48 * mob.scale
	for scale = 2 * radius, 0, -radius do 
		local pos = vector.Scale(dir, scale)
		explode.Area(pos, mob.body, 10, radius, 0.05)
	end
	powerup.ArcOfStars(mob, 5)
	actor.DelayedDelete(mob, 0.7)
	actor.DeleteShape(mob)
	mob.exploding = true
end

local function Animate(projectile)
	local plop = eapi.Clone(projectile.tile)
	eapi.Animate(plop, eapi.ANIM_CLAMP, 64, 0)
	eapi.AddTimer(projectile.body, 0.25, util.DestroyClosure(plop))
	eapi.AnimateAngle(projectile.tile, eapi.ANIM_LOOP, 
			  vector.null, 2 * math.pi, 0.05, 0)
end

local function Spirrral(obj, offset, count, amplitude)
	local body = obj.body
	count = count or 100
	offset = offset or 0
	amplitude = amplitude or 10
	local arm = { x = 1, y = 0 }
	local step = 300 / amplitude
	local interval = 1 / amplitude
	local vel = vector.Normalize(eapi.GetVel(body), bullet.speed)
	local function Emit()
		count = count - 1
		local pos = eapi.GetPos(body)
		local speed = arm.x * 0.1 + 0.9
		local scale = arm.x * 0.25 + 0.75
		local angle = amplitude * math.atan2(arm.y, 1)
		local direction = vector.Rotate(vel, angle)
		bullet.z_epsilon = 0.001 * scale 
		pos = vector.Add(pos, vector.Normalize(direction, offset))
		local velocity = vector.Scale(direction, speed)
		Animate(bullet.CyanCustom(pos, velocity, scale))
		if count > 0 and not obj.exploding then 
			eapi.AddTimer(body, interval, Emit)
		end
		arm = vector.Rotate(arm, step)
	end
	Emit()
end

local function Launch(pos, vel, scale)
	scale = scale or 1
	vel = vel or defaultVelocity
	local size = vector.Scale(defaultSize, scale)
	local init = { health = 20 * scale,
		       class = "Mob",		       
		       sprite = gatlingImg,
		       spriteSize = size,
		       velocity = vector.Scale(vel, 2),
		       offset = vector.Scale(size, -0.5),
		       bb = actor.Square(20 * scale),
		       angle = vector.Angle(vel) - 180,
		       Shoot = Explode,
		       scale = scale,
		       pos = pos,
		       z = 2, }

	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 32, 0)
	util.RotateTile(obj.tile, obj.angle)

	eapi.SetAcc(obj.body, vector.Scale(vel, -1))
	local function Stop() eapi.SetAcc(obj.body, vector.null) end	
	eapi.AddTimer(obj.body, 0.5, Stop)

	local function AddShape(offset, size)
		local pos = vector.Normalize(vel, offset * scale)
		actor.MakeShape(obj, actor.BoxAtPos(pos, size * scale))
	end
	AddShape( 48, 28)
	AddShape(-48, 28)

	local size = vector.Scale({ x = 128, y = 160 }, scale)
	local offset = vector.Scale({ x = -160, y = -80 }, scale)
	obj.flame = eapi.NewTile(obj.body, offset, size, player.flame, 1.9)
	eapi.Animate(obj.flame, eapi.ANIM_LOOP, 32, util.Random())
	util.RotateTile(obj.flame, obj.angle + 180)	

	local function EmitSpiral()
		Spirrral(obj, 80 * scale, 60 * scale, 24 * scale)
	end
	eapi.AddTimer(obj.body, 0.5, EmitSpiral)
end

local function Homing(scale, height)
	local playerPos = player.Pos()
	local pos = { x = 400 + 128 * scale, y = height }
	Launch(pos, bullet.HomingVector(player.Pos(), pos, 100), scale)
end

gatling = {
	Launch = Launch,
	Homing = Homing,
	Spirrral = Spirrral,
}
return gatling
