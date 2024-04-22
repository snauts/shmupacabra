dofile("script/parallax.lua")
dofile("script/level2-parallax.lua")
dofile("script/player.lua")
dofile("script/powerup.lua")
dofile("script/bullet.lua")
dofile("script/rocket.lua")
dofile("script/bomber.lua")
dofile("script/plane.lua")
dofile("script/cargo.lua")
dofile("script/gatling.lua")
dofile("script/baker.lua")
dofile("script/serpent.lua")
dofile("script/boss2.lua")

player.Start(true, 0.3, 0.6)

local Attack = bomber.Attack(0.3, 5, 0.5)
local function AttackingBomber(pos, vel)
	return function()
		bomber.Launch(pos, vel, Attack)
	end
end

local function BomberFormationFromTop(x)
	local function EmitBomber(pos)
		return AttackingBomber(pos, { x = -100 - x, y = -200 })
	end
	local function Pos(offset)
		return { x = offset + x, y = 300 }
	end
	return function()
		util.DoEventsRelative({ { 0.00, EmitBomber(Pos(320)) },
					{ 0.10, EmitBomber(Pos(400)) },
					{ 0.10, EmitBomber(Pos(480)) },
					{ 0.20, EmitBomber(Pos(240)) },
					{ 0.40, EmitBomber(Pos(160)) }})
	end
end

local function BomberFormationFromBottom()
	local function EmitBomber(pos)
		return AttackingBomber(pos, { x = -100, y = 200 })
	end
	util.DoEventsRelative({ { 0.00, EmitBomber({ x = 360, y = -300 }) },
				{ 0.20, EmitBomber({ x = 440, y = -300 }) },
				{ 0.20, EmitBomber({ x = 500, y = -300 }) },
				{ 0.05, EmitBomber({ x = 280, y = -300 }) },
				{ 0.45, EmitBomber({ x = 200, y = -300 }) }, })
end

local function RocketFan(dir)
	local angle = dir * 50
	local vel = { x = -dir * 600, y = 500 }
	local pos = { x = dir * 300, y = -200 }
	local function EmitRocket()
		rocket.Smoking(pos, vel)
		vel = vector.Offset(vel, dir * 51, 0)
		pos = vector.Offset(pos, dir * 5, 0)
	end
	return function()
		util.Repeater(EmitRocket, 13, 0.1, staticBody)
	end
end

local function FearlessFollowers()
	local count = 1
	local function Emit()
		eapi.RandomSeed(count * 0xdead)
		y = util.Random(-240, 240)
		plane.Follower({ x = 464, y = y })
		count = count + 1
	end
	util.Repeater(Emit, 40, 0.2)
end

local Strategy = bomber.Accelerate(0, { x = 200, y = -100 }, bomber.Scissor())

local function Swooper(pos)
	return function()
		bomber.Swoop(pos, { x = 0, y = 400 }, Strategy)
	end
end

local function SwooperWave()
	util.DoEventsRelative({ {  1.0, Swooper({ x = 192, y = -240 }) }, 
				{  1.0, Swooper({ x = 112, y = -240 }) },
				{  0.2, Swooper({ x = 256, y = -240 }) },
				{  1.3, Swooper({ x = 48,  y = -240 }) },
				{  0.2, Swooper({ x = 192, y = -240 }) },
				{  0.2, Swooper({ x = 352, y = -240 }) } })
end

local function BigBomberFormation()
	local i = 0
	local pos = { x = 432, y = 0 }
	local vel = { x = -200, y = 0 }
	local function Strategy(dir)
		local acceleration = { x = -100, y = 32 * dir * i }
		local Fn = bomber.Accelerate(1, acceleration)
		return bomber.Lines(1.0, Fn)
	end	
	local function EmitBomber()		
		if i == 0 then
			bomber.Launch(pos, vel, Strategy(0))
		else
			local pos1 = vector.Offset(pos, 20 * i, 16 + 48 * i)
			bomber.Launch(pos1, vel, Strategy(1))
			local pos2 = vector.Offset(pos, 20 * i, -16 - 48 * i)
			bomber.Launch(pos2, vel, Strategy(-1))
		end
		i = i + 1
	end
	util.Repeater(EmitBomber, 4, 0.2)
end
			
local function RetreatingBombers()
	local i = 0
	local dir = 1
	local function EmitBomber()
		local pos = { x = 432, y = -64 + i * 56 }
		local Strategy = bomber.Sway(1.0, bomber.Arc(dir, 0.2))
		bomber.Launch(pos, { x = -250, y = 0 }, Strategy)
		dir = -dir
		i = i + 1
	end
	util.Repeater(EmitBomber, 5, 0.4)
end

