local function LoadTexture(fileName)
	return eapi.ChopImage(fileName, { 64, 64 })
end

local planeSimple = LoadTexture({ "image/plane.png", filter = true })
local planeFlip   = LoadTexture({ "image/plane-flip.png", filter = true })

local kamikazeSpeed = 600
local kamikazeVelocity = { x = -kamikazeSpeed, y = 0 }

local function PlaneExplode(mob)
	explode.Area(vector.null, mob.body, 10, 32, 0.05)
	powerup.ArcOfStars(mob, 3)
end

local function Explode(mob)
	PlaneExplode(mob)
	actor.Delete(mob)
end

local function Simple(pos, gravity)
	local init = { health = 3,
		       class = "Mob",		       
		       sprite = planeSimple,
		       offset = { x = -32, y = -32 },
		       velocity = kamikazeVelocity,
		       decalOffset = { x = 0, y = -4 },
		       bb = { b = -16, t = 12, l = -8, r = 24 },
		       Shoot = Explode,
		       pos = pos,
		       z = 1, }
	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 24, util.Random())
	if gravity then eapi.SetAcc(obj.body, gravity) end
	return obj
end

local function Kamikaze(pos, gravity)
	Simple(pos, gravity)
end

local inversePlaneBB = { b = -8, t = 16, l = -24, r = 8 }

local function DefaultRetreatingPattern(obj, pos)
	bullet.AimLine(pos, player.Pos(), bullet.MagentaLong, 5, 0.85, 1.15)
end

local function WidePattern(obj, pos)
	local i = 1
	local target = vector.Offset(player.Pos(), 0, 32 - util.Random(0, 64))
	local function NextArc()
		if i <= 3 then
			bullet.AimArc(pos, target, bullet.Magenta, i, 2 * i)
			eapi.AddTimer(obj.body, 0.075, NextArc)
			i = i + 1
		end
	end
	NextArc()
end

local function ThreadPattern(obj, pos)
	local angle = 2.0
	local speed = 250
	local step = 15
	local aim = bullet.HomingVector(player.Pos(), pos)
	local function Emit()
		local currentPos = actor.GetPos(obj)
		local vel = vector.Rotate(aim, angle)
		vel = vector.Normalize(vel, speed)
		bullet.Magenta(currentPos, vel)
		speed = speed + step
		angle = angle - 0.4
		step = step - 1
	end
	util.Repeater(Emit, 10, 0.02, obj.body)
end

local function Retreating(pos, retreatTime, Pattern)
	local obj = Simple(pos)
	obj.health = 5
	local function BackAnim()
		actor.SwapTile(obj, planeSimple, eapi.ANIM_LOOP, 32, 0.5)
                eapi.FlipX(obj.tile, true)
                eapi.FlipY(obj.tile, true)
		eapi.SetAcc(obj.body, { x = 0, y = 0 })
		local pos = actor.GetPos(obj)
		util.MaybeCall(Pattern or DefaultRetreatingPattern, obj, pos)
		actor.DeleteShape(obj)
		obj.bb = inversePlaneBB
		actor.MakeShape(obj)
	end
	local function Retreat()
		eapi.SetAcc(obj.body, { x = 1300, y = 0 })
		actor.SwapTile(obj, planeFlip, eapi.ANIM_CLAMP, 48)
		eapi.AddTimer(obj.body, 0.66, BackAnim)
	end
	eapi.AddTimer(obj.body, retreatTime, Retreat) 
end

