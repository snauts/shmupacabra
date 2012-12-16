local bossHealth = 2500

local cannonFile = { "image/boss2-cannon.png", filter = true }
local cannonImg = eapi.ChopImage(cannonFile, { 64, 64 })

local function Tree(cannon, Pos)
	local Bullet = nil
	local step = 0.02
	local value = 0.0
	local interval = 0.5
	local function Split(obj)
		if obj.level < 8 and not obj.exploding then
			local vel = eapi.GetVel(obj.body)
			local pos = eapi.GetPos(obj.body)
			local scale = obj.spriteSize.x / 64.0
			local angle = 60 * math.pow(scale, 4)
			Bullet(pos, vector.Rotate(vel, angle), 
			       0.85 * scale, 2 * obj.level)
			Bullet(pos, vector.Rotate(vel, -angle),
			       0.85 * scale, 2 * obj.level + 1)
			actor.Delete(obj)
		end
	end
	local function Emit()
		if cannon.stopUpdate then return end
		eapi.AddTimer(cannon.body, interval, Emit)
		Bullet(Pos(), bullet.HomingVector(player.Pos(), Pos()), 1, 1)
		local projectile = bullet.Cyan(Pos(), vector.null)
		if value <= -1 or value >= 1 then step = -step end
		interval = math.max(0.05, interval - 0.05)
		projectile.Explode(projectile)
		value = value + step
	end
	Bullet = function(pos, vel, scale, level)
		vel = vector.Rotate(vel, 13 * value)
		local obj = bullet.CyanCustom(pos, vel, scale)
		util.Delay(obj.body, 0.5 * scale, Split, obj)
		obj.level = level
	end
	Emit()
end

local frame = { { 1, 1 }, { 254, 478 } }
local scaleFile = { "image/boss2-wings.png", filter = true }
local scaleImg = eapi.NewSpriteList(scaleFile, frame)

local function Scale(pos, vel, z, scale, animScale, lifetime)
	local originalOffset = { x = -127, y = -239 }
	local originalSize = { x = 254, y = 478 }
	local offset = vector.Scale(originalOffset, scale)
	local size = vector.Scale(originalSize, scale)
	local init = { sprite = scaleImg,
		       spriteSize = size,
		       offset = offset,
		       velocity = vel,
		       pos = pos,
		       z = z, }
	local color = util.Gray(0.25 + 0.75 * scale)
	local animColor = util.Gray(0.25 + 0.75 * animScale)
	local angle = vector.Angle(vel)
	local obj = actor.Create(init)
	actor.DelayedDelete(obj, lifetime)
	util.RotateTile(obj.tile, angle)
	eapi.SetColor(obj.tile, color)
	
	local animSize = vector.Scale(originalSize, animScale)
	local animOffset = vector.Scale(originalOffset, animScale)
	eapi.AnimateSize(obj.tile, eapi.ANIM_CLAMP, animSize, lifetime, 0)
	eapi.AnimatePos(obj.tile, eapi.ANIM_CLAMP, animOffset, lifetime, 0)
	eapi.AnimateColor(obj.tile, eapi.ANIM_CLAMP, animColor, lifetime, 0)
	
	local function ScaledTile(pos, size, img, z)
		local animSize = vector.Scale(size, animScale)
		local animOffset = vector.Scale(animSize, -0.5)
		size = vector.Scale(size, scale)
		local newPos = vector.Scale(pos, animScale)
		newPos = vector.Rotate(newPos, angle)
		pos = vector.Rotate(vector.Scale(pos, scale), angle)
		local body = eapi.NewBody(obj.body, pos)
		local diff = vector.Sub(newPos, pos)
		eapi.SetVel(body, vector.Scale(diff, 1 / lifetime))
		local offset = vector.Scale(size, -0.5)
		local tile = eapi.NewTile(body, offset, size, img, z)
		eapi.AnimateSize(tile, eapi.ANIM_CLAMP, animSize, lifetime, 0)
		eapi.AnimatePos(tile, eapi.ANIM_CLAMP, animOffset, lifetime, 0)
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, animColor, lifetime, 0)
		util.RotateTile(tile, angle)
		eapi.SetColor(tile, color)
		return tile
	end

	local function AddCannon(yOffset, shouldFlip)
		local pos = { x = -36, y = yOffset }
		local size = { x = 64, y = 64 }
		local tile = ScaledTile(pos, size, cannonImg, z + 1)
		eapi.FlipY(tile, shouldFlip)
	end
	AddCannon( 120, false)
	AddCannon(-120, true)	

	local function AddFlame(yOffset, angle)
		local pos = { x = -128, y = yOffset }
		local size = { x = 160, y = 256 }
		local tile = ScaledTile(pos, size, player.flame, z - 1)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
		util.RotateTile(tile, angle)
	end
	AddFlame( 40, angle - 20)
	AddFlame(-40, angle + 20)
