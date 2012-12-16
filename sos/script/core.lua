local shellFile = { "image/shell.png", filter = true }
local shellImg = eapi.ChopImage(shellFile, { 512, 64 })

local ribHealth = 54
local shellSegments = 16
local cageHealth = 1000
local starHealth = 800
local shieldHealth = 1500
local shellHealth = (2 * shellSegments * ribHealth)
local totalHealth = shellHealth + shieldHealth + cageHealth + starHealth

local function RibExplode(rib)
	local x = 0
	local delay = 0
	local pos = actor.GetPos(rib)
	local function Explode()
		local pos = { x = x, y = 0 }
		explode.Simple(pos, rib.body, rib.z + 1)
		x = x + 32
	end
	for i = pos.x, 464, 32 do
		eapi.AddTimer(rib.body, delay, Explode)
		delay = delay + 0.05
	end

	local dark = util.Gray(0.4)
	local size = { x = 512, y = 72 }
	eapi.SetColor(rib.tile, util.Gray(0.6))
	eapi.AnimateColor(rib.tile, eapi.ANIM_REVERSE_LOOP, dark, 0.05, 0)
	eapi.AnimateSize(rib.tile, eapi.ANIM_REVERSE_LOOP, size, 0.05, 0)
end

local function RibShoot(rib)
	actor.DeleteShape(rib)
end

local function Rib(obj, i, dir, index)
	local delay = 0.5
	local start = -0.1 * delay * i
	local x = (dir > 0) and 0 or -512
	local offset = { x = x, y = i }
	local init = {
		i = i,
		dir = dir,
		parentBody = obj.follow[dir],
		pos = { x = (8 + i) * i, y = dir * (14 * i - 4) },
		offset = { x = x, y = 0 },
		ignoreStreet = true,
		sprite = shellImg,
		Shoot = util.Noop,
		Hit = obj.Hit,
		health = ribHealth,
		parent = obj,
		class = "Mob",
		z = 10 - i * 0.01,
		x = 32 - 2 * i,
	}
	local rib = actor.Create(init)
	if i == 0 and dir > 0 then obj.centerRib = rib end
	obj.segments[rib] = rib
	eapi.FlipX(rib.tile, dir < 0)
	local startAngle, endAngle
	if i == 0 then
		local amplitude = 1.2
		startAngle = dir > 0 and amplitude or 180 + amplitude
		endAngle = dir > 0 and -amplitude or 180 - amplitude
	else
		startAngle = dir > 0 and i * 2 or 180
		endAngle = dir > 0 and 0 or 180 - i * 2
	end
	util.RotateTile(rib.tile, startAngle)
	eapi.Animate(rib.tile, eapi.ANIM_LOOP, 32, i / shellSegments)
	eapi.AnimatePos(rib.tile, eapi.ANIM_REVERSE_LOOP, offset, delay, start)
	eapi.AnimateAngle(rib.tile, eapi.ANIM_REVERSE_LOOP, vector.null, 
			  vector.Radians(endAngle), delay, start)
end

local function AddShape(obj)
	local function Shape(rib)
		local y = (rib.dir > 0) and 0 or -24
		local w = 64 + 20 * (shellSegments - rib.i)
		local bb = { l = rib.x, r = w, b = y + 4, t = y + 20 }
		actor.MakeShape(rib, bb)
	end
	util.Map(Shape, obj.segments)
end

local function Slam(rib, delay, amount, Fn)
	local body = rib.body
	local vel = { x = amount, y = 0 }
	local function Stop()
		eapi.SetVel(body, vector.null)
		eapi.SetPos(body, rib.pos)
	end
	local function Reverse()
		eapi.AddTimer(body, 4 * delay, Stop)
		eapi.SetVel(body, vel)
		if not rib.parent.stopAction then 
			util.MaybeCall(Fn)
		end
	end
	eapi.SetVel(body, vector.Scale(vel, -4))
	eapi.AddTimer(body, delay, Reverse)
end

local function Contract(obj, delay, amount)
	local function Move(rib)
		Slam(rib, delay, amount * (rib.i - shellSegments / 2))
	end
	util.Map(Move, obj.segments)
end

local function RibPos(rib)
	local pos = actor.GetPos(rib, gameWorld)
	return vector.Offset(pos, rib.x, rib.dir * 20)
end

local function Wave(obj, interval, Fn)
	local function Shoot(rib)
		local function SndFn()
			eapi.PlaySound(gameWorld, "sound/slam.ogg", 0, 0.1)
			Fn(rib, RibPos(rib))
		end
		Slam(rib, 0.1, -50, SndFn)
	end 
	local function Bam(rib)
		local delay = interval * (shellSegments - (rib.i + 1))
		util.Delay(rib.body, delay, Shoot, rib)
	end	
	util.Map(Bam, obj.segments)
end

local function CrossStich(obj)
	local width = 512
	local counter = 0
	local function Line(rib, pos)
		local yOffset = (util.golden * counter * width) % width
		local aim = vector.Offset(player.Pos(), 0, yOffset - width / 2)
		bullet.AimLine(pos, aim, bullet.MagentaLong, 5, 0.9, 1.1)
		counter = counter + 1
	end
	Wave(obj, 0.4, Line)
end

local function BamBam(obj)
	local totalCount = 5
	local function Bullet(rib, pos)
		local count = 0
		local function Cyan(pos, vel)
			pos = vector.Rnd(pos, 8)
			vel = vector.Rnd(vel, 8)
			local scale = 0.5 * (1 + (count / (totalCount - 1)))
			local projectile = bullet.CyanCustom(pos, vel, scale)
			count = count + 1
		end
		local function Line(pos, vel)
			bullet.Line(pos, vel, Cyan, totalCount, 0.6, 1.0)
			count = 0
		end
		bullet.AimArc(pos, player.Pos(), Line, 3, 70)
	end
	Wave(obj, 0.1, Bullet)
