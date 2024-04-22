local cargoTopFile = { "image/cargo-top.png", filter = true }
local cargoTopImg = eapi.ChopImage(cargoTopFile, { 192, 64 })
local cargoBottomImg = eapi.ChopImage("image/cargo-bottom.png", { 192, 64 })

local function StarArc(pos, vel)
	vel = vector.Rotate(vel, 10 * (util.Random() - 0.5))
	bullet.Arc(pos, vel, powerup.Star, 3, 15)	
end

local function CargoStars(mob)
	local pos = actor.GetPos(mob)
	bullet.Line(pos, { x = -300, y = 0 }, StarArc, 5, 0.7, 1.3)
end

local function CrashingStars(mob)
	local count = 10 - mob.dir
	local pos = actor.GetPos(mob)
	local angle = 150 - 15 * (mob.dir + 1) 
	bullet.Arc(pos, { x = 0, y = 500 }, powerup.Star, count, angle)	
end

local function ShipShoot(mob)
	if mob.splitup or mob.crashing then
		local z = mob.z + 0.5
		explode.Bulk(mob, 32, 64)
		if not mob.crashing then
			CargoStars(mob)
		else
			CrashingStars(mob)
		end
		actor.DelayedDelete(mob, 0.5)
		eapi.AnimateColor(mob.tile, eapi.ANIM_CLAMP,
				  util.invisible, 0.5, 0)
		util.Map(eapi.Destroy, mob.flameTiles)
		mob.splitup = true
	else		
		mob.Crash()
		mob.sibling.Crash()
	end
end

local function AddFlames(obj)
	obj.flameTiles = { }
	local z = obj.z - 0.5
	local img = player.flame
	for i = -2, 2, 1 do
		local offset = { x = 112 - math.abs(i) * 8, y = -32 + i * 7 }
		local tile = eapi.NewTile(obj.body, offset, nil, img, z)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
		obj.flameTiles[tile] = tile
		eapi.FlipX(tile, true)		
	end
end

local function RevealBlobs(obj)
	-- splitup
	local function Splitup()
		if not obj.crashing and not obj.splitup then
			local pos = actor.GetPos(obj)
			for i = 1, 4, 1 do				
				local vel = { x = -40, y = 0 }
				local time = 1 + i * 1 + obj.dir * 0.25
				local x = i * 16 + obj.dir * 4 - 24
				local y = -24 * obj.dir
				local where = vector.Offset(pos, x, y)
				bullet.CyanBlob(where, vel, time, nil,
						(i == 4) and obj.FollowUp)
			end
			eapi.SetVel(obj.body, { x = -40, y = obj.dir * 50 })
			eapi.SetAcc(obj.body, { x = 0, y = -obj.dir * 50 })
			eapi.AddTimer(obj.body, 1, obj.Constant)
			obj.splitup = true
		end
	end
	eapi.AddTimer(obj.body, 4.0, Splitup)

	-- shooting
	local function Row(pos, vel)
		bullet.Line(pos, vel, bullet.MagentaLong, 3, 0.9, 1.1)
	end
	local function EmitBullets()
		if not obj.splitup then
			local pos = actor.GetPos(obj)
			bullet.AimArc(pos, player.Pos(), Row, 3, 30)
			eapi.AddTimer(obj.body, 2, EmitBullets)
		end
	end
	eapi.AddTimer(obj.body, 1.0, EmitBullets)
end

local function CyanBig(pos, vel)
	local obj = bullet.CyanBig(pos, vel)
	eapi.AnimateAngle(obj.tile, eapi.ANIM_LOOP, 
			  vector.null, 2 * math.pi,
			  0.1, 0.1 * util.Random())
end

local function FastSplitter(obj, Pattern, Protect)		
	if obj.crashing then return end
	local function MaybeFollowup()
		util.MaybeCall(obj.FollowUp)
	end
	local function Split()
		if not obj.crashing and not obj.splitup then
			local vel = eapi.GetVel(obj.body)
			vel.y = obj.dir * 100
			eapi.SetVel(obj.body, vel)
			util.MaybeCall(Pattern, obj)
			eapi.AddTimer(staticBody, 3, MaybeFollowup)
			obj.splitup = true
		end
	end
	Protect(obj)
	eapi.AddTimer(obj.body, 1.5, Split)