end

local wingImg = { }
local brokenWingImg = { }
for i = 1, 2, 1 do
	local frame = { { 0, (i - 1) * 240 }, { 256, 240 } }
	wingImg[i] = eapi.NewSpriteList(scaleFile, frame)
	local frame = { { 256, (i - 1) * 240 }, { 256, 240 } }
	brokenWingImg[i] = eapi.NewSpriteList(scaleFile, frame)
end

local function Cannon(body, dir)
	local cannon = { }
	local offset = { x = -32, y = -32 }
	cannon.body = eapi.NewBody(body, { x = -36, y = 0 })
	cannon.tile = eapi.NewTile(cannon.body, offset, nil, cannonImg, -1.5)
	eapi.FlipY(cannon.tile, dir < 0)

	local pos = nil
	local function Update(frame)
		if dir > 0 then frame = 32 - frame end
		eapi.SetFrame(cannon.tile, math.floor(frame % 32))
	end
	local function UpdatePos()
		if cannon.stopUpdate then return end
		eapi.AddTimer(cannon.body, 0.05, UpdatePos)
		pos = eapi.GetPos(cannon.body, gameWorld)
		local aim = bullet.HomingVector(player.Pos(), pos, 24)
		local angle = vector.Angle(aim)
		Update(32 * (angle + 180) / 360)
		pos = vector.Add(pos, aim)
	end
	UpdatePos()
	local function StartShooting()
		Tree(cannon, function() return pos end)
	end
	eapi.AddTimer(cannon.body, 1.5, StartShooting)
	return cannon
end

local ribFile = { "image/boss2-rib.png", filter = true }
local ribImg = eapi.ChopImage(ribFile, { 192, 64 })

local gatlingFile = { "image/boss2-gatling.png", filter = true }
local gatlingImg = eapi.ChopImage(gatlingFile, { 64, 64 })

local frame = { { 0, 160 }, { 64, 64 } }
local bossFile = { "image/boss2-misc.png", filter = true }
local brokenImg = eapi.NewSpriteList(bossFile, frame)

local sheetFile = { "image/boss2-sheet.png", filter = true }
local sheetImg = eapi.ChopImage(sheetFile, { 64, 64 })

local doorFile = { "image/boss2-doors.png", filter = true }
local doorImg = eapi.ChopImage(doorFile, { 256, 160 })

local eyeOpenFile = { "image/boss2-open.png", filter = true }
local eyeOpenImg = eapi.ChopImage(eyeOpenFile, { 88, 88 })

local lookFile = { "image/boss2-look.png", filter = true }
local lookImg = eapi.ChopImage(lookFile, { 88, 88 })

local hatchImg = { }
hatchImg[-1] = eapi.NewSpriteList(doorFile, { { 768, 1120 }, { 256, 80 } })
hatchImg[ 1] = eapi.NewSpriteList(doorFile, { { 768, 1200 }, { 256, 80 } })

local function HatchOff(obj, dir)
	local offset = { x = -128, y = -40 }
	local body = eapi.NewBody(obj.body, { x = 128, y = -dir * 40 })
	local tile = eapi.NewTile(body, offset, nil, hatchImg[dir], 1)
	explode.Area(vector.null, body, 10, 32, 0.05)
	eapi.SetVel(body, { x = 100, y = -dir * 250 })
	eapi.SetAcc(body, { x = -600, y = dir * 75 })
	util.AnimateRotation(tile, -4 * dir)
	util.DelayedDestroy(body, 1.5)
end

local function VShape(pos, vel, count, BulletFn)
	local stepBase = vector.Normalize(vel, 8)
	local stepRight = vector.Rotate(stepBase, 150)
	local stepLeft = vector.Rotate(stepBase, -150)
	for i = count, 1, -1 do
		BulletFn(vector.Add(pos, vector.Scale(stepLeft, i)), vel)
		BulletFn(vector.Add(pos, vector.Scale(stepRight, i)), vel)
	end
	BulletFn(pos, vel)
end

local function VLine(pos, vel, BulletFn)
	for i = 1, 2, 0.2 do
		VShape(pos, vector.Scale(vel, i), 3, BulletFn or bullet.Cyan)
	end
end

local function EyePos(obj)
	if obj.destroyed then return vector.null end
	return vector.Offset(eapi.GetPos(obj.body, gameWorld), 192, 0)
end

local function VArc(obj, n, angle)
	return function()
		if not obj.eyeFollow then return end
		eapi.PlaySound(gameWorld, "sound/cannon.ogg", 0, 0.5)
		bullet.AimArc(EyePos(obj), player.Pos(), VLine, n, angle, 200)
	end
end

local function VPattern(obj)
	util.DoEventsRelative(
		{ { 0.0, VArc(obj, 1, 0) },
		  { 0.8, VArc(obj, 3, 30) },
		  { 0.8, VArc(obj, 5, 45) }, })