end

local function ManyCircles(obj)
	local function Bullet(rib, pos)
		local function Magenta(pos, vel)
			local scale = 1.0 - (math.abs(rib.i) / 32)
			bullet.MagentaCustom(pos, vel, scale)
		end
		local aim = bullet.HomingVector(player.Pos(), pos)
		bullet.FullArc(pos, aim, Magenta, 13)
	end
	Wave(obj, 0.1, Bullet)
end

local RingStich = util.Noop

local function Bang(obj, Fn)
	for delay = 0, 2, 1 do
		util.Delay(obj.body, delay, Fn, obj)
	end
end

local function BamCircles(obj)
	Bang(obj, ManyCircles)
	util.Delay(obj.body, 6, RingStich, obj)
end

local function ThrowMesh(obj)
	local count = 7
	local function Line(pos, vel)
		bullet.Line(pos, vel, bullet.CyanLong, 13, 0.5, 1.0)
	end
	local function Shoot(i)
		local rib = obj.sorted[i]
		if obj.stopAction then return end
		bullet.AimArc(RibPos(rib), vector.null, Line, count, 120)
		eapi.PlaySound(gameWorld, "sound/slam.ogg", 0, 0.5)
		Slam(rib, 0.1, -200)
	end
	local function EmitMesh()
		util.Map(Shoot, { -12, 12 })
		count = count + 1
		if count < 12 then eapi.AddTimer(obj.body, 1.0, EmitMesh) end
	end
	util.Delay(obj.body, 7, BamCircles, obj)
	EmitMesh()
end

local function Tights(obj)
	local doit = true
	local angles = { 45, 60, 90 }
	local function Line(pos, vel)
		bullet.Line(pos, vel, bullet.Magenta, 3, 0.95, 1.05)
	end
	local function Shoot(i)
		local rib = obj.sorted[i]
		if obj.stopAction then return end
		for i, tau in pairs(angles) do
			bullet.AimArc(RibPos(rib), player.Pos(), Line, 2, tau)
		end
		Slam(rib, 0.02, -200)
	end
	local function EmitTights()
		util.Map(Shoot, { -7, 7 })
		if doit then eapi.AddTimer(obj.body, 0.1, EmitTights) end
	end
	EmitTights()
	
	local dir = 2
	local function Arc(i)
		local counter = 0
		local function Cyan(pos, vel)
			local amount = 1.0 - 0.02 * math.abs(counter - 4)
			amount = amount + 0.1 * (util.Random() - 0.5)
			bullet.Cyan(pos, vector.Scale(vel, amount))
			counter = counter + 1
		end
		local rib = obj.sorted[i]
		local pos = RibPos(rib)
		local vel = bullet.HomingVector(player.Pos(), pos)
		if obj.stopAction then return end
		vel = vector.Rotate(vel, 3 * dir)
		bullet.Arc(pos, vel, Cyan, 9, 12)
		eapi.PlaySound(gameWorld, "sound/slam.ogg", 0, 0.3)
		Slam(rib, 0.1, -100)		
	end
	local function EmitArcs()
		Arc(dir)
		dir = -dir
		if doit then eapi.AddTimer(obj.body, 0.6, EmitArcs) end
	end
	EmitArcs()

	local function Stop()
		doit = false
	end
	eapi.AddTimer(obj.body, 5, Stop)
	util.Delay(obj.body, 6, ThrowMesh, obj)
end

local function BamBamBam(obj)
	Bang(obj, BamBam)
	util.Delay(obj.body, 6, Tights, obj)
end

local function SparklingRing(obj, i)	
	Contract(obj, 0.19, 20)
	local function Spin(pos, vel)
		return bullet.CyanSpin(pos, vector.Add(baseVel, vel))
	end
	local function Ring()
		if not obj.stopAction then 
			local vel = { x = -70 + 7 * i, y = 0 }
			local pos = actor.GetPos(obj.centerRib, gameWorld)
			pos = vector.Offset(pos, 50, -4)
			bullet.FullArc(pos, vel, bullet.CyanSpin, 25 - i)
			eapi.PlaySound(gameWorld, "sound/slam.ogg")
		end
	end
	eapi.AddTimer(obj.body, 0.1, Ring)
end

RingStich = function(obj)
	for i = 0, 2, 1 do 
		util.Delay(obj.body, i, SparklingRing, obj, i)
	end
	util.Delay(obj.body, 3, CrossStich, obj)
	util.Delay(obj.body, 10, BamBamBam, obj)
end
		
local function Flash()
	player.KillFlash(0.5, { r = 1, g = 1, b = 1, a = 0.75 })
	eapi.PlaySound(gameWorld, "sound/boom.ogg")
end

local function StageTwo(obj)
	for i = -1, 1, 2 do
		eapi.SetAcc(obj.follow[i], { x = 0, y = i * 120 })
	end
	local function Open(rib)
		local i = (shellSegments - rib.i)
		local angle = vector.Radians(rib.dir > 0 and -i or 180 + i)
		eapi.AnimateAngle(rib.tile, eapi.ANIM_REVERSE_LOOP,
				  vector.null, angle, 0.1, 0.0)
	end
	local function Explode(rib)
		if rib.i == 0 then RibExplode(rib) end
	end
	local function Bang()
		util.Map(Explode, obj.segments)
	end
	util.Delay(staticBody, 1.5, analDestructor.Shield, obj.healthObj)
	eapi.AddTimer(obj.body, 2.5, Flash)
	util.Repeater(Bang, 10, 0.3, obj.body)
	util.Map(Open, obj.segments)
	actor.DelayedDelete(obj, 3)
	obj.stopAction = true
	bullet.Sweep()
	Flash()