local function RocketArc(pos, angle1, angle2, step)
	for i = angle1, angle2, step do
		local vel = vector.Rotate({ x = 0, y = 400 }, i)
		rocket.Smoking(pos, vel)
	end
end

local function DeliverRockets(pos)
	local function Deliver(pos)
		RocketArc(pos, -32, 32, 8)
	end
	return function()
		cargo.Vertical(pos, { x = 0, y = 300 }, Deliver)
	end
end

local function RocketDeliveryService()
	util.DoEventsRelative(
		{ { 0.0, DeliverRockets({ x =    0, y = -200 }) },
		  { 1.0, DeliverRockets({ x = -200, y = -200 }) },
		  { 1.0, DeliverRockets({ x =  200, y = -200 }) }, })
end

local function Delay(pos, vel, scale)
	return function() gatling.Launch(pos, vel, scale) end
end

local function GatlingAttack()
	local function V(angle)
		return vector.Rotate({ x = -100, y = 0 }, angle)
	end
	util.DoEventsRelative(
		{ { 0.0, Delay({ x = 512, y = 0 }, V(0), 1) }, 
		  { 1.0, Delay({ x = 464, y = -32 }, V(15), 0.5) },
		  { 0.2, Delay({ x = 464, y = -32 }, V(40), 0.5) },
		  { 0.2, Delay({ x = 464, y = 32 }, V(-15), 0.5) },
		  { 0.2, Delay({ x = 464, y = 32 }, V(-40), 0.5) },
		  { 0.8, Delay({ x = 464, y = -192 }, V(-15), 0.5) },
		  { 0.4, Delay({ x = 464, y = 192 }, V(15), 0.5) },
		  { 1.0, Delay({ x = 512, y = 0 }, V(0), 1) }, })
end

local PostGatlingDelivery = DeliverRockets({ x = 200, y = -200 })

local function PlaneSwarm()
	local function Emit()
		plane.BackRunner({ x = 464, y = -300 }, 
				 { x = -500, y = 300, }, 
				 { x = 450, y = 0 })
		plane.BackRunner({ x = 464, y = 300 }, 
				 { x = -500, y = -300 }, 
				 { x = 600, y = 0 })
	end
	util.Repeater(Emit, 20, 0.3)
end

local function HomingGatlings()
	local scale = 0.5
	local height = 0
	local function Emit()
		gatling.Homing(scale, height)
		height = (height == 0) and 240 or -height
		scale = scale + 0.1		
	end
	util.Repeater(Emit, 6, 1)
end

local function RocketBomber(pos, vel, rocketDelay1, rocketDelay2, acc, delay)
	local Strategy = bomber.Rocket({ rocketDelay1, rocketDelay2 })
	if acc and delay then 
		Strategy = bomber.Accelerate(delay, acc, Strategy)
	end
	return function() bomber.Launch(pos, vel, Strategy) end
end

local function RocketBomberFirstWave()
	local function V(angle)
		return vector.Rotate({ x = -200, y = 0 }, angle)
	end
	local function Pair(angle, delay)
		return function()
			delay = delay or 1
			local d2 = delay - 0.5
			RocketBomber({ x = 464, y = -200 }, V(-angle),
				     d2, delay, { x = 0, y = -200 }, delay)()
			RocketBomber({ x = 464, y = 200 }, V(angle),
				     delay, d2, { x = 0, y = 200 }, delay)()
		end
	end
	local function Straight()
		return RocketBomber({ x = 464, y = 0 }, V(0), 0.5, 1.0)
	end
	util.DoEventsRelative({ {  0.0, Pair(15) },
				{  0.5, Pair(20) },
				{  0.5, Pair(25) },
				{  0.5, Pair(30) },
				{  0.5, Pair(40, 0.7) },
				{  0.5, Pair(50, 0.5) },
				{  1.2, Straight() }, })
end

local function DeliverBraid(Continue)
	cargo.Splitting({ x = 464, y = 0 }, Continue,
			cargo.Braid, cargo.FrontProtect1)
end

local function Interleavers()
	local x = 200
	local dir = 1
	local acc = { x = 500, y = 0 }
	local function Emit()
		local pos = { x = x, y = dir * 300 }
		local vel = { x = -500, y = -dir * 300 }
		plane.Leaver(pos, vel, acc)
		x = x + 25
		dir = -dir
	end
	util.Repeater(Emit, 16, 0.5)
end

