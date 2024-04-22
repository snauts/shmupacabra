local rocketAcc = 300
local turnSpeed = 2 -- seconds to turn 360
local bomberFile = { "image/bomber.png", filter = true }
local bomberImg = eapi.ChopImage(bomberFile, { 128, 64 })

local swayFile = { "image/bomber-sway.png", filter = true }
local swayImg = eapi.ChopImage(swayFile, { 128, 64 })

local function Explode(mob)
	explode.Area(vector.null, mob.body, 10, 48, 0.04)
	powerup.ArcOfStars(mob, 5)
	actor.DelayedDelete(mob, 0.25)
	actor.DeleteShape(mob)
end

local function Launch(pos, vel, Strategy, z)
	z = z or 1
	local init = { health = 7,
		       class = "Mob",		       
		       velocity = vel,
		       sprite = bomberImg,
		       offset = { x = -64, y = -32 },
		       bb = { b = -16, t = 16, l = -16, r = 16 },
		       angle = vector.Angle(vel),
		       Shoot = Explode,		       
		       flame = { },
		       pos = pos,
		       z = z, }
	local obj = actor.Create(init)
	util.RotateTile(obj.tile, obj.angle + 90)

	z = z + 0.1
	local function Flame(offset, x)
		local img = player.burner
		local rnd = util.Random()
		local size1 = { x = 32, y = 64 }
		local size2 = { x = 28, y = 64 }
		local tile = eapi.NewTile(obj.body, offset, size1, img, z)
		eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, size2, 0.1, rnd)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
		util.RotateTile(tile, obj.angle - 180)
		return tile
	end
	obj.flame[1] = Flame({ x = 10, y = -22 })
	obj.flame[2] = Flame({ x = 10, y = -42 })

	util.MaybeCall(Strategy, obj)
	return obj
end

local function Scissor(MoreStrategy)
	return function(obj)
		local function Pos() return actor.GetPos(obj) end
		local vel = bullet.HomingVector(player.Pos(), Pos())
		local function FullArc()
			bullet.FullArc(Pos(), vel, bullet.Magenta, 15)
		end
		util.Repeater(FullArc, 12, 0.05, obj.body)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local swooperZ = -25
local function Swoop(pos, vel, Strategy)
	vel = vel or { x = 0, y = 400 }
	local init = { velocity = vel,
		       angle = vector.Angle(vel),
		       spriteSize = { x = 64, y = 32 },
		       offset = { x = -32, y = -16 },
		       sprite = bomberImg,
		       pos = pos,
		       z = swooperZ, }
	local obj = actor.Create(init)	
	swooperZ = swooperZ - 0.0001
	eapi.SetAcc(obj.body, { x = 0, y = -0.5 * vel.y } )
	util.RotateTile(obj.tile, obj.angle - 90)
	eapi.SetColor(obj.tile, util.Gray(0.6))
	eapi.SetFrame(obj.tile, 31)

	local function Switch()
		Launch(actor.GetPos(obj), eapi.GetVel(obj.body), Strategy)
		actor.Delete(obj)
	end
	
	local size = { x = 128, y = 64 }
	local offset = { x = -64, y = -32 }
	local white = util.Gray(1.0)
	local function Flip()		
		eapi.Animate(obj.tile, eapi.ANIM_CLAMP, -32, 0)
 		eapi.AnimateSize(obj.tile, eapi.ANIM_CLAMP, size, 1, 0)
		eapi.AnimatePos(obj.tile, eapi.ANIM_CLAMP, offset, 1, 0)
		eapi.AnimateColor(obj.tile, eapi.ANIM_CLAMP, white, 1, 0)
		eapi.AddTimer(obj.body, 1.0, Switch)
	end
	eapi.AddTimer(obj.body, 1.5, Flip)
end

local function AnimateAngle(obj, angle, duration)
	actor.AnimateAngle(obj.flame[1], angle - 180, duration)
	actor.AnimateAngle(obj.flame[2], angle - 180, duration)
	actor.AnimateAngle(obj.tile, angle + 90, duration)
end

local function Accelerate(delay, dir, MoreStrategy)
	return function(obj)
		local function Start()
			eapi.SetAcc(obj.body, dir)
			actor.AnimateToVelocity(obj, turnSpeed, AnimateAngle)
		end
		eapi.AddTimer(obj.body, delay, Start)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local function ShootBullet(obj)
	local pos = actor.GetPos(obj)
	local vel = bullet.HomingVector(player.Pos(), pos, bullet.speed)
	bullet.CyanSpin(pos, vector.Rnd(vel, 32))