end

local function Health()
	local obj = { health = totalHealth, maxHealth = totalHealth }
	obj.Dec = function(amount)
		obj.health = obj.health - amount
		powerup.ShowProgress(obj)
	end
	obj.Dec(0)
	return obj
end

local function Launch()
	local pos = { x = 512, y = 0 }
	local init = {
		parentBody = eapi.NewBody(gameWorld, pos),
		pos = { x = 16, y = 0 },
		Shoot = util.Noop,
		healthObj = Health(),
		segments = { },
		sorted = { },
		follow = { },
	}
	local obj = actor.Create(init)
	obj.OnDelete = function()
		eapi.Unlink(obj.body)
		eapi.Destroy(obj.parentBody)
	end
	eapi.SetStepC(obj.body, eapi.STEPFUNC_ROT, -2 * math.pi)
	for i = -1, 1, 2 do
		obj.follow[i] = eapi.NewBody(obj.body, vector.null)
	end
	local nextRib = -16
	local healthBucket = ribHealth
	obj.Hit = function(victim, shoot)
		obj.healthObj.Dec(shoot.damage)
		healthBucket = healthBucket - shoot.damage
		if healthBucket <= 0 then
			RibExplode(obj.sorted[nextRib])
			nextRib = -(nextRib - ((nextRib < 0) and 0 or 1))
			healthBucket = ribHealth
			if nextRib == 0 then
				util.Map(RibShoot, obj.segments)
				StageTwo(obj)
			end
		end		
	end
	for i = 0, shellSegments - 1, 1 do
		Rib(obj, i,  1)
		Rib(obj, i, -1)
	end
	local function Sort(rib)
		obj.sorted[rib.dir * (rib.i + 1)] = rib
	end
	util.Map(Sort, obj.segments)
	local function Start()
		RingStich(obj)
		AddShape(obj)
	end
	local function Stop()
		eapi.SetVel(obj.parentBody, vector.null)
		eapi.SetAcc(obj.parentBody, vector.null)
		eapi.AddTimer(obj.parentBody, 1, Start)
	end
	eapi.SetVel(obj.parentBody, vector.Scale(pos, -1.0))
	eapi.SetAcc(obj.parentBody, vector.Scale(pos, 0.5))
	eapi.AddTimer(obj.parentBody, 2, Stop)
end

local shieldFile = { "image/shield.png", filter = true }
local shieldImg = eapi.ChopImage(shieldFile, { 256, 256 })
local halves = {
	[1] = eapi.NewSpriteList(shieldFile, { { 0, 0 }, { 256, 128 } }),
	[-1] = eapi.NewSpriteList(shieldFile, { { 0, 128 }, { 256, 128 } })
}

local function ShieldShape(obj, size)
	local stepAngle = 10
	local base = { x = size or 112, y = 0 }
	for angle = 0, 85, stepAngle do
		local arm1 = vector.Rotate(base, angle)
		local arm2 = vector.Rotate(base, angle + stepAngle)
		actor.MakeShape(obj, { b = arm1.y, t = arm2.y, 
				       l = -arm1.x, r = arm1.x })
		actor.MakeShape(obj, { b = -arm2.y, t = -arm1.y,
				       l = -arm1.x, r = arm1.x })
	end
end

local function LoopMovement(obj)
	local Hdir = -1
	local Vdir = -1
	eapi.SetVel(obj.body, obj.initVel)
	local function SetAcceleration()
		if obj.finish then return end 
		eapi.SetAcc(obj.body, { x = Hdir * 400, y = Vdir * 400 })
	end
	local function HLoop()
		Hdir = -Hdir
		SetAcceleration()
		eapi.AddTimer(obj.body, obj.hDelay, HLoop)
	end
	local function VLoop()
		Vdir = -Vdir
		SetAcceleration()
		eapi.AddTimer(obj.body, obj.vDelay, VLoop)
	end
	if obj.initVel.x == 0 then
		eapi.AddTimer(obj.body, 0.5 * obj.hDelay, HLoop)
	else
		HLoop()
	end
	VLoop()
end

local function ExplodeSpikes(ring, bend)
	eapi.Unlink(ring.body)
	local angle = 360 * eapi.GetTime(ring.body) / ring.speed
	local vel = vector.Rotate({ x = 800, y = 0 }, angle + bend)
	util.AnimateRotation(ring.tile, ring.speed / 20, angle + ring.angle)
	util.DelayedDestroy(ring.body, 2)
	eapi.SetVel(ring.body, vel)
end

local function ShieldHurt(obj)
	if not obj.ringsDestroyed then
		explode.Area(vector.null, obj.body, obj.z + 1, 96, 0.0)
		ExplodeSpikes(obj.rings[1], -90)
		ExplodeSpikes(obj.rings[2], 90)
		obj.ringsDestroyed = true
	end
end

local size = { x = 64, y = 64 }
local offset = { x = -32, y = -32 }
local function Pulse(tile)
	eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, size, 0.1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, offset, 0.1, 0)
end