local function VRow(pos)
	local store = { }
	local function Bullets(projectile, obj) 
		local objPos = actor.GetPos(obj)
		local aim = { x = -400, y = pos.y + 9 * (pos.y - objPos.y) }
		local vel = bullet.HomingVector(aim, objPos)
		eapi.SetVel(projectile.body, vel)
	end
	for i = 0, 9, 1 do
		local function Put(dir)
			local x = i * 20
			local y = dir * (14 + 10 * i)
			local delay = 1 + 0.1 * i
			local place = vector.Offset(pos, x, y)
			local obj = baker.Put(place, -dir * 90, delay, Bullets)
			eapi.Animate(obj.tile, eapi.ANIM_LOOP, 32, i * 0.1)
			actor.Rush(obj, 0.9, 400, 50)
			store[obj] = obj

			local function Turnable() 
				baker.MakeTurnable(obj, 2)
			end
			local turnDelay = 0.2 + i * 0.1
			eapi.AddTimer(obj.body, turnDelay, Turnable)
			obj.index = i
		end
		Put( 1)
		Put(-1)
	end
	local acc = { x = -50, y = 0 }
	local function Accelerate(obj)
		if not obj.destroyed then eapi.SetAcc(obj.body, acc) end
	end
	local function Spread(obj)
		if not obj.destroyed then 
			local angle = -obj.pos.y / 2
			eapi.SetAcc(obj.body, vector.Rotate(acc, angle)) 
		end
	end
	local function AccelerateAll()
		util.Map(Accelerate, store)
	end
	local function SpreadAll()
		util.Map(Spread, store)
	end
	eapi.AddTimer(staticBody, 1.2, AccelerateAll)
	eapi.AddTimer(staticBody, 3.0, SpreadAll)
end

local function WWWs()
	local function W()
		VRow({ x = 416, y = 120 })
		VRow({ x = 448, y = -120 })
	end
	util.Repeater(W, 7, 1.5)
end

local function CCCs()
	local function C(pos, vel, acc1, acc2, angle, step, minAngle, maxAngle)
		local dir = { x = -bullet.speed, y = 0 }
		local function Bullets(projectile, obj) 
			eapi.SetVel(projectile.body, vector.Rotate(dir, angle))
			if angle < minAngle or angle > maxAngle then
				step = -step
			end
			angle = angle + step
		end
		local function Emit()
			local obj = baker.Turning(pos, vel, 0.5, Bullets) 
			eapi.SetAcc(obj.body, acc1)
			local function Acc()
				eapi.SetAcc(obj.body, acc2)
			end
			eapi.AddTimer(obj.body, 1, Acc)			
		end
		util.Repeater(Emit, 50, 0.25)
	end
	C({ x = 416, y = -220 }, { x = -120, y = 20 },
	  { x = 0, y = 120 }, { x = 120, y = 0 }, 0, 10, -60, 60)
	C({ x = 496, y = 190 }, { x = -135, y = 0 },
	  { x = 0, y = -130 }, { x = 135, y = 10 }, 0, -10, -60, 60)

	C({ x = -120, y = -256 }, { x = 10, y = 100 },
	  { x = 120, y = 0 }, { x = 0, y = -100 }, -90, 10, -170, 10)
	C({ x = 280, y = -320 }, { x = -40, y = 120 },
	  { x = -75, y = 0 }, { x = 0, y = -120 }, -90, -10, -170, 10)

	C({ x = -120, y = 256 }, { x = 10, y = -100 },
	  { x = 120, y = 0 }, { x = 0, y = 100 }, 90, 10, -10, 170)
	C({ x = 280, y = 320 }, { x = -40, y = -120 },
	  { x = -75, y = 0 }, { x = 0, y = 120 }, 90, -10, -10, 170)	  
end

local function Serpent(pos, vel)
	return function() serpent.Put(pos, vel) end
end
local function RipplingSerpents()
	util.DoEventsRelative({ 
		{ 0.0, Serpent({ x = 464, y = 0 }, { x = -200, y = 0 }) },
		{ 2.0, Serpent({ x = 464, y = 250 }, { x = -150, y = -150 }) },
		{ 3.0, Serpent({ x = 464, y = -250 }, { x = -150, y = 150 }) },
		{ 2.5, Serpent({ x = 300, y = 250 }, { x = 0, y = -200 }) },
	})
end

local function BakerTower(pos, vel, acc, step, count)
	count = count or 10
	local speed = 200
	local playerPos = nil
	local angle = -0.5 * step * count
	local function Bullets(projectile, obj)
		playerPos = playerPos or player.Pos()
		local pos = actor.GetPos(projectile)
		local vel = bullet.HomingVector(playerPos, pos, speed)
		vel = vector.Rotate(vel, angle)
		eapi.SetVel(projectile.body, vel)
		angle = angle + step
		speed = speed + 20
	end
	local Strategy = bomber.Accelerate(1, acc)
	local obj = bomber.Launch(pos, vel, Strategy, 6)
	for i = 1, count, 1 do
		obj = baker.Following(pos, obj, 0.5 + i * 0.1, Bullets)
	end
