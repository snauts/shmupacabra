local barrelRoll = 2 -- time it takes to roll barrel 180 degrees
local tankVelocity = { x = -40, y = 0 }

local tracksImg = eapi.ChopImage("image/tracks.png", { 192, 96 })
local turretBack = eapi.ChopImage("image/turret-back.png", { 192, 96 })
local turretFront = eapi.ChopImage("image/turret-front.png", { 192, 96 })
local turretShadow = eapi.ChopImage("image/turret-shadow.png", { 192, 96 })

local barrelFile = { "image/barrel.png", filter = true }
local barrelImg = eapi.ChopImage(barrelFile, { 192, 32 })

local function TankShoot(obj)
	explode.Bulk(obj, 40, 80)
	actor.DelayedDelete(obj, 0.25)
end

local frame = { { 1, 1 }, { 62, 30 } }
local shadow = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)

local function TracksOnDelete(obj)
	obj.sibling.sibling = nil
end

local offset = { x = -96, y = -48 }
local function Tracks(pos, health, standing)
	local init = { health = health,
		       jerk = 8,
		       class = "Mob",		       
		       sprite = tracksImg,
		       offset = offset,
		       pos = pos or { x = 496, y = -190 },
		       bb = { l = -84, r = 84, b = -40, t = 8 },
		       Shoot = actor.SiblingShoot(TankShoot),
		       Hit = actor.SiblingHit,
		       OnDelete = TracksOnDelete,
		       ignoreStreet = true,
		       z = -5, }
	local obj = actor.Create(init)
	if not standing then eapi.Animate(obj.tile, eapi.ANIM_LOOP, -32, 0) end

	-- shadow
	local size = { x = 240, y = 80 }
	local offset =  { x = -120, y = -52 }
	local tile = eapi.NewTile(obj.body, offset, size, shadow, -5.1)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0.5 })

	return obj
end

local function CallContinuation(obj)
	util.MaybeCall(obj.Continuation)
	obj.Continuation = nil
end

local function StarCloud(pos, vel)
	bullet.Cloud(pos, vel, powerup.Star, 5, 40)
end

local function TurretShoot(obj)
	CallContinuation(obj)
	local pos = actor.GetPos(obj)
	actor.DelayedDelete(obj, 0.25)
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	bullet.Arc(pos, { x = -100, y = 200 }, StarCloud, 5, 135)
	if not obj.standing then bullet.Sweep() end
end

local function BarrelShoot(obj)
	eapi.Animate(obj.barrelTile, eapi.ANIM_CLAMP, 32, 0)
end

local function AimBarrel(obj, angle, Callback)
	local function CallbackWrapper()
		obj.barrelAngle = angle
		Callback(obj)
	end

	local diff = math.abs(obj.barrelAngle - angle)
	local time = diff * obj.barrelRoll / 180
	eapi.AddTimer(obj.body, time, CallbackWrapper)

	eapi.AnimateAngle(obj.barrelTile, eapi.ANIM_CLAMP, vector.null,
			  vector.Radians(angle), time, 0)	
end

local function TankCyan(pos, vel)
	vel = vector.Add(vel, tankVelocity)
	return bullet.Cyan(pos, vel)
end

local function TankAttack(obj, Pattern)
	local vel = vector.Rotate({ x = bullet.speed, y = 0 }, obj.barrelAngle)
	local function Attack()
		local pos = actor.GetPos(obj)
		pos = vector.Add(pos, vector.Normalize(vel, 140))
		eapi.PlaySound(gameWorld, "sound/cannon.ogg", 0, 0.5)
		Pattern(pos, vel)
	end
	-- barrel animates at 32FPS
	-- it is at it's lowest at 6th frame
	-- therefore we wait 6/32=0.1875s
	eapi.AddTimer(obj.body, 0.1875, Attack)
	BarrelShoot(obj)
end

local function AngleWithPlayer(obj)
	local diff = vector.Sub(player.Pos(), actor.GetPos(obj))
	local angle = vector.Angle(diff)
	if angle < -90 then angle = angle + 360 end
	return angle - obj.barrelAngle
end

local function GunCooldown(obj, Continue)
	if actor.GetPos(obj).x > -400 then
		eapi.AddTimer(obj.body, 0.5, Continue)	
	else
		CallContinuation(obj)
	end
end

local function TurretAttackTypeA(obj)
	local function ClockWorkAttack(pos, vel)
		for angle = -60, 60, 30 do
			local bulletVel = vector.Rotate(vel, angle)
			bullet.Line(pos, bulletVel, TankCyan, 5, 0.9, 1.1)
		end
	end
	TankAttack(obj, ClockWorkAttack)

	local function Continue()
		local diffAngle = AngleWithPlayer(obj)
		local absoluteStep = math.abs(obj.barrelStep)
		local angle = obj.barrelAngle - obj.barrelStep
		obj.barrelStep = -util.Sign(diffAngle) * absoluteStep
		if math.abs(diffAngle) > 40 then
			angle = angle + diffAngle
		end
		if angle <= 0 then
			obj.barrelStep = -absoluteStep
		elseif angle >= 180 then
			obj.barrelStep =  absoluteStep
		end
		angle = math.min(180, math.max(angle, 0))
		AimBarrel(obj, angle, TurretAttackTypeA)
	end
	GunCooldown(obj, Continue)
end	

local proximityTable = { }
local proximityBB = { l = -136, r = 136, b = -72, t = 136 }
local function AddProximity(obj)
	local shape = eapi.NewShape(obj.body, nil, proximityBB, "Proximity")
	proximityTable[shape] = obj