local function SpiralPattern(obj)
	local base = { x = 128, y = 0 }
	local bendStep = -5
	local maxBend = 90
	local bend = maxBend
	local angle = 0
	local store = { }
	local function Halt(ball)
		if not ball.destroyed then
			eapi.SetAcc(ball.body, vector.null)
		end
	end
	local function Redirect(ball)
		if not ball.destroyed then
			local pos = actor.GetPos(ball)
			local len = vector.Length(pos)
			local scale = 0.25 * (len + 200)
			local vel = vector.Normalize(pos, scale)
			eapi.RandomSeed(math.abs(pos.x * pos.y + pos.x))
			local angle = util.Random() > 0.5 and 90 or -90
			eapi.SetAcc(ball.body, vector.Rotate(vel, angle))
			util.Delay(ball.body, 1, Halt, ball)
		end
	end
	local function Stop(ball)
		if not ball.destroyed then 
			local pos = actor.GetPos(ball)
			eapi.RandomSeed(math.abs(pos.x * pos.y + pos.x))
			local vel = vector.Rnd(vector.null, 500)
			util.Delay(ball.body, 1, Redirect, ball)
			eapi.SetAcc(ball.body, vector.Scale(vel, -1))
			eapi.SetVel(ball.body, vel)
		end
	end
	local function Emit()
		local basePos = actor.GetPos(obj)
		local function Bullet(Fn, i, dir)
			local arm = vector.Rotate(base, dir * (angle + i))
			local vel = vector.Rotate(arm, dir * bend)
			local pos = vector.Add(basePos, arm)
			local ball = Fn(pos, vector.Normalize(vel, 500))
			eapi.SetAcc(ball.body, vector.Normalize(vel, -500))
			store[ball] = ball
			Pulse(ball.tile)
		end
		for i = 0, 359, 45 do
			Bullet(bullet.Cyan, i, 1)
			Bullet(bullet.Magenta, i, -1)
		end
		if angle < 40 then
			eapi.AddTimer(obj.bullet, 0.05, Emit)
		else
			util.Map(Stop, store)
		end
		angle = angle + 1
		bend = bend + bendStep
		if math.abs(bend) >= maxBend then
			bendStep = -bendStep
		end
	end
	Emit()
	ShieldHurt(obj)
	util.Delay(obj.bullet, 11, analDestructor.Spin, obj)
end

local function CrossRayPattern(obj)
	local Bullet = { [1] = bullet.CyanLong, [-1] = bullet.MagentaLong }
	local base = { x = 128, y = 0 }
	local angleOffset = 0
	local bendBase = 70
	local count = 5
	local function Emit()
		local bend = bendBase
		local basePos = actor.GetPos(obj)
		if eapi.GetVel(obj.body).x < 0 then
			eapi.AddTimer(obj.bullet, 0.95, Emit)
			return
		end
		for angle = angleOffset, angleOffset + 359, 10 do
			local sign = util.Sign(bend)
			local arm = vector.Rotate(base, angle)
			local pos = vector.Add(basePos, arm)
			local vel = vector.Rotate(arm, bend)
			bullet.Line(pos, vel, Bullet[sign], 5, 0.9, 1.1)
			bend = -bend
		end
		if count > 0 then
			eapi.AddTimer(obj.bullet, 0.5, Emit)
			angleOffset = angleOffset + 3
			bendBase = bendBase + 10
			count = count - 1
		else
			util.Delay(obj.bullet, 3.0, SpiralPattern, obj)
		end
	end
	Emit()
end

local function ShieldSpinPattern(obj)
	local arc = 60
	local step = 2
	local count = 0
	local baseAngle = 0
	local spinBase = { x = 128, y = 0 }
	local delay = 0.05

	local BulletFns = {
		[-1] = bullet.MagentaBig,
		 [0] = bullet.Cyan,
		 [1] = bullet.CyanBig,
		 [2] = bullet.Magenta }

	local function Spin()
		local flip = 0
		local pos = actor.GetPos(obj)
		local function Emit(angle)
			local sign = util.Sign(step)
			local arm = vector.Rotate(spinBase, angle)
			local vel = vector.Normalize(arm, 200)
			BulletFns[sign + flip](vector.Add(pos, arm), vel)
			flip = 1 - flip
		end
		local startAngle = baseAngle + 180
		for angleStep = startAngle, startAngle + 359, arc * 2 do
			for angle = 0, arc, 4 do
				Emit(angle + angleStep)
			end
		end
		baseAngle = baseAngle + step
		if baseAngle >= 60 or baseAngle <= 0 then
			if baseAngle <= 0 then
				count = count + 1
				arc = 180 / (count + 3)
				delay = 0.2
			end
			step = -step
		end
		if count < 3 then
			eapi.AddTimer(obj.bullet, delay, Spin)
			delay = 0.05
		else
			util.Delay(obj.bullet, 1.5, CrossRayPattern, obj)
		end
	end
	Spin()
end

local function Hit(victim, shoot)
	if victim.healthObj then
		victim.healthObj.Dec(shoot.damage)
	end
end

local function ScreechSound()
	local function Screech()
		eapi.PlaySound(gameWorld, "sound/screech.ogg")
	end
	for delay = 0.0, 0.5, 0.1 do
		eapi.AddTimer(staticBody, delay, Screech)
	end
end

local function BallFlash()
	eapi.AddTimer(staticBody, 1.2, Flash)
	Flash()
end

local ballAngle = 30
local ballSPR = 2 -- (S)econds (P)er (R)evolution
local function GetBallAngle(obj)
	local time = eapi.GetTime(obj.body)
	if time < 1 then 
		return ballAngle * time
	elseif time < 2 then
		return ballAngle
	else
		return ballAngle - (360 / ballSPR) * (time - 2)
	end
end