end

local function BakerTowerGang()
	local x = 200
	local accY = -40
	local velY = 100
	local flip = -1
	local function Tower(dir)
		BakerTower({ x = x, y = dir * 280 },
			   { x = 0, y = -dir * velY },
			   { x = 200, y = -dir * accY },
			   dir * 2)
	end
	local function Emit()
		Tower(flip)
		velY = velY + 5
		accY = accY - 10
		x = x - 40
		flip = -flip
	end
	util.Repeater(Emit, 8, 0.6)
end

local function DoCircles(obj)
	if obj.dir < 0 then return end

	local store = { }
	local pos = vector.Offset(actor.GetPos(obj), 16, -31)
	local center = eapi.NewBody(gameWorld, pos)	
	eapi.SetVel(center, { x = eapi.GetVel(obj.body).x, y = 0 })
	local function Bullets(projectile, obj)
		local pos = actor.GetPos(projectile)
		local vel = vector.Sub(pos, eapi.GetPos(center))
		vel = vector.Rotate(vel, obj.dir * 90)
		vel = vector.Normalize(vel, bullet.speed)
		eapi.SetVel(projectile.body, vel)
	end
	local function Add(distance, count, dir, delay, cyan)
		for i = 0, count - 1, 1 do
			local vel = { x = distance, y = 0 }
			local vec = vector.Rotate(vel, i * 360 / count)
			vel = vector.Normalize(vec, distance * 2)
			local place = vector.Add(pos, vec)
			local item = baker.Turning(place, vel, delay, Bullets)
			eapi.Link(item.body, center)
			store[item] = item
			item.dir = dir

			if cyan then
				item.bulletImg = bullet.cyanImg
				item.Bullet = bullet.Cyan
			end
		end
	end
	Add(40, 32,  1, 0.8)
	Add(32, 28, -1, 1.2, true)
	Add(24, 24,  1, 1.6)
	local function Rotate(item)
		if not item.destroyed then 
			eapi.SetStepC(item.body, eapi.STEPFUNC_ROT, item.dir)
		end
	end
	local function Spread(item)
		if not item.destroyed then 
			eapi.SetStepC(item.body, eapi.STEPFUNC_STD)
			local centerPos = eapi.GetPos(center)
			local pos = eapi.GetPos(item.body)
			local vel = vector.Rotate(pos, -90 * item.dir)
			eapi.SetAcc(item.body, vector.null)
			eapi.SetVel(item.body, vel)
		end
	end
	local function RotateAll()
		util.Map(Rotate, store)
		eapi.SetVel(center, vector.null)
	end
	local function SpreadAll()
		util.Map(Spread, store)
	end
	eapi.AddTimer(staticBody, 2.0, RotateAll)
	eapi.AddTimer(staticBody, 3.0, SpreadAll)
	util.DelayedDestroy(center, 10)
end

local function CirclesOfDoom(Continue)
	cargo.Splitting({ x = 464, y = 0 }, Continue, 
			DoCircles, cargo.FrontProtect2)
end

local function BossScript()
	util.DoEventsRelative({ {  0.0, menu.Warning },
				{  2.0, pussyFilter.Approach },
				{  0.0, menu.StopMusic(4.0) },
				{  4.0, menu.BossMusic },
				{  3.0, pussyFilter.Start }, })
end

local function MasterScript()
	util.DoEventsRelative({ {  1.0, menu.Mission(2) }, 
				{  2.5, BomberFormationFromBottom },
				{  3.0, BomberFormationFromTop(0) },
				{  2.0, BomberFormationFromTop(50) },
				{  3.0, RocketFan(1) },
				{  6.0, FearlessFollowers },
				{  9.0, SwooperWave },
				{  7.0, RocketFan(-1) },
				{  6.0, BigBomberFormation },
				{  4.0, RocketDeliveryService }, 
				{  8.0, RetreatingBombers },
				{  3.0, GatlingAttack }, 
				{  5.0, PostGatlingDelivery },
				{  5.0, PlaneSwarm },
				{  7.5, HomingGatlings }, 
				{  6.5, PostGatlingDelivery },
				{  5.0, RocketBomberFirstWave },
				{  7.0, DeliverBraid },
				{  nil, util.Noop },			
				{  3.0, Interleavers },
				{ 10.0, WWWs },
				{ 15.0, RipplingSerpents },
				{ 12.0, BakerTowerGang },
				{  9.0, CCCs },
				{ 15.0, CirclesOfDoom },
				{  nil, util.Noop },
				{  5.0, BossScript }, })
end

if state.boss then
	BossScript()
else
	MasterScript()
end
util.StopMusic()
