local gaspRows  = 6
local gaspCount = 10

local lastHealth = 600
local legOffHealth = lastHealth + 800
local bossHealth = legOffHealth + 1300

local bossImg = eapi.ChopImage("image/boss1.png", { 256, 480 })
local legFile = { "image/boss1-leg.png", filter = true }
local legImg = eapi.ChopImage(legFile, { 256, 256 })

local frame = { { 1, 33 }, { 30, 30 } }
local circularFile = { "image/misc.png", filter = true }
local circularImg = eapi.NewSpriteList(circularFile, frame)

local firstStageTimer = true
local secondStageTimer = true
local thirdStageTimer = true
local function FirstStageTimer(interval, FN)
	local function Wrapper()
		if firstStageTimer then FN() end
	end
	eapi.AddTimer(staticBody, interval, Wrapper)
end

local function EmitStars(obj, count, x, y)
	for i = 1, count, 1 do
		local pos = actor.GetPos(obj)
		pos = vector.Offset(pos, x or 128, y or 30)
		powerup.Star(pos, vector.Rnd({ x = 100, y = 50 }, 300))
	end
end

local function BlowLegOff(obj)
	local img = player.sparkAnim
	local offset = player.sparkOffset
	local function Explode()
		EmitStars(obj, 10, 192, -64)
		for i = 0, 2, 1 do 
			local pos = { x = 224 - i * 40, y = -64 * (i + 1) }
			explode.Area(vector.Rnd(pos, 32), obj.body, 10, 32, .05)
		end
	end
	local function EmitSpark(sparkOffset)
		local pos = vector.Add(actor.GetPos(obj), sparkOffset)
		local body = eapi.NewBody(gameWorld, pos)
		local tile = eapi.NewTile(body, offset, nil, img, -2)
		eapi.SetVel(body, { x = -500, y = 0 })
		eapi.SetAcc(body, { x = 0, y = 500 * util.Random() })
		eapi.Animate(tile, eapi.ANIM_CLAMP, 64 - 16 * util.Random(), 0)
		util.DelayedDestroy(body, 0.5)
		util.RandomRotateTile(tile)
	end
	local function Sparks()
		EmitSpark({ x = 200, y = -148 })
		EmitSpark({ x = 136, y = -112 })
		eapi.AddTimer(obj.body, 0.02, Sparks)
	end
	local function CogShadow(i, pos, size, start)
		obj.leg[i] = eapi.NewTile(obj.body, pos, size, circularImg, -3)
		eapi.SetColor(obj.leg[i], util.Gray(0))
		local color = util.SetColorAlpha(util.Gray(0), 0.3)
		eapi.AnimateColor(obj.leg[i], eapi.ANIM_LOOP, color, 0.2, start)
	end
	local function HitGround()
		eapi.SetAcc(obj.body, vector.null)
		eapi.SetVel(obj.body, vector.null)
		CogShadow(1, { x = 136, y = -164 }, { x = 128, y = 32 }, 0)
		CogShadow(2, { x = 88, y = -124 }, { x = 96, y = 24 }, 0.1)
		eapi.AddTimer(obj.body, 1, obj.SecondStage)
		Sparks()
	end
	local function RemoveLegs()
		util.DestroyTable(obj.leg)
		eapi.SetAcc(obj.body, { x = 0, y = -1000 })
		eapi.AddTimer(obj.body, 0.35, HitGround)
		util.Map(parallax.Straight, parallax.level1)
	end
	eapi.AddTimer(obj.body, 1.5, RemoveLegs)
	util.Repeater(Explode, 3, 0.75, obj.body)
	obj.firstStage.stop = true
	firstStageTimer = false
	bullet.Sweep()
end

local function LastBreath(obj)
	EmitStars(obj, 30)
	eapi.AddTimer(obj.body, 1, obj.ThirdStage)
	explode.Area({ x = 176, y = 30 }, obj.body, 10, 128, 0.01)	
	secondStageTimer = false
	bullet.Sweep()
end

local function Hit(obj)
	powerup.ShowProgress(obj)
	if obj.prevHealth > legOffHealth and obj.health <= legOffHealth then
		BlowLegOff(obj)
	end
	if obj.prevHealth > lastHealth and obj.health <= lastHealth then
		LastBreath(obj)
	end
	obj.prevHealth = obj.health
end