local function BallHalf(parent, pos, dir)
	local init = {
		pos = pos,
		dir = dir,
		sprite = halves[dir],
		offset = { x = -128, y = -64 + dir * 64 },
		z = 6,
	}
	local delay = 0.06
	local base = { x = 128, y = 0 }
	local obj = actor.Create(init)
	local angle = vector.Radians(ballAngle)
	eapi.AnimateAngle(obj.tile, eapi.ANIM_CLAMP, vector.null, angle, 1, 0)
	local function Explode()
		util.AnimateRotation(obj.tile, -ballSPR, ballAngle)
		local vel = vector.Scale({ x = 400, y = 0 }, dir)
		eapi.SetVel(obj.body, vector.Rotate(vel, ballAngle + 90))
		eapi.SetAcc(obj.body, vector.null)
		actor.DelayedDelete(obj, 1.2)
		if dir > 0 then 
			analDestructor.Cage(pos, parent.healthObj)
			BallFlash()
		end
		delay = 0.02
	end
	local function Split()
		local vel = vector.Scale({ x = 100, y = 0 }, dir)
		eapi.SetAcc(obj.body, vector.Rotate(vel, ballAngle))
		eapi.AddTimer(obj.body, 1, Explode)
		if dir > 0 then ScreechSound() end
		delay = 0.04
	end
	eapi.AddTimer(obj.body, 1, Split)
	local function BoomBoom()
		local offset = vector.Rotate(base, GetBallAngle(obj))
		offset = vector.Scale(offset, 2.0 * util.Random() - 1.0)
		explode.Simple(offset, obj.body, obj.z + 0.1)
		eapi.AddTimer(obj.body, delay, BoomBoom)
	end
	BoomBoom()
end

local function ExplodeInHalves(obj)
	local pos = actor.GetPos(obj)
	for i = -1, 1, 2 do BallHalf(obj, pos, i) end
end

local function DestroyShield(obj)
	obj.finish = true
	eapi.SetAcc(obj.body, vector.null)
	eapi.SetVel(obj.body, vector.null)
	ExplodeInHalves(obj)
	eapi.Destroy(obj.bullet)
	bullet.Sweep()
	actor.Delete(obj)
end

local gearFile = { "image/gears.png", filter = true }
local gearImg = eapi.ChopImage(gearFile, { 320, 112 })

local function AddGears(index, obj, sprite, offset, speed, fps, z, angle)
	local body = eapi.NewBody(obj.body, vector.null)
	local tile = eapi.NewTile(body, offset, nil, sprite, obj.z + z)
	obj.rings[index] = { tile = tile, body = body, speed = speed }
	eapi.Animate(tile, eapi.ANIM_LOOP, fps, 0)
	util.AnimateRotation(tile, speed, angle)
	obj.rings[index].angle = angle
end

local frame = { { 1, 65 }, { 254, 62 } }
local beamImg = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)

local function FloatBeam(obj)
	local flip = -1
	local life = 0.7
	local z = obj.z - 1
	local size = { x = 128, y = 64 }
	local offset = { x = -64, y = -32 }
	local size2 = { x = 512, y = 64 }
	local offset2 = { x = -256, y = -32 }
	local color = { r = 0.6, g = 0.8, b = 0.8, a = 0.1 }
	local vel = { [-1] = { x = 0, y = -400 }, [1] = { x = 0, y = 400 } }
	local function Emit()
		local pos = actor.GetPos(obj)
		local body = eapi.NewBody(gameWorld, pos)
		local tile = eapi.NewTile(body, offset, size, beamImg, z)
		eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size2, life, 0)
		eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset2, life, 0)
		util.DelayedDestroy(body, life)
		eapi.AddTimer(obj.body, 0.01, Emit)
		eapi.SetVel(body, vel[flip])
		eapi.SetColor(tile, color)
		flip = -flip
	end
	Emit()
end

local function Shield(healthObj)
	local init = {
		sprite = shieldImg,
		hDelay = 1, vDelay = 0.5,
		initVel = { x = 0, y = -100 },
		offset = { y = -128, x = -128 },
		pos = { x = 400 + 128, y = 0 },
		health = shieldHealth,
		prevHealth = shieldHealth,
		healthObj = healthObj,
		Shoot = DestroyShield,
		class = "Mob",
		rings = { },
		Hit = Hit,
		z = 5,
	}
	local obj = actor.Create(init)
	obj.bullet = eapi.NewBody(obj.body, vector.null)
	util.Delay(obj.bullet, 2, ShieldSpinPattern, obj)
	util.Delay(obj.body, 1, LoopMovement, obj)
	util.Delay(obj.body, 1, ShieldShape, obj)
	actor.Rush(obj, 0.99, 600, 0)
	AddGears(1, obj, gearImg, { x = -160, y = -112 }, 12, 32, 0.5, 0)
	AddGears(2, obj, gearImg, { x = -160, y = -112 }, 12, 32, 0.5, 180)
	FloatBeam(obj)
end

local cageFFile = { "image/cage-front.png", filter = true }
local cageFImg = eapi.ChopImage(cageFFile, { 256, 256 })

local cageBFile = { "image/cage-back.png", filter = true }
local cageBImg = eapi.ChopImage(cageBFile, { 256, 256 })

local glassBallFile = { "image/glass-ball.png", filter = true }
local glassBallImg = eapi.ChopImage(glassBallFile, { 128, 128 })

local function Navigate(obj, src, dst)
	local base = vector.Sub(dst, src)
	local function SetAcc(scale)
		eapi.SetAcc(obj.body, vector.Scale(base, scale))
	end
	eapi.AddTimer(obj.body, 0.5, util.Closure(SetAcc, -4));
	SetAcc(4)
end

local function AnimateCage(tile)
	util.AnimateRotation(tile, 16)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, 0)
end

local function AddGlassBall(obj)
	local z = obj.z - 1
	local offset = { x = -64, y = -64 }
	obj.ballBody = eapi.NewBody(obj.body, { x = 16, y = 0 })
	eapi.SetStepC(obj.ballBody, eapi.STEPFUNC_ROT, -math.pi)
	local tile = eapi.NewTile(obj.ballBody, offset, nil, glassBallImg, z)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, 0)
end