end

local function FrontProtect1(obj)
	local arcCount = 6
	local arcAngle = 30
	local yoffset = 0
	local function BigArc()
		local pos = vector.Offset(actor.GetPos(obj), 0, -yoffset)
		local vel = { x = -bullet.speed, y = yoffset - 32 }
		bullet.Arc(pos, vel, CyanBig, arcCount, arcAngle)
		arcCount = arcCount - 1
		arcAngle = arcAngle - 5
		yoffset = 64 - yoffset
	end
	local circleCount = 15
	local function BigCircle()
		local pos = vector.Offset(actor.GetPos(obj), 0, -yoffset)
		local vel = { x = -bullet.speed, y = 32 - yoffset }
		bullet.FullArc(pos, vel, bullet.Magenta, circleCount)
		vel = vector.Rotate(vector.Scale(vel, 0.95), 180 / circleCount)
		bullet.FullArc(pos, vel, bullet.Magenta, circleCount)
		circleCount = circleCount + 1
	end
	if obj.dir > 0 then
		util.Repeater(BigArc, 3, 0.6, obj.body)		
		util.Repeater(BigCircle, 2, 0.6, obj.body, 0.3)
	end
end

local function FrontProtect2(obj)
	local flip = 11.5
	local arcCount = 7
	local arcAngle = 35
	local baseVel = { x = -bullet.speed, y = 0 }
	local function Arcs()
		local vel = vector.Rotate(baseVel, flip)
		local yoffset = (flip > 0) and 0 or -64
		local pos = vector.Offset(actor.GetPos(obj), 0, yoffset)
		local Bullet = bullet[(flip < 0) and "Cyan" or "Magenta"]
		bullet.Arc(pos, vel, Bullet, arcCount, arcAngle)
		flip = -flip
	end
	if obj.dir > 0 then
		util.Repeater(Arcs, 6, 0.3, obj.body)		
	end
end

local function FastSplit(Pattern, Protect)
	return function(obj) FastSplitter(obj, Pattern, Protect) end
end
	
local function ShipCommon(pos, init, Pattern)
	init.z = 14
	init.jerk = 8
	init.pos = pos
	init.health = 150
	init.class = "Mob"
	init.splitup = true
	init.Shoot = ShipShoot
	init.offset = { x = -64, y = -32 }
	init.bb = { t = 31, b = -31, l = -56, r = 84 }
	init.decalOffset = { x = 32, y = -10 }
	init.Hit = actor.SiblingHit
	local obj = actor.Create(init)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 16, util.Random())
	actor.MakeShape(obj, { t = 16, b = -16, l = 84, r = 128 })
	AddFlames(obj)

	local function Rush()
		obj.Constant = actor.Rush(obj, 0.9, 400, 40)
	end
	obj.rush = eapi.AddTimer(obj.body, 0.01, Rush)

	eapi.AddTimer(obj.body, 1, function() util.MaybeCall(Pattern, obj) end)

	local function Boost()
		eapi.SetAcc(obj.body, { x = -40, y = 0 })
	end
	eapi.AddTimer(obj.body, 10.0, Boost)
		
	-- crashing
	obj.Crash = function()
		eapi.SetAcc(obj.body, { x = 0, y = -100 })
		explode.Continuous(obj)
		obj.health = 1000000
		obj.crashing = true
		util.MaybeCall(obj.FollowUp)
	end

	return obj
end

local function ShipTop(pos, Pattern)
	local init = { sprite = cargoTopImg, dir = 1 }
	return ShipCommon(pos, init, Pattern)
end

local function ShipBottom(pos, Pattern)
	local init = { sprite = cargoBottomImg, dir = -1 }
	return ShipCommon(pos, init, Pattern)
end

local function SplittingShip(pos, FollowUp, Pattern)
	local top = ShipTop(vector.Offset(pos, 0, 31), Pattern)
	local bottom = ShipBottom(vector.Offset(pos, 0, -31), Pattern)
	top.FollowUp = FollowUp
	top.sibling = bottom
	top.splitup = false
	bottom.sibling = top
	bottom.splitup = false
	return { top, bottom }
end