local function StarWave(pos, duration)
	local interval = 0.05
	local height = 64
	local maxstep = 12
	local step = maxstep
	local substep = -1
	local function Emit()
		local newPos = vector.Offset(pos, 0, height)
		local vel = vector.Rnd({ x = -100, y = height }, 64)
		powerup.Star(newPos, vel)
		duration = duration - interval
		if duration > 0 then 
			eapi.AddTimer(staticBody, interval, Emit)
		end
		height = height + step
		step = step + substep
		if math.abs(step) > maxstep then
			substep = -substep
		end
	end
	Emit()
end

local function Shoot(obj)
	local step = 5
	local function Tremble()
		eapi.SetPos(obj.body, vector.Offset(actor.GetPos(obj), 0, step))
		eapi.AddTimer(obj.body, 0.02, Tremble)
		step = -step
	end
	Tremble()
		
	obj.bb = { l = 64, r = 256, b = -96, t = 128 }
	explode.Continuous(obj)
	
	local function Area(offset)
		return function()
			player.Flash()
			return explode.Area(offset, obj.body, 10, 64, 0.01)
		end
	end
	local function AllWhite()
		local tile = util.WhiteScreen(200)
		local color = { r = 1, g = 1, b = 1, a = 0 }
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, color, 2, 0)
		eapi.AddTimer(obj.body, 2.0, util.DestroyClosure(tile))
		eapi.Destroy(obj.tile)		
	end
	local function FinalExplode(offset)
		offset = vector.Rnd(offset, 16)
		local noSplinter = util.Random() > 0.5
		local body = explode.Simple(offset, obj.body, -0.5, noSplinter)
		eapi.SetAcc(body, vector.Rnd(vector.null, 128))		
		local probably = util.Random() > 0.8
		eapi.SetVel(body, { x = probably and -512 or 0, y = -64 })
	end
	local function TimedExplode(offset)
		local time = 0.1 * util.Random()
		local function FN() FinalExplode(offset) end
		eapi.AddTimer(staticBody, time, FN)
	end
	local function Full()
		local step = 144 / gaspRows
		for y = -gaspRows, gaspRows, 1 do
			for x = math.abs(y) * 1.4, 10, 1 do
				TimedExplode({ x = x * 24, y = y * step + 30 })
			end
		end
	end
	local function FullSeries()
		util.Repeater(Full, gaspCount, 2.5 / gaspCount, staticBody)
		StarWave({ x = 300, y = -100 }, 3)
	end
	util.DoEventsRelative({ { 0.0, Area({ x = 156, y =  60 }) },
				{ 0.5, Area({ x = 126, y =   0 }) },
				{ 0.5, Area({ x = 196, y =  30 }) },
				{ 0.5, Area({ x = 176, y = -60 }) },
				{ 0.5, Area({ x = 226, y =  90 }) },
				{ 0.5, Area({ x =  96, y =  50 }) },
				{ 0.5, AllWhite },
				{ 0.0, FullSeries },
				{ 2.6, function() actor.Delete(obj) end },
				{ 0.1, function() util.StopMusic(2.0) end },
				{ 3.5, menu.Success },
				{ 2.0, player.DisableAllInput },
				{ 0.5, player.LevelEnd },
				{ 2.5, util.Level(2) } })

	bullet.Sweep()
	actor.DeleteShape(obj)
	powerup.ShowProgress(obj)
	thirdStageTimer = false
end

local function CircleOfArcs(pos, angle)
	local function PrimitiveArc(pos, vel)
		bullet.Arc(pos, vel, bullet.Cyan, 7, 20)
	end	
	local vel = vector.Rotate(bullet.velocity, angle)
	bullet.FullArc(pos, vel, PrimitiveArc, 10)
end

local function ShiftedArcs(Pos, count)
	local add = 0
	local angle = -10
	local function EmitCircle()
		CircleOfArcs(Pos(), angle + add)
		count = count - 1
		angle = -angle
		add = add + 1
		if count > 0 then 
			FirstStageTimer(0.8, EmitCircle)
		end
	end
	EmitCircle()
	
end

local function SpinningSpirals(Pos, count, duration)
	local interval = 0.05
	local angle = 0
	local maxstep = 2.5
	local step = maxstep
	local substep = 0.1
	local function Turn()
		local vel = vector.Rotate(bullet.velocity, angle)
		bullet.FullArc(Pos(), vel, bullet.Magenta, count)
		duration = duration - interval
		if duration > 0 then 
			FirstStageTimer(interval, Turn)
		end
		angle = angle + step
		step = step + substep
		if math.abs(step) > maxstep then
			substep = -substep
		end
	end
	Turn()
end