local fractalTable = { }
local function GenerateFractal()
	local index = 1
	local startPos = { x = 100, y = -150 }
	local sizes = { 4, 8, 5, 3, 2, 1, 1, 0 }
	local function Generate(pos, dir, depth)
		local count = sizes[depth]
		local scale = 1.0 - 0.06 * depth
		if count > 0 then
			for i = 1, count, 1 do
				local obj = { pos = pos, scale = scale }
				fractalTable[index] = obj
				pos = vector.Add(pos, dir)
				index = index + 1
			end
			Generate(pos, vector.Rotate(dir, 30), depth + 1)
			Generate(pos, vector.Rotate(dir, -30), depth + 1)
		end
	end
	Generate(startPos, { x = -10, y = 10 }, 1)
	local function DistanceFromStart(obj)
		return vector.Length(vector.Sub(obj.pos, startPos))
	end
	local function Comp(a, b)
		return DistanceFromStart(a) > DistanceFromStart(b)
	end
	table.sort(fractalTable, Comp)
end
GenerateFractal()

local function EmitFractal(obj)
	local time = 3
	local index = 1
	local arm = { x = -96, y = 0 }
	local angle = util.fibonacci
	local function Emit()
		local pos = actor.GetPos(obj)
		local info = fractalTable[index]
		pos = vector.Add(pos, vector.Rotate(arm, angle))
		local vel = vector.Sub(info.pos, pos)
		vel = vector.Scale(vel, 1 / time)
		bullet.MagentaCustom(pos, vel, info.scale)
		
		if index < #fractalTable then
			index = index + 1
			time = time - 0.01
			angle = angle - 10
			eapi.AddTimer(obj.bullet, 0.01, Emit)
		else
			util.Delay(obj.bullet, 5, analDestructor.Fixed, obj)
		end
	end
	Emit()
end

local function SweeperTail(projectile)
	local gamma = 0
	local step = 10
	local function Shoot()
		local pos = eapi.GetPos(projectile.body)
		local vel = eapi.GetVel(projectile.body)
		bullet.CyanLong(pos, vector.Rotate(vel, 180 - gamma))
		if pos.x > -400 and math.abs(pos.y) < 240 then
			eapi.AddTimer(projectile.body, 0.02, Shoot)
			gamma = gamma + step
		end
		if gamma > 90 or gamma < -90 then
			step = -step
		end
	end
	Shoot()
end

local function SweepArcs(obj)
	local counter = 7
	local function Emit()
		local pos = actor.GetPos(obj)
		local aim = bullet.HomingVector(player.Pos(), pos, 200)
		aim = vector.Rotate(aim, 30 * (counter % 3 - 1))
		SweeperTail(bullet.MagentaSpin(pos, aim))		
		if counter > 0 then
			eapi.AddTimer(obj.bullet, 0.5, Emit)
			counter = counter - 1
		else
			util.Delay(obj.bullet, 5, EmitFractal, obj)
		end
	end
	Emit()
end

local function FixedCircle(obj)
	local flip = 90
	local counter = 50
	local Fn = bullet.MagentaLong
	local center = { x = 256, y = 0 }
	local armBase = { x = 96, y = 0 }
	local function Emit()
		local basePos = actor.GetPos(obj)
		for angle = 0, 359, 15 do
			local arm = vector.Rotate(armBase, angle)
			local pos = vector.Add(basePos, arm)
			local offset = vector.Normalize(arm, 512)
			local aim = vector.Add(center, offset)
			bullet.AimLine(pos, aim, Fn, 5, 0.9, 1.1)
		end	
		if counter < 45 and counter > 10 and counter % 4 == 0 then
			for angle = 0, 359, 10 do
				local arm = vector.Rotate(armBase, angle)
				local pos = vector.Add(basePos, arm)
				local vel = vector.Normalize(arm, 250)
				vel = vector.Rotate(vel, flip)
				bullet.CyanSpin(pos, vel)
			end
			flip = -flip
		end
		if counter > 0 then
			eapi.AddTimer(obj.bullet, 0.1, Emit)
			counter = counter - 1
		else
			util.Delay(obj.bullet, 2, SweepArcs, obj)
		end
	end
	Emit()
end

local function UnlinkBall(obj)
	eapi.Unlink(obj.ballBody)
	eapi.SetStepC(obj.ballBody, eapi.STEPFUNC_STD)
	eapi.SetPos(obj.ballBody, vector.null)
	eapi.SetVel(obj.ballBody, vector.null)
	eapi.SetAcc(obj.ballBody, vector.null)
end

local function CageCleanup(obj)
	analDestructor.Star(obj.healthObj)
	util.Map(eapi.Destroy, obj.tiles)
	eapi.Destroy(obj.ballBody)
	Flash()
end

local function CageAnim(i, obj, pos, size)
	eapi.AnimatePos(obj.tiles[i], eapi.ANIM_CLAMP, pos, 1.2, 0)
	eapi.AnimateSize(obj.tiles[i], eapi.ANIM_CLAMP, size, 1.2, 0)
	eapi.AnimateColor(obj.tiles[i], eapi.ANIM_CLAMP, util.invisible, 1.2, 0)
end

local function CageExplode(obj)
	obj.explosionRow = true
	eapi.SetVel(obj.body, vector.null)
	eapi.SetAcc(obj.body, vector.null)
	CageAnim(1, obj, { x = -2048, y = -2048 }, { x = 4096, y = 4096 })
	CageAnim(2, obj, { x = -1, y = -1 }, { x = 2, y = 2 })
	util.Delay(obj.body, 1.4, CageCleanup, obj)
	UnlinkBall(obj)
	Flash()
end