local function Ship(pos, FollowUp)
	SplittingShip(pos, FollowUp, RevealBlobs)
end

local function Splitting(pos, FollowUp, Pattern, Protect)
	SplittingShip(pos, FollowUp, FastSplit(Pattern, Protect))	
end

local scissorVel = {
	vector.Normalize({ x = -3, y =  1 }, bullet.speed),
	vector.Normalize({ x = -3, y = -1 }, bullet.speed) }

local function Scissor(obj)
	if not obj.scissorAlternate then obj.scissorAlternate = 0 end
	local vel = scissorVel[obj.scissorAlternate + 1]
	obj.scissorAlternate = (obj.scissorAlternate + 1) % 2
	bullet.Line(actor.GetPos(obj), vel, bullet.Cyan, 3, 0.9, 1.1)
	eapi.AddTimer(obj.body, 0.5, function() Scissor(obj) end)
end

local function Vertical(pos, vel, Delivery)	
	local objs = { }
	local size = { x = 96, y = 32 }
	local function Cargo(img, dir)
		local obj = { dir = dir }
		local offset = { x = -48, y = dir * 15 }
		obj.body = eapi.NewBody(gameWorld, pos)
		obj.tile = eapi.NewTile(obj.body, offset, size, img, -24)
		eapi.Animate(obj.tile, eapi.ANIM_LOOP, 16, 0)
		util.RotateTile(obj.tile, -90)

		local img = player.flame
		local size = { x = 64, y = 96 }
		local offset = vector.Offset(offset, -48, -64)
		obj.flame = eapi.NewTile(obj.body, offset, size, img, -24.1)
		eapi.Animate(obj.flame, eapi.ANIM_LOOP, 32, util.Random())
		util.RotateTile(obj.flame, 90)

		return obj
	end
	objs[1] = Cargo(cargoTopImg, 1)
	objs[2] = Cargo(cargoBottomImg, -1)
	eapi.Link(objs[2].body, objs[1].body)
	eapi.SetAcc(objs[1].body, vector.Scale(vel, -1))
	eapi.SetVel(objs[1].body, vel)

	local function FallDown(obj)
		local angle = -(obj.dir + 0.5) * math.pi
		eapi.AnimateAngle(obj.tile, eapi.ANIM_CLAMP, 
				  vector.null, angle, 4, 0)
		eapi.SetVel(obj.body, { x = obj.dir * 100, y = 150 })
		util.DelayedDestroy(obj.body, 1.5)
	end
	local function PopOpen()
		util.Map(FallDown, objs)
		eapi.Unlink(objs[2].body)
		local center = vector.Offset(actor.GetPos(objs[2]), 16, 0)
		util.PlaySound(gameWorld, "sound/explode.ogg", 0.1, 0, 0.25)
		for i = -16, 48, 12 do
			local pos = vector.Rnd(vector.Offset(center, 0, i), 8)
			explode.Small(pos, -23.9)
		end
		eapi.SetAcc(objs[2].body, vector.Scale(vel, -1))
		util.Map(function(obj) eapi.Destroy(obj.flame) end, objs)
		util.MaybeCall(Delivery, center)
	end
	eapi.AddTimer(objs[1].body, 1.0, PopOpen)
end	

local function Braid(obj)
	local stop = false
	local store = { }
	local offset = { x = 4, y = 0 }
	local function Repeat()
		if stop then return end
		for x = -32, 60, 12 do
			local pos = actor.GetPos(obj)
			pos = vector.Add(pos, offset)
			pos = vector.Offset(pos, x, -26 * obj.dir)
			local projectile = bullet.Cyan(pos, vector.null)
			local vel = vector.Offset(offset, 0.15 * (x - 64), 0)
			eapi.SetVel(projectile.body, vector.Scale(vel, -16))
			offset = vector.Rotate(offset, util.fibonacci)
			eapi.SetAcc(projectile.body, vector.Scale(vel, 16))
			store[projectile] = projectile
		end
		eapi.AddTimer(obj.body, 0.05, Repeat)
	end
	Repeat()

	local function StopBullet(projectile)
		if projectile.destroyed then return end
		eapi.SetAcc(projectile.body, { x = 400, y = 0 })
	end
	local function Stop()
		util.Map(StopBullet, store)
		stop = true
	end
	eapi.AddTimer(staticBody, 3.0, Stop)