end

local function CSpread(center)
	local pos = actor.GetPos(center)
	local vel = eapi.GetVel(center.body)
	local function Detach(child)
		if child.destroyed then return end
		eapi.SetStepC(child.body, eapi.STEPFUNC_STD)
		eapi.Unlink(child.body)
		local childPos = actor.GetPos(child)
		local diff = vector.Sub(childPos, pos)
		diff = vector.Rotate(vector.Scale(diff, 2), 90)
		local velDiff = vector.Scale(diff, 4)
		eapi.SetVel(child.body, vector.Add(vel, velDiff))
		eapi.SetAcc(child.body, vector.Scale(diff, -8))
		util.Delay(child.body, 0.4, util.StopAcc, child)
	end
	util.Map(Detach, center.children)
	center.children = nil
end

local function CShape(pos, vel)
	local offset = { x = 16, y = 0 }
	local center = bullet.CyanBig(pos, vel)
	for i = 0, 360 - 1, 45 do
		local childPos = vector.Add(pos, vector.Rotate(offset, i))
		local child = bullet.Magenta(childPos, vector.null)
		actor.Link(child, center)
		eapi.SetStepC(child.body, eapi.STEPFUNC_ROT, 12)
	end
	util.Delay(center.body, 0.8, CSpread, center)
end

local function CPattern(obj)
	local count = 120
	local angleStep = util.golden * util.fibonacci
	local vel = bullet.HomingVector(player.Pos(), EyePos(obj), 350)
	local function Emit()
		if count > 0 and obj.eyeFollow then
			CShape(EyePos(obj), vel)
			eapi.AddTimer(obj.body, 0.03, Emit)
			vel = vector.Rotate(vel, angleStep)
			count = count - 1
		end
	end
	Emit()
end

local pSize = { x = 64, y = 64 }
local pOffset = { x = -32, y = -32 }
local function Pulsating(pos, vel, Fn)
	local obj = Fn(pos, vel)
	local rnd = 0.05 * util.Random()
	eapi.AnimateSize(obj.tile, eapi.ANIM_REVERSE_LOOP, pSize, 0.05, rnd)
	eapi.AnimatePos(obj.tile, eapi.ANIM_REVERSE_LOOP, pOffset, 0.05, rnd)
end

local function PPattern(obj)
	local angle = 0
	local count = 50
	local function Bullet(angle, Fn)
		local vel = vector.Rotate(bullet.velocity, angle)
		Pulsating(EyePos(obj), vel, Fn)
	end
	local function Emit()
		if count > 0 and obj.eyeFollow then
			angle = angle + 3.5
			count = count - 1
			eapi.AddTimer(obj.body, 0.1, Emit)
			if angle % 14 == 0 then return end
			for i = angle, angle + 315, 45 do
				Bullet(-i, bullet.Magenta)
				Bullet(i, bullet.Cyan)
			end
		end
	end
	Emit()
end

local function Scare(obj)
	if obj.eyeFollow then
		local gap = 10
		local pos = EyePos(obj)
		local aim = bullet.HomingVector(player.Pos(), pos)
		eapi.PlaySound(gameWorld, "sound/cannon.ogg", 0, 1)
		for i = 0, 500, 1 do
			local scale = 0.4 + 0.2 * util.Random()
			local angle = gap + util.Random(0, 360 - 2 * gap)
			local vel = vector.Rotate(aim, angle)
			vel = vector.Normalize(vel, util.Random(200, 800))
			bullet.MagentaCustom(pos, vel, scale)
		end
	end
end

local function BPattern(obj)
	if obj.eyeFollow then
		local pos = EyePos(obj)
		local function Emit(i)
			if not obj.eyeFollow then return end
			local x = -0.002 * math.pow(math.abs(i), 2)
			bullet.CyanBlob(pos, { x = x, y = i })
			eapi.PlaySound(gameWorld, "sound/cannon.ogg", 0, 0.1)
		end
		for i = -200, 200, 20 do
			local timeout = 0.005 * (i + 200)
			util.Delay(obj.body, timeout, Emit, i)
		end
		util.Delay(obj.body, 2.5, Scare, obj)
	end
end

local function EyePattern(obj)
	util.DoEventsRelative(
		{ { 0.0, util.Closure(CPattern, obj) },
		  { 4.0, util.Closure(VPattern, obj) },
		  { 4.0, util.Closure(PPattern, obj) },
		  { 6.0, util.Closure(BPattern, obj) },
		  { 5.5, util.Closure(EyePattern, obj) }, })
end