local function Transporter(pos, deliverTime, velocity)
	local init = { health = 5,
		       class = "Mob",		       
		       sprite = planeSimple,
		       offset = { x = -16, y = -16 },
		       spriteSize = { x = 32, y = 32 },
		       velocity = velocity,
		       Shoot = Explode,
		       pos = pos,
		       z = -25.0, }
	local obj = actor.Create(init)
	eapi.SetColor(obj.tile, util.Gray(0.5))
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 24, util.Random())

	local offset = { x = 6, y = -8 }
	local blobTile = bullet.CyanBlobSmall(obj.body, obj.z - 0.1, offset)

	local function BackAnim()
		obj.spriteSize = nil
		obj.offset = { x = -32, y = -32 }
		actor.SwapTile(obj, planeSimple, eapi.ANIM_LOOP, 32, 0.5)
                eapi.FlipX(obj.tile, true)
                eapi.FlipY(obj.tile, true)
		eapi.SetAcc(obj.body, { x = 500, y = -100 })
		obj.bb = inversePlaneBB
		actor.MakeShape(obj)
	end
	local function Retreat()
		eapi.SetAcc(obj.body, { x = 100, y = 0 })
		actor.SwapTile(obj, planeFlip, eapi.ANIM_CLAMP, 32)

		eapi.SetColor(obj.tile, util.Gray(0.5))
		eapi.AnimateColor(obj.tile, eapi.ANIM_CLAMP,
				  util.Gray(1.0), 1, 0)
		eapi.AnimateSize(obj.tile, eapi.ANIM_CLAMP,
				 { x = 64, y = 64 }, 1, 0)
		eapi.AnimatePos(obj.tile, eapi.ANIM_CLAMP,
				{ x = -32, y = -32 }, 1, 0)

		eapi.AddTimer(obj.body, 1.0, BackAnim)
		local pos = vector.Add(actor.GetPos(obj), { x = 12, y = 0 })
		bullet.CyanBlobZoom(pos, { x = -50, y = 0 }, 1, obj.z - 0.1)
		eapi.Destroy(blobTile)
	end
	eapi.AddTimer(obj.body, deliverTime, Retreat) 
	return obj	
end

local function Formation(pos)	
	local size = { x = 32, y = 32 }
	local body = eapi.NewBody(gameWorld, pos)
	local function AddPlane(offset, dark)
		local tile = eapi.NewTile(body, offset, size, planeSimple, -25)
		eapi.SetColor(tile, util.Gray(dark))		
		eapi.SetFrame(tile, 30)
	end

	AddPlane({ x = 40, y =  30 }, 0.30)
	AddPlane({ x = 20, y =  15 }, 0.35)
	AddPlane({ x =  0, y =   0 }, 0.40)
	AddPlane({ x = 30, y = -20 }, 0.45)
	AddPlane({ x = 60, y = -40 }, 0.50)

	eapi.SetVel(body, { x = -400, y = 0 })
	util.DelayedDestroy(body, 2.5)
end

local function AnimateAngle(obj, angle, duration)
	actor.AnimateAngle(obj.tile, angle - 180, duration)
end

local function Follower(pos)
	local obj = Simple(pos)
	obj.angle = vector.Angle(kamikazeVelocity)
	actor.AnimateToVelocity(obj, 1, AnimateAngle)
	local function Follow()
		local pos1 = player.Pos()
		local pos2 = actor.GetPos(obj)
		local diff = pos2.y - pos1.y
		local scale = pos2.x - pos1.x
		if scale < 64 then
			eapi.SetAcc(obj.body, vector.null)
		else
			scale = kamikazeSpeed / math.abs(scale)
			eapi.SetAcc(obj.body, { x = 0, y = -scale * diff })
			eapi.AddTimer(obj.body, 0.2, Follow)
		end 
	end
	Follow()
end

local function BackRunner(pos, vel, acc)
	local obj = Simple(pos, acc)
	eapi.SetVel(obj.body, vel)
	obj.angle = vector.Angle(vel)
	actor.AnimateToVelocity(obj, 1, AnimateAngle)
	local function Emit()
		if util.Random() > 0.5 then
			local pos = actor.GetPos(obj)
			local vel = bullet.HomingVector(player.Pos(), pos)
			bullet.MagentaLong(pos, vel)
		end
	end
	local function Start()
		util.Repeater(Emit, 3, 0.2, obj.body)
	end
	eapi.AddTimer(obj.body, 0.2 + 0.2 * util.Random(), Start)
end

local function Leaver(pos, vel, acc)
	local obj = Simple(pos, acc)
	eapi.SetVel(obj.body, vel)
	obj.angle = vector.Angle(vel)
	util.RotateTile(obj.tile, obj.angle - 180)
	actor.AnimateToVelocity(obj, 1, AnimateAngle)
	local dir = util.Sign(vel.y)
	local baseVel = { x = -bullet.speed, y = 0 }
	local function Bullet()
		local pos = actor.GetPos(obj)
		local projectile = bullet.Cyan(pos, vector.null)
		local angle = -actor.GetPos(projectile).y / 4 - dir * 30
		eapi.SetVel(projectile.body, vector.Rotate(baseVel, angle))
	end
	local function Emit()
		util.Repeater(Bullet, 6, 0.02, obj.body)
	end
	local function Start()
		util.Repeater(Emit, 5, 0.2, obj.body)
	end
	eapi.AddTimer(obj.body, 0.3, Start)