end

local function Attack(delay, count, interval, MoreStrategy)
	return function(obj)
		local function Shoot()
			ShootBullet(obj)
		end
		local function Start()
			util.Repeater(Shoot, count, interval, obj.body)
		end
		eapi.AddTimer(obj.body, delay, Start)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local function Lines(delay, MoreStrategy)
	return function(obj)
		local aim = nil
		local function Shoot()
			local pos = actor.GetPos(obj)
			bullet.AimArc(pos, aim, bullet.Cyan, 5, 60, 400)
		end
		local function Start()
			aim = player.Pos()
			util.Repeater(Shoot, 5, 0.05, obj.body)
		end
		eapi.AddTimer(obj.body, delay, Start)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local function Swayer(pos, vel)
	vel = vel or { x = 0, y = 400 }
	local init = { velocity = vel,
		       angle = vector.Angle(vel),
		       offset = { x = -64, y = -32 },
		       sprite = swayImg,
		       pos = pos,
		       z = swooperZ, }
	local obj = actor.Create(init)	
	swooperZ = swooperZ - 0.0001
	eapi.SetAcc(obj.body, { x = -0.5 * vel.x, y = -100 })
	util.RotateTile(obj.tile, obj.angle + 90)
	
	eapi.Animate(obj.tile, eapi.ANIM_CLAMP, 32, 0)
	eapi.AnimateColor(obj.tile, eapi.ANIM_CLAMP, util.Gray(0.6), 1, 0)
	local function Stop()
		eapi.SetAcc(obj.body, { x = 0, y = -100 })
	end
	eapi.AddTimer(obj.body, 1, Stop)
	local function Destroy()
		actor.Delete(obj)
	end
	eapi.AddTimer(obj.body, 3, Destroy)
end

local function Sway(delay, MoreStrategy)
	return function(obj)
		local function Start()
			Swayer(actor.GetPos(obj), eapi.GetVel(obj.body))
			actor.Delete(obj)
		end
		eapi.AddTimer(obj.body, delay, Start)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local function Arc(dir, delay, MoreStrategy)
	return function(obj)
		local pos = actor.GetPos(obj)
		local vel = bullet.HomingVector(player.Pos(), pos, 300)
		vel = vector.Rotate(vel, dir * 120)
		local function Start()
			eapi.AddTimer(obj.body, 0.05, Start)
			local pos = actor.GetPos(obj)	
			vel = vector.Rotate(vel, -10 * dir)
			bullet.Magenta(pos, vel)
		end
		eapi.AddTimer(obj.body, delay, Start)
		util.MaybeCall(MoreStrategy, obj)
	end
end

local function AddRocket(obj, offset)
	local z = obj.z - 0.1
	local tile = eapi.NewTile(obj.body, offset, nil, rocket.img, z)
	util.RotateTile(tile, obj.angle - 90)
	eapi.FlipY(tile, true)
	return tile
end

local function LaunchRocket(obj, offset)
	local pos = actor.GetPos(obj)
	offset = vector.Add(offset, { x = 16, y = 32 })
	offset = vector.Rotate(offset, obj.angle - 90)
	pos = vector.Add(pos, offset)

	local z = obj.z - 0.1
	local vel = eapi.GetVel(obj.body)
	rocket.Launch(pos, vel, vector.Normalize(vel, rocketAcc), nil, z)
end

local rocketOffsets = { { x = 16, y = -32 }, { x = -48, y = -32 } }

local function Rocket(delays, MoreStrategy)
	return function(obj)		       
		local rockets = { }
		local function Start(i)
			eapi.Destroy(rockets[i])
			LaunchRocket(obj, rocketOffsets[i])
		end
		for i = 1, 2, 1 do
			local function StartRocket() Start(i) end
			eapi.AddTimer(obj.body, delays[i], StartRocket)
			rockets[i] = AddRocket(obj, rocketOffsets[i])
		end
		util.MaybeCall(MoreStrategy, obj)
	end
end

bomber = {
	Arc = Arc,
	Sway = Sway,
	Lines = Lines,
	Swoop = Swoop,
	Launch = Launch,
	Attack = Attack,
	Rocket = Rocket,
	Scissor = Scissor,
	Accelerate = Accelerate,
}
return bomber