local function EyeFollow(obj)
	local offset = { x = 158, y = -44 }
	local color = { r = 1, g = 1, b = 1, a = 1 }
	obj.eyeTile1 = eapi.NewTile(obj.body, offset, nil, lookImg, -0.90)
	obj.eyeTile2 = eapi.NewTile(obj.body, offset, nil, lookImg, -0.95)
	obj.eyeFollow = true
	local function Follow()
		if obj.eyeFollow then
			local parentPos = eapi.GetPos(obj.body, gameWorld)
			local pos = vector.Offset(parentPos, 200, 0)
			local aim = vector.Sub(pos, player.Pos())
			local angle = 80 - vector.Angle(aim)
			local frame = 31 * (angle % 360 / 360)
			eapi.SetFrame(obj.eyeTile1, math.floor(frame))
			eapi.SetFrame(obj.eyeTile2, math.ceil(frame))
			color.a = 1 - (frame % 1)
			eapi.SetColor(obj.eyeTile1, color)
		end
		eapi.AddTimer(obj.body, 0.03, Follow)
	end
	Follow()
	actor.MakeShape(obj, { l = 180, r = 220, b = -32, t = 32 })
	EyePattern(obj)
end

local function EyeOpen(obj)
	local offset = { x = 158, y = -44 }
	local tile = eapi.NewTile(obj.body, offset, nil, eyeOpenImg, -0.9)
	eapi.Animate(tile, eapi.ANIM_CLAMP, 32, 0)
	local function EyeOpened()
		eapi.Destroy(tile)
		EyeFollow(obj)
	end
	eapi.AddTimer(obj.body, 1, EyeOpened)
end

local function BlowOffHatchDoors(obj)
	bullet.Sweep()
	eapi.Destroy(obj.doorTile)
	HatchOff(obj, 1)
	HatchOff(obj, -1)
	util.Delay(obj.body, 1.5, EyeOpen, obj)
end

local function BoxedGatlingShoot(obj)
	obj.exploded = true
	actor.DeleteShape(obj)
	eapi.Destroy(obj.tile)
	obj.sprite = brokenImg
	actor.MakeTile(obj)
	local gatlings = obj.parent.gatlings
	util.AnimateRotation(obj.tile, obj.speed)
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	if gatlings[1].exploded and gatlings[2].exploded then
		BlowOffHatchDoors(obj.parent)
	end
end

local AddGatling = util.Noop

local function BoxGatling(obj, pos, dir, z, health)
	local img = player.shield
	local size = { x = 44, y = 44 }
	local offset = { x = -22, y = -22 }
	local gatling = AddGatling(obj, pos, dir, z)
	gatling.shadow = eapi.NewTile(gatling.body, offset, size, img, z - 0.2)
	eapi.SetColor(gatling.shadow, { r = 0, g = 0, b = 0, a = 0.4 })
	gatling.Shoot = BoxedGatlingShoot
	gatling.health = health
	return gatling
end

local bulletSpeed = 200

local function WingGatlingEmit(gatling, Fn, count, Correction)
	local index = 0
	local function Bullet(pos, vel)
		if count then bullet.z_epsilon = 0.01 * count end
		pos = vector.Add(pos, vector.Normalize(vel, 28))
		local projectile = Fn(pos, vel)
		projectile.index = index
		util.MaybeCall(Correction, projectile)
		index = index + 1
	end
	local pos = eapi.GetPos(gatling.body, gameWorld)
	local time = eapi.GetTime(gatling.body)
	local angle = 360 * (time % gatling.speed) / gatling.speed
	local vel = vector.Rotate({ x = bulletSpeed, y = 0 }, angle + 90)
	bullet.FullArc(pos, vel, Bullet, 6)
end

local spiralAim = { x = -400, y = 0 }
local function ReAim(projectile, aim)
	local pos = actor.GetPos(projectile)
	local spiralAngle = math.sin(projectile.gatling.counter)
	spiralAngle = 0.75 * vector.Degrees(spiralAngle)
	local vel = bullet.HomingVector(aim, pos, bulletSpeed)
	vel = vector.Rotate(vel, projectile.index * 60 + spiralAngle + 30)
	eapi.SetVel(projectile.body, vel)
end

local function SpiralVelocity(projectile)
	ReAim(projectile, spiralAim)
end

local function BoxRepeatPattern(Fn, obj, interval, delay)
	if obj.exploded then return end
	local function EmitElement(gatling)
		if not gatling.exploded then Fn(gatling) end
	end
	local count = math.floor((delay or 6) / interval)
	local RepeatFn = util.Closure(EmitElement, obj)
	util.Repeater(RepeatFn, count, interval, obj.body)
end

local function BoxSpiral(gatling)
	gatling.counter = 0
	local function ChangeToSpiral(projectile)
		util.Delay(projectile.body, 0.1, SpiralVelocity, projectile)
		projectile.gatling = gatling
	end
	local function Element(gatling)
		WingGatlingEmit(gatling, bullet.Magenta, nil, ChangeToSpiral)
		gatling.counter = gatling.counter + 0.04
	end
	BoxRepeatPattern(Element, gatling, 0.04)