local function Blades(Pos, amount, density)
	local maxDuration = 0.5
	local duration = maxDuration
	local interval = 0.05
	local angle = 0
	local step = 2

	local bulletFns = { }
	bulletFns[step] =  bullet.Magenta
	bulletFns[-step] =  bullet.Cyan

	local function Turn()
		local vel = vector.Rotate(bullet.velocity, angle)
		bullet.FullArc(Pos(), vel, bulletFns[step], density)
		duration = duration - interval
		angle = angle + step		
		if duration > 0 then
			FirstStageTimer(interval, Turn)
		elseif amount > 0 then
			step = -step
			amount = amount - 1
			duration = maxDuration
			angle = angle + 215 / density
			FirstStageTimer(interval, Turn)
		end
	end
	Turn()
end

local function Leaf(pos, vel, BulletFn, count, min, max)
	z_epsilon = 0
	local step = (max - min) / (count - 1)
	local sideVel = vector.Normalize({ x = -vel.y, y = vel.x })
	for i = 0, count, 1 do
		local q = (2 * i / count) - 1.0
		local vel = vector.Scale(vel, min + step * i)
		local amount = 0.5 - 0.5 * q * q
		local scaledSideVel = vector.Scale(sideVel, 10 * amount)
		BulletFn(pos, vector.Add(vel, scaledSideVel))
		BulletFn(pos, vector.Sub(vel, scaledSideVel))
	end
end

local function Flower(Pos)
	local angle = 0
	local interval = 0.1
	local function Burst(obj, inner)
		return function()
			if not obj.exploding then
				local pos = actor.GetPos(obj)
				local vel = vector.Normalize(obj.velocity, 80)
				Leaf(pos, vel, bullet.Magenta, 10, 0.8, 1.2)
				actor.Delete(inner)
				obj.Explode(obj)
			end
		end
	end
	local function Emit()
		if secondStageTimer then
			local dir = vector.Rotate({ x = 10, y = 0 }, angle)
			local pos = vector.Add(Pos(), dir)
			local gravity = vector.Scale(dir, -4)
			local vel = vector.Scale(dir, 8)
			local obj = bullet.CyanBig(pos, vel)
			local inner = bullet.Magenta(pos, vel)
			eapi.SetAcc(obj.body, gravity)
			eapi.SetAcc(inner.body, gravity)
			eapi.AddTimer(obj.body, 2, Burst(obj, inner))
			angle = angle + util.fibonacci
			eapi.AddTimer(staticBody, interval, Emit)
		end
	end
	Emit()
end

local function BlockedTunnels(Pos, count)
	local interval = 0.05
	local duration = 0.25
	local timeLeft = duration
	local vel = bullet.velocity

	local function Homing()
		return bullet.HomingVector(player.Pos(), Pos(), bullet.speed)
	end

	local function Chmok()
		local vel = Homing()
		for i = 1, 5, 1 do
			local pos = vector.Rnd(Pos(), 32)
			bullet.CyanSpin(pos, vector.Rnd(vel, 32))
		end
	end
	
	local function Emit()
		bullet.FullArc(Pos(), vel, bullet.Magenta, 15)
		timeLeft = timeLeft - interval

		if timeLeft > 0 then
			FirstStageTimer(interval, Emit)
		elseif count > 0 then
			FirstStageTimer(2 * duration, Emit)
			FirstStageTimer(1 * duration, Chmok)
			vel = vector.Rotate(vel, 12)
			timeLeft = duration
			count = count - 1
		end
	end
	Emit()
end

local function PanicPattern(Pos, obj)
	local interval = 0.5
	local counter = 1
	local function EmitBullets(pos)
		local angle = 0
		eapi.RandomSeed(counter)
		counter = counter + 1
		for i = 1, 20, 1 do
			angle = angle + util.Random(5, 30)
			local amount = 0.8 + 0.4 * util.Random()
			local baseVel = vector.Scale(bullet.velocity, amount)
			local vel = vector.Rotate(baseVel, angle)
			bullet.Magenta(pos, vel)
		end
	end
	local function Emit()
		if thirdStageTimer then
			local pos = Pos()
			EmitBullets(pos)
			local offset = vector.Rnd({ x = 160, y = 30 }, 192)
			explode.Simple(offset, obj.body, obj.z + 0.1)
			eapi.AddTimer(obj.body, interval, Emit)
		end
	end
	Emit()
end