local function CageEnd(obj)
	obj.finish = true
	actor.DeleteShape(obj)
	eapi.Destroy(obj.bullet)
	local pos = actor.GetPos(obj)	
	eapi.SetVel(obj.body, vector.Scale(pos, -2))
	eapi.SetAcc(obj.body, vector.Scale(pos, 2))
	util.Delay(obj.body, 1, CageExplode, obj)

	local arm = { x = 96, y = 0 }
	local function RowOfExplosions()
		local offset = vector.Rotate(arm, 360 * util.Random())
		local amount = 1 - util.Random() * util.Random()
		offset = vector.Scale(offset, amount)
		explode.Simple(offset, obj.body, 0.5)
		if not obj.explosionRow then
			eapi.AddTimer(obj.body, 0.02, RowOfExplosions)
		end
	end
	RowOfExplosions()
	bullet.Sweep()
end

local function Cage(pos, healthObj)
	pos = pos or { x = 128, y = 0 }
	local init = {
		pos = pos,
		sprite = cageFImg,
		hDelay = 0.5, vDelay = 1,
		initVel = { x = -100, y = -200 },
		offset = { y = -128, x = -128 },
		health = cageHealth,
		prevHealth = cageHealth,
		healthObj = healthObj,
		Shoot = CageEnd,
		class = "Mob",
		Hit = Hit,
		z = 0,
	}	
	local obj = actor.Create(init)
	obj.tiles = { [1] = obj.tile }
	obj.bullet = eapi.NewBody(obj.body, vector.null)
	obj.tiles[2] = eapi.NewTile(obj.body, obj.offset, nil, cageBImg, -2)
	util.Delay(obj.body, 1, ShieldShape, obj, 88)
	util.Delay(obj.body, 1, LoopMovement, obj)
	util.Delay(obj.bullet, 2, FixedCircle, obj)
	Navigate(obj, pos, { x = 256, y = 0 })
	util.Map(AnimateCage, obj.tiles)
	AddGlassBall(obj)
end

local frame = { { 0, 0 }, { 128, 128 } }
local starFile = { "image/death-star.png", filter = true }
local starImg = eapi.NewSpriteList(starFile, frame)

local glowFile = { "image/star-blur.png", filter = true }
local glowImg = eapi.NewSpriteList(glowFile, frame)

local function AddStarGlow(obj)
	local z = obj.z + 1
	local color = { r = 1 , g = 1, b = 1, a = 0.2 }
	obj.glow = eapi.NewTile(obj.body, obj.offset, nil, glowImg, z)
	eapi.SetColor(obj.glow, { r = 1 , g = 1, b = 1, a = 0.6 })
	eapi.AnimateColor(obj.glow, eapi.ANIM_REVERSE_LOOP, color, 0.2, 0)
end

local function AddStarRings(obj)
	local z = obj.z - 1
	local offset = { x = -15, y = -15 }
	local dstPos = { x = -1024, y = -1024 }
	local dstSize = { x = 2048, y = 2048 }
	local color = { r = 1, g = 1, b = 1, a = 0.2 }
	for i = 1, 20, 1 do
		local delay = -0.1 * i 
		local tile = eapi.NewTile(obj.body, offset, nil, util.ring, z)
		eapi.AnimateSize(tile, eapi.ANIM_LOOP, dstSize, 2, delay)
		eapi.AnimatePos(tile, eapi.ANIM_LOOP, dstPos, 2, delay)
		eapi.SetColor(tile, color)
		obj.rings[i] = tile
	end
end

local function PulseStar(obj)
	local dstPos = { x = -68, y = -68 }
	local dstSize = { x = 136, y = 136 }
	eapi.AnimateSize(obj.tile, eapi.ANIM_REVERSE_LOOP, dstSize, 0.1, 0)
	eapi.AnimatePos(obj.tile, eapi.ANIM_REVERSE_LOOP, dstPos, 0.1, 0)
end

local shineFile = { "image/shine.png", filter = true }
local shineImg = eapi.ChopImage(shineFile, { 256, 256 })

local function StarShine(obj)
	local delay = 0
	local alpha = 1.0
	local z = obj.z - 0.5
	local offset = { x = -128, y = -128 }
	for speed = -16, 16, 32 do
		local tile = eapi.NewTile(obj.body, offset, nil, shineImg, z)
		eapi.Animate(tile, eapi.ANIM_REVERSE_LOOP, 32, delay)
		util.AnimateRotation(tile, speed)
		eapi.SetColor(tile, { r = 1, g = 1, b = 1, a = alpha })
		eapi.FlipX(tile, speed > 0)
		obj.shine[tile] = tile
		delay = delay + 0.5
		alpha = alpha - 0.2
		z = z + 0.1
	end
end

local function GetNoise(x, y, z)
	return eapi.Fractal(0.01 * x, 0.01 * y, 0.01 * z, 4, 2)
end

local maxVal = 0.6
local threshold = 0.45
local function NoiseBullet(pos, size) 
	if size > threshold then
		local range = (size - threshold) / (maxVal - threshold)
		local scale = 0.5 + 0.5 * range
		bullet.z_epsilon = 0.001 * scale
		bullet.CyanCustom(pos, vector.Normalize(pos, 150), scale)
	end
end

local function Perlin(obj)
	local delta = 0
	local progress = 0
	local arm = { x = 48, y = 0 }
	local function Emit()
		for angle = 0, 359, 10 do
			local pos = vector.Rotate(arm, angle + delta)
			local size = GetNoise(pos.x, pos.y, progress)
			NoiseBullet(pos, size)
		end
		eapi.AddTimer(obj.bullet, 0.02, Emit)
		delta = (delta + util.fibonacci) % 360
		progress = progress + 1
	end
	Emit()
end