end

local function BoxInverse(gatling, BulletFn)
	local counter = 0
	local function Invert(projectile)
		local vel = eapi.GetVel(projectile.body)
		vel = vector.Scale(vel, 1 + 0.09 * counter)
		eapi.SetVel(projectile.body, vel)
	end
	local function Element(gatling)
		WingGatlingEmit(gatling, BulletFn, counter, Invert)
		counter = counter + 1
	end
	BoxRepeatPattern(Element, gatling, 0.02, 0.2)
end

local function BoxInverseCyan(gatling)
	BoxInverse(gatling, bullet.Cyan)
end

local function BoxInverseMagenta(gatling)
	BoxInverse(gatling, bullet.Magenta)
end

local function CyanMagot(pos, vel)
	local offset = vector.Normalize(vel, -8)
	local subPos = vector.Add(pos, offset)
	local subPos2 = vector.Add(subPos, offset)
	bullet.CyanCustom(subPos2, vel, 0.45)
	bullet.CyanCustom(subPos, vel, 0.60)
	return bullet.CyanCustom(pos, vel, 0.75)
end

local function BoxFibonacci(gatling)
	local function Element(gatling)
		WingGatlingEmit(gatling, CyanMagot)
	end
	BoxRepeatPattern(Element, gatling, 2 * (1/6) * util.golden)
end

local function StartGatlingPattern(gatlings)
	if not (gatlings[1].exploded and gatlings[2].exploded) then
		local Sweep1 = util.Closure(BoxInverseCyan, gatlings[1])
		local Sweep2 = util.Closure(BoxInverseMagenta, gatlings[2])
		util.DoEventsRelative(
			{ { 0.0, util.Closure(BoxSpiral, gatlings[1]) },
			  { 0.2, util.Closure(BoxFibonacci, gatlings[2]) },
			  { 8.1, Sweep1 },
			  { 0.7, Sweep2 },
			  { 0.9, Sweep1 },
			  { 0.7, Sweep2 },
			  { 0.9, Sweep1 },
			  { 0.7, Sweep2 },
			  { 2.0, util.Closure(StartGatlingPattern, gatlings) },
		  })
	end
end

local function StartGatlings(obj)
	local function Animate(gatling)
		eapi.Animate(gatling.tile, eapi.ANIM_CLAMP, 32, 0)
		actor.MakeShape(gatling, actor.Square(28))
	end
	StartGatlingPattern(obj.gatlings)
	util.Map(Animate, obj.gatlings)
end

local function OpenHatch(obj)
	obj.gatlings = { }
	obj.gatlings[1] = BoxGatling(obj, { x =  88, y = 0 },  4.0, -0.6, 400)
	obj.gatlings[2] = BoxGatling(obj, { x = 128, y = 0 }, -2.0, -0.5, 100)
	eapi.Animate(obj.doorTile, eapi.ANIM_CLAMP, 32, 0)
	util.Delay(obj.body, 1, StartGatlings, obj)
end

local function Explode(obj, offset)
	local function Do()
		local noSpliter = util.Random() > 0.5
		local body = explode.Simple(offset, obj.body, -1.6, noSpliter)
		eapi.Link(body, obj.parent.body)
	end
	eapi.AddTimer(obj.body, 0.2 * util.Random(), Do)
end

local function RandomSpeed()
	local amount = 0.4 + 0.8 * util.Random()
	return amount * ((util.Random() > 0.5) and 1 or -1)
end

local function MetalSheet(pos, obj)
	local scale = 0.8 + 0.4 * util.Random()
	local size = vector.Scale({ x = 64, y = 64 }, scale)
	local init = {
		class = "Garbage",
		sprite = sheetImg,
		bb = actor.Square(4),
		offset = vector.Scale(size, -0.5),
		parentBody = obj.body,
		spriteSize = size,
		pos = pos,
		z = -1.65,
	}
	local sheet = actor.Create(init)
	util.AnimateRotation(sheet.tile, RandomSpeed())
	eapi.Animate(sheet.tile, eapi.ANIM_LOOP, 32, util.Random())
	local globalPos = eapi.GetPos(sheet.body)
	eapi.FlipX(sheet.tile, util.Random() > 0.5)
	eapi.FlipY(sheet.tile, util.Random() > 0.5)
	local vel = { x = -200, y = 4 * globalPos.y }
	eapi.SetVel(sheet.body, vector.Rnd(vel, 100))
	eapi.SetAcc(sheet.body, { x = -400, y = 0 })
	eapi.Unlink(sheet.body)
end