local function Start()
	local init = { class = "Mob",
		       sprite = bossImg,
		       health = bossHealth,
		       maxHealth = bossHealth,
		       prevHealth = bossHealth,
		       offset = { x = 0, y = -240 },
		       pos = { x = 400, y = 0 },
		       Shoot = Shoot,
		       jerk = 16,
		       Hit = Hit,
		       leg = { },
		       z = -1, }
	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 32, 0)

	local function AddShape()
		actor.MakeShape(obj, { b = 4, t = 52, l = 32, r = 64 })
		actor.MakeShape(obj, { b = -128, t = -80, l = 192, r = 256 })
		actor.MakeShape(obj, { b = 128, t = 152, l = 160, r = 256 })
		actor.MakeShape(obj, { b = -56, t = 112, l = 96, r = 128 })
		actor.MakeShape(obj, { b = -40, t = 96, l = 64, r = 96 })
		actor.MakeShape(obj, { b = -80, t = 128, l = 128, r = 256 })
	end
	eapi.AddTimer(staticBody, 0.5, AddShape)
	actor.Rush(obj, 1.0, 510, 0)

	local function LegTile(offset, i, z, animOffset, size) 
		obj.leg[i] = eapi.NewTile(obj.body, offset, size, legImg, z)
		eapi.Animate(obj.leg[i], eapi.ANIM_LOOP, -32, animOffset)
	end
	LegTile({ x = 52, y = -244 }, 1, -0.9, 0.0)
	LegTile({ x = 24, y = -184 }, 2, -1.1, 0.25, { x = 192, y = 192 })
	eapi.SetColor(obj.leg[2], util.Gray(0.6))

	powerup.ShowProgress(obj)

	local function ShootPos()
		return vector.Offset(actor.GetPos(obj), 8, 30)
	end
	local function SevenShiftedArcs()
		ShiftedArcs(ShootPos, 7)
	end
	local function NineSpinningSpirals()
		SpinningSpirals(ShootPos, 9, 5)
	end
	local function SevenBlockedTunnels()
		BlockedTunnels(ShootPos, 7)
	end
	local function SevenBlades()
		Blades(ShootPos, 7, 7)
	end
	obj.SecondStage = function()
		Flower(ShootPos)
	end
	obj.ThirdStage = function()
		PanicPattern(ShootPos, obj)
	end
	local function FirstStage()
		obj.firstStage = util.DoEventsRelative(
			{ { 2.0, SevenShiftedArcs },
			  { 7.0, NineSpinningSpirals }, 
			  { 7.0, SevenBlockedTunnels }, 
			  { 7.5, SevenBlades },
			  { 5.0, FirstStage }, })
	end
	FirstStage()

	util.Map(parallax.Reverse, parallax.level1)
end

local frame = { { 1, 1 }, { 254, 478 } }
local bossFile = { "image/boss1.png", filter = true }
local filteredBossImg = eapi.NewSpriteList(bossFile, frame)

local function Tow()
	local body = eapi.NewBody(gameWorld, { x = -530, y = 24 })
	eapi.SetVel(body, { x = 20, y = 0 })
	eapi.SetAcc(body, { x = 40, y = 0 })

	local offset = { x = -32, y = -60 }
	local size = { x = 64, y = 120 }
	local boss = eapi.NewTile(body, offset, size, filteredBossImg, -35)
	util.RotateTile(boss, -45)
	eapi.SetColor(boss, util.Gray(0.5))

	local function Cargo(pos)
		local img = cargo.topImg
		local size = { x = 48, y = 16 }		
		local tile = eapi.NewTile(body, pos, size, img, -35)
		eapi.Animate(tile, eapi.ANIM_LOOP, 16, util.Random())
		eapi.SetColor(tile, util.Gray(0.75))
		eapi.FlipX(tile, true)

		local img = player.flame
		local size = { x = 48, y = 48 }
		offset = vector.Offset(pos, -32, -16)
		local flame = eapi.NewTile(body, offset, size, img, -35.1)
		eapi.Animate(flame, eapi.ANIM_LOOP, 32, util.Random())
		eapi.SetColor(flame, util.Gray(0.5))

		local i = 0
		local z = -35.2
		local offset = vector.null
		local img = bullet.cyanImg
		local target = vector.Offset(pos, 16, 0)
		local step = vector.Normalize(target, 4)		
		while offset.x < target.x do
			local size = { x = 8, y = 8 }
			offset = vector.Add(offset, step)
			local tile = eapi.NewTile(body, offset, size, img, z)
			eapi.Animate(tile, eapi.ANIM_LOOP, -32, i)
			eapi.SetColor(tile, util.Gray(0.5))
			i = i + 0.05
		end
	end
	Cargo({ x = 32, y = 64 })
	Cargo({ x = 80, y = 40 })	

	util.DelayedDestroy(body, 7.0)
end

bulletPooper = {
	Start = Start,
	Tow = Tow,
}
return bulletPooper