local function HeavyRain(obj)
	local x = 0
	local angle = 0
	local vel = { x = 0, y = -150 }
	local function Emit()
		bullet.z_epsilon = 0.5
		local pos = { x = x - 400, y = 256 }
		bullet.MagentaLong(pos, vector.Rotate(vel, -angle))
		eapi.AddTimer(obj.bullet, 0.1, Emit)
		angle = (angle == 0) and 30 or 0
		x = (x + 800 * util.golden) % 800
	end
	Emit()
end

local function ShrinkShine(tile)
	local size = { x = 2, y = 2 }
	local pos = { x = -1, y = -1 }
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, 0.5, 0)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size, 1.0, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, pos, 1.0, 0)
end

local raysSize = { x = 1536, y = 1536 }
local raysOffset = { x = -768, y = -768 }
local raysFile = { "image/rays.png", filter = true }
local raysImg = eapi.ChopImage(raysFile, { 1024, 1024 })

local function ExplodeRays(obj, dir)
	local z = obj.z + 0.5
	local tile = eapi.NewTile(obj.body, raysOffset, raysSize, raysImg, z)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, 0.25, 0)
	eapi.AddTimer(obj.body, 0.25, function() eapi.Destroy(tile) end)
	util.AnimateRotation(tile, dir)
end

local function Victory()
	menu.CompletionMessage(txt.victory)
end

local function FadeOut()
	local tile = util.WhiteScreen(150)
	local black = { r = 0, g = 0, b = 0, a = 1 }
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0 })
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, black, 2, 0)
end

local function Outro()
	util.Goto("outro")
end

local function ThisIsIt()
	util.DoEventsRelative( { { 0.0, Victory },
				 { 2.0, player.DisableAllInput },
				 { 1.0, FadeOut },
				 { 3.0, Outro }, })
end

local function SlowWhite(obj)
	local count = 3
	actor.Delete(obj)
	local tile = util.WhiteScreen(150)
	local function Fade()
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, 2, 0)
	end
	eapi.AddTimer(staticBody, 1, Fade)
	eapi.AddTimer(staticBody, 4, ThisIsIt)
	eapi.FadeMusic(3.0)
	local function Boom()
		eapi.PlaySound(gameWorld, "sound/boom.ogg")
		if count > 0 then
			eapi.AddTimer(staticBody, 0.05, Boom)
			count = count - 1
		end
	end
	Boom()
end

local function FinalRays(obj)
	Flash()
	eapi.SetVel(obj.body, vector.null)
	eapi.SetAcc(obj.body, vector.null)
	ExplodeRays(obj, obj.raySpeed * (1 - (2 * (obj.rayCount % 2))))
	local Fn = obj.rayCount < 10 and FinalRays or SlowWhite
	util.Delay(obj.body, 0.25, Fn, obj)
	obj.raySpeed = obj.raySpeed - 0.5
	obj.rayCount = obj.rayCount + 1
end

local function PingPong(obj)
	Flash()
	local vel = { x = 64, y = 440 * obj.dir }
	eapi.SetVel(obj.body, vector.Scale(vel, obj.bounce))
	util.AnimateRotation(obj.tile, obj.dir)
	if obj.pongs < 5 then
		util.Delay(obj.body, 1 / obj.bounce, PingPong, obj)
	else
		eapi.SetAcc(obj.body, vector.Scale(vel, -4))
		util.Delay(obj.body, 0.5 / obj.bounce, FinalRays, obj)
		obj.raySpeed = 6
		obj.rayCount = 0
	end
	if obj.pongs == 1 then
		obj.bounce = 2
	end
	obj.pongs = obj.pongs + 1
	obj.dir = -obj.dir
end

local function StarTrail(obj)
	obj.dir = -1
	obj.pongs = 0
	obj.bounce = 1
	eapi.SetVel(obj.body, { x = -256, y = 480 })
	util.Delay(obj.body, 0.5, PingPong, obj)
	local function Emit()
		local pos = vector.Rnd(actor.GetPos(obj), 48)
		explode.Simple(pos, staticBody, obj.z + 1)
		eapi.AddTimer(obj.body, 0.01, Emit)
	end
	Emit()
end

local function EndStar(obj)
	Flash()
	bullet.Sweep()
	actor.DeleteShape(obj)
	eapi.Destroy(obj.glow)
	eapi.Destroy(obj.bullet)
	util.AnimateRotation(obj.tile, 1)
	util.Map(eapi.Destroy, obj.rings)
	util.Map(ShrinkShine, obj.shine)
	StarTrail(obj)
end

local function HitStar(victim, shoot)
	Hit(victim, shoot)
	if victim.health < 0.25 * starHealth and not victim.heavyRain then
		victim.heavyRain = true
		HeavyRain(victim)
	end
end

local function Star(healthObj)
	local init = {
		pos = vector.null,
		sprite = starImg,
		offset = { y = -64, x = -64 },
		health = starHealth,
		prevHealth = starHealth,
		healthObj = healthObj,
		bb = { b = -28, l = -24, r = 24, t = 16 },
		Shoot = EndStar,
		class = "Mob",
		shine = { },
		rings = { },
		Hit = HitStar,
		z = -5,
	}	
	local obj = actor.Create(init)
	actor.MakeShape(obj, { b = 16, l = -8, r = 8, t = 32 })
	obj.bullet = eapi.NewBody(obj.body, vector.null)
	util.Delay(obj.bullet, 1, Perlin, obj)
	AddStarGlow(obj)
	AddStarRings(obj)
	PulseStar(obj)
	StarShine(obj)
end

analDestructor = {
	Star = Star,
	Cage = Cage,
	Launch = Launch,
	Shield = Shield,
	Spin = ShieldSpinPattern,
	Fixed = FixedCircle,
}
return analDestructor