local function FullWingExplosion(obj)
	bullet.Sweep()
	local function Pos(x, y)
		return { x = -88 + y * 8 + x * 36,
			 y = obj.dir * (112 - y * 32 - x * 8) }
	end
	for x = 0, 3, 1 do
		for y = 1, 7, 1 do
			local pos = Pos(x, y)
			MetalSheet(pos, obj)
			for j = 8, 32, 8 do
				Explode(obj, vector.Rnd(pos, j))
			end
		end
	end
end

local function PutPlaneRib(obj, pos, angle, dir, anim, z)
	local offset = { x = 0, y = -32 }
	local body = eapi.NewBody(obj.parent.body, pos)
	local tile = eapi.NewTile(body, offset, nil, ribImg, z)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32 * dir, anim)
	util.RotateTile(tile, angle)
	eapi.FlipY(tile, dir < 0)

	return { body = body, tile = tile, angle = angle }
end
	
local function ExplodeWing(obj)
	obj.ribs = { }
	FullWingExplosion(obj)
	local function PlaneRib(x, y, angle, anim, z, i)
		local n = -obj.dir
		local pos = { x = x, y = n * y }
		anim = (n > 0) and anim or (0.834 - anim)
		local rib = PutPlaneRib(obj, pos, n * angle, n, anim, z)
		obj.ribs[i] = rib
	end
	local function Swap()
		actor.Delete(obj)
		PlaneRib(32, -64 ,  0, 0.000, -2.0, 1)
		PlaneRib(24, -136, 10, 0.166, -2.1, 2)
		PlaneRib(28, -212, 20, 0.333, -2.2, 3)
	end
	eapi.AddTimer(obj.body, 0.5, Swap)
end

local function ShowWingRibs(wing)
	local parent = wing.parent
	util.Delay(parent.body, 1.2, OpenHatch, parent)
	ExplodeWing(wing.sibling)
	ExplodeWing(wing)
end

local function StageTwoWingShoot(obj)
	local body = eapi.NewBody(obj.parent.body, obj.pos)
	local tile = eapi.NewTile(body, obj.offset, nil, brokenImg, -1.9)
	util.RotateTile(tile, 70 * obj.parent.dir)
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	actor.Delete(obj)
	obj.parent.exploded = true
	if obj.parent.sibling.exploded then
		ShowWingRibs(obj.parent)
	end
end

AddGatling = function(obj, pos, dir, z)
	local gatling = {
		class = "Mob",
		health = 300,		
		Hit = obj.Hit,
		Shoot = StageTwoWingShoot,		
		pos = pos or { x = -36, y = -4 * obj.dir },
		offset = { x = -32, y = -32 },
		parentBody = obj.body,
		sprite = gatlingImg,
		parent = obj,
		speed = dir,
		z = z or -1.9,
		dir = dir,
	}
	gatling = actor.Create(gatling)
	util.AnimateRotation(gatling.tile, gatling.speed)
	return gatling	
end

local function BlowOffCannon(obj)
	eapi.Unlink(obj.cannon.body)
	local body = explode.Simple(vector.null, obj.cannon.body, -1.2)
	eapi.SetVel(obj.cannon.body, { x = 100, y = 100 * obj.dir })
	eapi.SetAcc(obj.cannon.body, { x = -500, y = 100 * obj.dir })
	eapi.Animate(obj.cannon.tile, eapi.ANIM_LOOP, obj.dir * 128, 0)
	util.DelayedDestroy(obj.cannon.body, 1.5)
	obj.cannon.stopUpdate = true
	eapi.Link(body, obj.body)
	obj.gatling = AddGatling(obj, nil, 2 * obj.dir)
end

local function ShowBrokenWing(obj)
	local function SwapSprite()
		eapi.Destroy(obj.tile)
		obj.sprite = brokenWingImg[obj.num]
		actor.MakeTile(obj)
	end
	eapi.AddTimer(obj.body, 0.6, SwapSprite)
	for i = 1, 7, 1 do
		local pos = { x = -88 + i * 8, y = obj.dir * (112 - i * 32) }
		for j = 8, 32, 8 do
			Explode(obj, vector.Rnd(pos, j))
		end
	end
	local function Flame(offset)
		local body = eapi.NewBody(obj.body, offset)
		explode.Flame(body, { x = -200, y = 0 }, -3)
	end
	Flame({ x = -48, y = obj.dir * 56 })
	if obj.num == 1 then
		Explode(obj, { x = 16, y = -8 })
		Explode(obj, { x = -24, y = 64 })
	end
end

local function WingGatlingPatternArc(obj)
	local count = 1
	local Fn = (obj.dir > 0) and bullet.Magenta or bullet.Cyan
	local function Repeat()
		WingGatlingEmit(obj.gatling, Fn, count)
		if count < 7 then
			eapi.AddTimer(obj.gatling.body, 0.01, Repeat)
			count = count + 1
		end
	end
	Repeat()
end