end

local function Defense(playerShape, proximityShape)
	local obj = proximityTable[proximityShape]
	proximityTable[proximityShape] = nil
	eapi.Destroy(proximityShape)

	local pos = vector.Offset(actor.GetPos(obj), -24, -8)
	bullet.FullArc(pos, bullet.velocity, bullet.Magenta, 16)
	eapi.AddTimer(obj.body, 0.5, function()	AddProximity(obj) end)
	eapi.PlaySound(gameWorld, "sound/plop.ogg")
end

actor.SimpleCollide("Player", "Proximity", Defense)

local function TypeA(obj)
	AimBarrel(obj, 150, TurretAttackTypeA)
end

local function TurretAttackTypeB(obj)
	local function ClockWorkAttack(pos, vel)
		vel = vector.Scale(vel, 2)
		local blob = bullet.CyanBlob(pos, vel)
		eapi.SetAcc(blob.body, { x = 0, y = -600 })
	end
	TankAttack(obj, ClockWorkAttack)

	local function Continue()
		obj.count = obj.count - 1
		local angle = obj.barrelAngle - ((obj.count < 0) and 5 or 0)
		AimBarrel(obj, angle, TurretAttackTypeB)
	end
	GunCooldown(obj, Continue)
end	

local function TypeB(obj)
	obj.count = 8
	AimBarrel(obj, 140, TurretAttackTypeB)	
end

local function MagentaSpinEmitter(pos, vel)
	local projectile = bullet.MagentaSpin(pos, vel)
	local function Emit()
		local pos = actor.GetPos(projectile)
		if pos.x > -400 and pos.y < 240 then 
			local vel = eapi.GetPos(projectile.body)
			vel = vector.Normalize(vel, 80)
			local obj = bullet.MagentaLong(pos, vel)
			eapi.SetAcc(obj.body, vector.Scale(vel, 0.2))
			eapi.AddTimer(projectile.body, 0.05, Emit)
		end
	end
	Emit()
	return projectile
end

local function BulletRocket(obj, pos, vel)
	vel = vector.Add(vel, eapi.GetVel(obj.body))
	bullet.MagentaSpin(pos, vel)
	
	pos = vector.Add(pos, vector.Normalize(vel, -16))
	local center = bullet.Dummy(pos, vel)

	local centerPos = actor.GetPos(center)
	local offset = { x = 8, y = 0 }
	
	for i = 0, 3, 1 do
		local childPos = vector.Add(centerPos, offset)
		local child = MagentaSpinEmitter(childPos, vector.null)
		eapi.SetStepC(child.body, eapi.STEPFUNC_ROT, 2 * obj.spiral)
		offset = vector.Rotate(offset, 90)
		actor.Link(child, center)
	end
end

local function TurretAttackTypeC(obj)
	local function BulletRocketAttack(pos, vel)
		BulletRocket(obj, pos, vector.Normalize(vel, 200))
		local progress = 1
		local function Explode()
			local objPos = actor.GetPos(obj)
			local offset = vector.Sub(pos, objPos)
			offset = vector.Scale(offset, progress)
			explode.Simple(offset, obj.body, -4)
			if progress < 0.5 and obj.barrelTile then 
				eapi.Destroy(obj.barrelTile)
				obj.barrelTile = nil
			end
			if progress > 0.0 then
				eapi.AddTimer(obj.body, 0.05, Explode)
				progress = progress - 0.25
			end
		end
		Explode()
	end
	TankAttack(obj, BulletRocketAttack)
end

local function TypeC(angle, spiral)
	return function(obj)
		obj.spiral = spiral
		AimBarrel(obj, angle, TurretAttackTypeC)	
	end
end

local offset = { x = -96, y = -62 }
local barrelOffset = { x = -16, y = -16 }
local shadowOffset = { x = -96, y = -63 }
local function Turret(pos, Continuation, Pattern, standing, velocity, angle)
	local health = standing and 50 or 200
	local tracks = Tracks(pos, health, standing)
	local init = { health = health,
		       class = "Mob",		       
		       jerk = 8,
		       sprite = turretFront,
		       offset = offset,
		       standing = standing,
		       velocity = velocity or tankVelocity,
		       Continuation = Continuation,
		       pos = vector.Offset(tracks.pos, 0, 44),
		       bb = { l = -32, r = 32, b = -36, t = 16 },
		       Shoot = actor.SiblingShoot(TurretShoot),
		       barrelRoll = barrelRoll,
		       Hit = actor.SiblingHit,
		       sibling = tracks,
		       barrelAngle = angle or 120,
		       barrelStep = -10,
		       z = -4, }
	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 16, 0)
	tracks.sibling = obj

	local function TurretTile(offset, sprite, z)
		return eapi.NewTile(obj.body, offset, nil, sprite, z)
	end
	obj.backTile = TurretTile(offset, turretBack, -4.5)
	obj.barrelTile = TurretTile(barrelOffset, barrelImg, -4.3)
	obj.shadowTile = TurretTile(shadowOffset, turretShadow, -4.7)
	
	if not velocity then actor.Rush(obj, 0.9, 400, -tankVelocity.x) end
	util.RotateTile(obj.barrelTile, obj.barrelAngle)
	eapi.FlipX(obj.barrelTile, true)
	util.MaybeCall(Pattern or TypeA, obj)
	eapi.Link(tracks.body, obj.body)
	if not standing then AddProximity(obj) end

	return obj
end

tank = {
	TypeB = TypeB,
	TypeC = TypeC,
	Tracks = Tracks,
	Turret = Turret,
	shadow = shadow,
}
return tank