end

local kamikazeRadius = kamikazeSpeed / (2 * math.pi)

local function Circler(pos, vel, Emit)
	local parent = nil
	local obj = Simple(pos)
	eapi.SetVel(obj.body, vel)
	obj.angle = vector.Angle(vel)
	util.RotateTile(obj.tile, obj.angle - 180)
	actor.AnimateToVelocity(obj, 1, AnimateAngle)

	local function Unlink()
		if obj.linked then
			eapi.Unlink(obj.body)
			obj.linked = false
		end
	end
	local function LinkTo(parent)
		if not obj.linked then
			eapi.Link(obj.body, parent)
			obj.linked = true
		end
	end
	obj.OnDelete = function()
		if parent then 
			Unlink()
			eapi.Destroy(parent)
		end
	end	
	local function Straighten()
		local vel = actor.GetDir(obj.body)
		eapi.SetStepC(obj.body, eapi.STEPFUNC_STD)
		eapi.SetVel(obj.body, vector.Normalize(vel, kamikazeSpeed))
		Unlink()
	end
	local function Circle()
		local pos = actor.GetPos(obj)
		local radius = vector.Normalize(vel, kamikazeRadius)
		local parentPos = vector.Add(pos, vector.Rotate(radius, 90))
		parent = eapi.NewBody(gameWorld, parentPos)
		LinkTo(parent)
		eapi.SetStepC(obj.body, eapi.STEPFUNC_ROT, 2 * math.pi)
		eapi.AddTimer(obj.body, 0.75, Straighten)
	end
	eapi.AddTimer(obj.body, 0.5, Circle)
	util.Delay(obj.body, 0.4, Emit, obj)
end

local jetLen = 1024
local function JetFlame(color, width, duration, z, obj) 
	local size = { x = 2, y = 2 * width }
	local offset = { x = -1, y = -width }
	local tile = eapi.NewTile(obj.body, offset, size, player.flame, z)
	eapi.SetColor(tile, color)
	util.RotateTile(tile, obj.angle)	
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, duration, 0)

	local size = { x = jetLen, y = 4 * width }
	local offset = { x = -jetLen + 128 - width, y = -2 * width }
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size, duration, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset, duration, 0)
end

local expandVel = { x = 10, y = 0 }
local function CyanCircle(pos, masterVel)
	local scale = 0.25
	masterVel = vector.Normalize(masterVel, bullet.speed)
	local function Cyan(pos, vel) 
		bullet.CyanCustom(pos, vector.Add(vel, masterVel), scale)
		scale = 0.75 - scale
	end
	bullet.FullArc(pos, expandVel, Cyan, 10)
end

local function JetExplode(mob)
	PlaneExplode(mob)
	eapi.Destroy(mob.tile)
	actor.DelayedDelete(mob, 1)
	CyanCircle(eapi.GetPos(mob.body), eapi.GetVel(mob.body))
	eapi.SetVel(mob.body, vector.null)
	eapi.SetAcc(mob.body, vector.null)
end

local function SuperJet(pos, vel)
	local obj = Simple(pos)
	obj.Shoot = JetExplode
	obj.angle = vector.Angle(vel)
	eapi.SetVel(obj.body, vector.Normalize(vel, 400))
	eapi.SetAcc(obj.body, vector.Normalize(vel, -400))
	util.RotateTile(obj.tile, obj.angle - 180)
	
	local function Launch()
		local red = { r = 1.0, g = 0.5, b = 0.5 }
		local white = { r = 1.0, g = 1.0, b = 1.0 }
		local duration = jetLen / vector.Length(vel)
		JetFlame(white, 32, duration, obj.z - 0.1, obj)
		JetFlame(red, 64, duration, obj.z - 0.2, obj)
		eapi.PlaySound(gameWorld, "sound/jet.ogg", 0, 0.25)
		eapi.SetAcc(obj.body, vel)
	end
	eapi.AddTimer(obj.body, 0.5, Launch)
end

plane = {
	Leaver = Leaver,
	Circler = Circler,
	Follower = Follower,
	Kamikaze = Kamikaze,
	SuperJet = SuperJet,
	Formation = Formation,
	Retreating = Retreating,
	BackRunner = BackRunner,
	Transporter = Transporter,
	WidePattern = WidePattern,
	velocity = kamikazeVelocity,
	ThreadPattern = ThreadPattern,
}
return plane