local function WingGatlingEmitAimed(obj, playerPos, Fn)
	local function Bullet(pos, vel)
		pos = vector.Add(pos, vector.Normalize(vel, 28))
		Fn(pos, vel)
	end
	local pos = eapi.GetPos(obj.gatling.body, gameWorld)
	bullet.AimArc(pos, playerPos, Bullet, 11, 120)
end

local function WingGatlingPatternAimed(obj, playerPos)
	local Fn = (obj.dir > 0) and bullet.Magenta or bullet.Cyan
	WingGatlingEmitAimed(obj, playerPos, Fn)
end

local function WingGatlingPattern(obj)
	local function Arc() WingGatlingPatternArc(obj) end
	local function SevenArcs()
		util.Repeater(Arc, 10, 0.56, obj.gatling.body)
	end
	local playerPos = nil
	local function Aim() WingGatlingPatternAimed(obj, playerPos) end
	local function AimedArcs()
		playerPos = player.Pos()
		util.Repeater(Aim, 20, 0.05, obj.gatling.body)
	end
	local function DoScript()
		util.DoEventsRelative({ { 0.0, SevenArcs },
					{ 6.5, AimedArcs },
					{ 2.5, DoScript  }, },
				      obj.gatling.body)
	end
	DoScript()
end

local function StageTwoWingSpread(obj)
	actor.MakeShape(obj.gatling, actor.Square(28))
	local function WingGatlingPatternStart() WingGatlingPattern(obj) end
	eapi.AddTimer(obj.gatling.body, 1, WingGatlingPatternStart)
	eapi.Animate(obj.gatling.tile, eapi.ANIM_CLAMP, 32, 0)
end

local function StageTwoWing(obj)
	obj.exploded = false
	util.Delay(obj.body, 1 + obj.dir * 0.14, StageTwoWingSpread, obj)
end

local function StageOneWingExplode(obj)
	actor.DeleteShape(obj)
	BlowOffCannon(obj)
	ShowBrokenWing(obj)
	obj.exploded = true
	if obj.sibling.exploded then
		StageTwoWing(obj.sibling)
		StageTwoWing(obj)
		bullet.Sweep()
	end
end

local function StageOneWing(parentObj, num, dir)
	local init = { class = "Mob",
		       health = 300,
		       sprite = wingImg[num],
		       pos = { x = 128, y = dir * 120 },
		       parentBody = parentObj.body,
		       offset = { x = -128, y = -120 },
		       Shoot = StageOneWingExplode,
		       Hit = parentObj.Hit,
		       parent = parentObj,
		       num = num,
		       dir = dir,
		       z = -2, }
	local obj = actor.Create(init)
	eapi.Link(obj.body, parentObj.body)
	local function MakeShape(bt1, bt2, l, r)
		local b = math.min(bt1 * dir, bt2 * dir)
		local t = math.max(bt1 * dir, bt2 * dir)
		actor.MakeShape(obj, { b = b, t = t, l = l, r = r })
	end
	local function AddShape()
		MakeShape(-96, -32, -48,  80)
		MakeShape(-32,  32, -64,  48)
		MakeShape( 32,  80, -80,  16)
	end
	eapi.AddTimer(obj.body, 1, AddShape)
	obj.cannon = Cannon(obj.body, dir)
	return obj
end

local frame = { { 0, 0 }, { 256, 160 } }
local bossBodyImg = eapi.NewSpriteList(bossFile, frame)

local function MakeHitFn(obj)
	return function(childObj, shoot)
		if obj ~= childObj then
			obj.health = obj.health - shoot.damage
		end
		powerup.ShowProgress(obj)
	end
end

local function BlowOffRibs(obj)
	local i = 3
	local function Next()
		local rib = obj.ribs[i]
		eapi.Unlink(rib.body)
		eapi.SetAcc(rib.body, { x = -200, y = obj.dir * 50 })
		local offset = vector.Rotate({ x = 176, y = 0 }, rib.angle)
		explode.Simple(offset, rib.body, -1.6)
		util.DelayedDestroy(rib.body, 2.5)
		eapi.AnimateAngle(rib.tile, eapi.ANIM_LOOP, 
				  vector.null, 2 * math.pi, 
				  12 * obj.dir, 0)

		i = i - 1
		if i > 0 then eapi.AddTimer(obj.ribs[i].body, 0.5, Next) end
	end
	Next()
end

local function EmitStars()
	for i = 1, 250, 1 do
		local vel = { x = 0, y = util.Random(128, 512) }
		vel = vector.Rotate(vel, util.Random(-90, 90))
		powerup.Star({ x = util.Random(-64, 64), y = -256 }, vel)
	end
end