end

local rotationSpeed = -0.25
local bulletEmitInterval = 0.02
local stepAngle = (360 / rotationSpeed) * bulletEmitInterval

local function BeltBullet(pos, acc)
	if pos.x < 400 and pos.y < 240 then
		local obj = bullet.Cyan(pos, vector.Add(train.velocity, acc))
		-- eapi.SetAcc(obj.body, acc)
		return obj
	end
end

local function EmitBelt(obj)
	local index = 1
	local count = -1
	local store = { }
	local boomCount = 1
	local slope = { x = 32, y = 0 }
	local function Emit()
		local pos = actor.GetPos(obj)
		local offset = vector.Rotate(slope, -90)
		local boomShift = (boomCount % 3) - 1
		local boomPos = vector.Add(pos, vector.Scale(slope, boomShift))
		local body = explode.Simple(boomPos, staticBody, 15)		
		eapi.SetVel(body, train.velocity)
		pos = vector.Add(pos, offset)
		for i = -1, 1.5, 0.1 do
			local adjust = vector.Scale(slope, i)
			local jitter = vector.Rnd(adjust, 16)
			local finalPos = vector.Add(pos, jitter)
			local mod = -4 * (1 - count * count)
			local acc = vector.Scale(adjust, mod)
			local item = BeltBullet(finalPos, acc)
			if item then
				store[index] = item
				item.number = index
				index = index + 1
			end
		end
		eapi.AddTimer(obj.body, bulletEmitInterval, Emit)
		slope = vector.Rotate(slope, stepAngle)
		boomCount = boomCount + 1
		count = count + 0.04
	end
	Emit()
	local function Explode()
		local aim = player.Pos()
		local function Adjust(projectile)
			if projectile.destroyed then return end
			local pos = actor.GetPos(projectile)
			eapi.SetAcc(projectile.body, vector.null)
			local vel = bullet.HomingVector(aim, pos)
			local amount = (projectile.number % 5) - 2
			local noise = util.Random(-3, 3)
			vel = vector.Rotate(vel, 30 * amount + noise)
			vel = vector.Scale(vel, 1 - 0.1 * amount * amount)
			eapi.SetVel(projectile.body, vel)
		end
		player.KillFlash(0.5, { r = 1, g = 1, b = 1, a = 0.7 })
		eapi.PlaySound(gameWorld, "sound/boom.ogg")
		util.Map(Adjust, store)
		actor.Delete(obj)
	end
	eapi.AddTimer(obj.body, 0.6, Explode)
end

local function Belt(obj)
	obj.splitup = true
	if obj.dir > 0 then
		eapi.SetVel(obj.body, { x = -200, y = 800 })
		eapi.SetAcc(obj.body, { x = 1600, y = -600 })
		util.AnimateRotation(obj.tile, rotationSpeed)
		actor.DeleteShape(obj)
		EmitBelt(obj)
	end
end

local function AddShadow(obj)
	if obj.dir < 0 then
		local z = obj.z - 0.1
		local size = { x = 200, y = 24 }
		local pos =  { x = -85, y = -38 }
		local tile = eapi.NewTile(obj.body, pos, size, tank.shadow, z)
		eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0.4 })
	end
end

local function FixForTransportation(obj)
	eapi.CancelTimer(obj.rush)
	eapi.SetFrame(obj.tile, 0)
	eapi.SetVel(obj.body, train.velocity)
	util.Map(eapi.Destroy, obj.flameTiles)
	obj.flameTiles = { }
	obj.health = 60
	AddShadow(obj)

	obj.Crash = function()
		obj.splitup = true
		ShipShoot(obj)
	end
end

local function Transported(pos)
	local objs = SplittingShip(pos, nil, Belt)
	util.Map(FixForTransportation, objs)
end

cargo = {
	Ship = Ship,
	Braid = Braid,
	ShipTop = ShipTop,
	Vertical = Vertical,
	ShipBottom = ShipBottom,
	FrontProtect1 = FrontProtect1,
	FrontProtect2 = FrontProtect2,
	Transported = Transported,
	Splitting = Splitting,
	Scissor = Scissor,
	topImg = cargoTopImg,
}
return cargo