local function Remove(obj)
	actor.Delete(obj)
	eapi.Destroy(obj.masterBody)
	local tile = util.WhiteScreen(150)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, 1, 0)
	eapi.AddTimer(staticBody, 1, util.DestroyClosure(tile))
	eapi.PlaySound(gameWorld, "sound/boom.ogg")
	
	util.DoEventsRelative({ { 0.0, EmitStars },
				{ 4.0, menu.Success },
				{ 2.0, player.DisableAllInput },
				{ 0.5, player.LevelEnd },
				{ 2.5, util.Level(3) } })
end

local function GoDown(obj)
	eapi.SetAcc(obj.masterBody, { x = -100, y = -200 })	
	util.Delay(obj.body, 2, Remove, obj)
	eapi.FadeMusic(2.0)
end

local function Finalize(obj)
	bullet.Sweep()
	actor.DeleteShape(obj)
	powerup.ShowProgress(obj)
	obj.bb = { l = 32, r = 192, b = -60, t = 60 }
	explode.Area({ x = 200, y = 0 }, obj.body, 10, 48, 0.05)
	for i = 1, 3, 1 do explode.Continuous(obj) end
	BlowOffRibs(obj.wing1)
	BlowOffRibs(obj.wing2)
	util.Delay(obj.body, 2, GoDown, obj)
	eapi.Animate(obj.eyeTile1, eapi.ANIM_LOOP, 32, 0)
	eapi.Animate(obj.eyeTile2, eapi.ANIM_LOOP, -32, 0)
	eapi.SetColor(obj.eyeTile1, { r = 1, g = 1, b = 1, a = 0.5 })
	obj.eyeFollow = false
end

local function Start()
	local init = { class = "Mob",
		       health = bossHealth,
		       sprite = bossBodyImg,
		       maxHealth = bossHealth,
		       prevHealth = bossHealth,
		       offset = { x = 0, y = -80 },
		       pos = { x = 500, y = 0 },
		       Shoot = Finalize,
		       blinkZ = 0.5,
		       z = -1, }
	local obj = actor.Create(init)
	obj.Hit = MakeHitFn(obj)
	powerup.ShowProgress(obj, 0.3, 0.6)
	obj.wing1 = StageOneWing(obj, 1,  1)
	obj.wing2 = StageOneWing(obj, 2, -1)
	obj.wing1.sibling = obj.wing2
	obj.wing2.sibling = obj.wing1

	obj.doorTile = eapi.NewTile(obj.body, obj.offset, nil, doorImg, 1)
	
	local startPos = eapi.GetPos(obj.body)
	local function RotateBody(x, child, speed)
		local pos = vector.Offset(startPos, x, 0)
		local body = eapi.NewBody(gameWorld, pos)
		eapi.SetStepC(child, eapi.STEPFUNC_ROT, speed)
		eapi.Link(child, body)
		return body
	end
	obj.slaveBody = RotateBody(-8, obj.body, 3)
	obj.masterBody = RotateBody(-16, obj.slaveBody, -7)		
	actor.Rush({ body = obj.masterBody }, 1.0, 720, 0)

	local function Flicker(tile, pos, size, scale)
		pos = { x = pos.x * 0.8, y = pos.y }
		size = { x = size.x * 0.8, y = size.y }
		local start = (scale == 1) and 0 or 0.1
		eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, pos, 0.1, start)
		eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, size, 0.1, start)
	end
	local function AddFlame(xOffset, yOffset, angle, scale, z)
		local img = player.flame
		local size = vector.Scale({ x = 160, y = 256 }, scale)
		local pos = vector.Scale({ x = -128, y = -128 }, scale)
		pos = vector.Offset(pos, xOffset, yOffset)
		local tile = eapi.NewTile(obj.body, pos, size, img, z)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
		Flicker(tile, pos, size, scale)
		util.RotateTile(tile, angle)
	end
	local function AddDoubleFlame(xOffset, yOffset, angle, scale, z)
		AddFlame(xOffset, yOffset, angle, scale, z)
		AddFlame(xOffset, -yOffset, -angle, scale, z)
	end
	AddDoubleFlame(  0,  40, -20, 1.0, -3.0)
	AddDoubleFlame(-20,  50, -40, 0.5, -3.1)
	AddDoubleFlame(  5, -20, 0.1, 0.5, -3.1)
end

local function Approach()
	local function P(x, y)
		return { x = x, y = y }
	end
	local function Delay(pos, vel, z, scale1, scale2, life)
		return function() Scale(pos, vel, z, scale1, scale2, life) end
	end
	util.DoEventsRelative(
	{ { 0.0, Delay(P(-300, -200), P(100,  400), -45, 0.2, 0.3, 1.4) },
	  { 2.0, Delay(P(-100,  300), P(150, -600), -35, 0.4, 0.6, 1.2) },
	  { 2.0, Delay(P( 100, -200), P(200,  800), -25, 0.6, 0.9, 1.0) }, })
end

pussyFilter = {
	Start = Start,
	Approach = Approach,
}
return pussyFilter
