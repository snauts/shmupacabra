dofile("script/parallax.lua")
dofile("script/level1-parallax.lua")
dofile("script/bullet.lua")
dofile("script/powerup.lua")
dofile("script/plane.lua")
dofile("script/rocket.lua")
dofile("script/cargo.lua")
dofile("script/tank.lua")
dofile("script/boss1.lua")

player.Start(false)

local size = util.GetCameraSize()
local bb = { b = -0.50 * size.y, t = -0.45 * size.y,
	     l = -0.50 * size.x, r =  0.50 * size.x }
eapi.NewShape(staticBody, nil, bb, "Street")

local function KamikazeWave1()
	plane.Kamikaze({ x = 450, y = 0 })
end

local function KamikazeWave2()
	plane.Kamikaze({ x = 464, y = 80 })
	plane.Kamikaze({ x = 464, y = -80 })
end

local function KamikazeWave3(width, gravity)
	plane.Kamikaze({ x = 496, y = width }, { x = 0, y = gravity })
	plane.Kamikaze({ x = 432, y = 0 })
	plane.Kamikaze({ x = 496, y = -width }, { x = 0, y = -gravity })
end

local function KamikazeWave3Sparse()
	KamikazeWave3(180, 0)
end

local function KamikazeWave3Dense()
	KamikazeWave3(80, 0)
end

local function KamikazeWave3Expand()
	KamikazeWave3(80, 100)
end

local function KamikazeWave3Contract()
	KamikazeWave3(180, -150)
end

local function KamikazeWaveRow()
	local flip = 1
	local offset = 0
	local count = 9
	local function EmitKamikaze()
		if count == 0 then return end
		local gravity = { x = 0, y = offset * flip }
		plane.Kamikaze({ x = 450, y = 0 }, gravity)
		eapi.AddTimer(staticBody, 0.15, EmitKamikaze)
		offset = offset + 25
		count = count - 1
		flip = -flip
	end
	EmitKamikaze()
end

local function RetreatingWave2(Pattern)
	return function()
		plane.Retreating({ x = 464, y = 80 },  0.05, Pattern)
		plane.Retreating({ x = 464, y = -80 }, 0.05, Pattern)
	end
end

local function RetreatingWave3(Pattern)
	return function()
		plane.Retreating({ x = 496, y = 180 },  0.05, Pattern)
		plane.Retreating({ x = 432, y = 0 },    0.05, Pattern)
		plane.Retreating({ x = 496, y = -180 }, 0.05, Pattern)
	end
end

local function KamikazeAttack()
	local delay = 0.7
	util.DoEventsAbsolute({ {   0 * delay, KamikazeWave1 },
				{   1 * delay, KamikazeWave2 },
				{   2 * delay, KamikazeWave3Sparse },
				{   3 * delay, KamikazeWave3Dense },
				{   4 * delay, KamikazeWave3Contract },
				{   5 * delay, KamikazeWave3Expand },
				{   6 * delay, KamikazeWaveRow },
				{   9 * delay, KamikazeWave3Sparse } })
end

local function RetreatingAttack()
	util.DoEventsAbsolute({ {  0.0, RetreatingWave2() },
				{  1.0, RetreatingWave3() },
				{  2.0, RetreatingWave2() },
				{  3.0, RetreatingWave3() },
				{  4.3, RetreatingWave2() },
				{  4.3, RetreatingWave3() },
})
end

local function KamikazeWall(height, step, count, interval, Gravity)
	local body = eapi.NewBody(gameWorld, vector.null)
	local function StartWall()
		local gravity = util.MaybeCall(Gravity, count)
		plane.Kamikaze({ x = 450, y = height }, gravity)
		height = height + step
		count = count - 1
		if count > 0 then
			eapi.AddTimer(body, interval, StartWall)
		else
			eapi.Destroy(body)
		end
	end
	return StartWall
end

local function KamikazeSpinGravity(amount)
	return function(num)
		return { x = 0, y = amount * (num - 4) }
	end
end

local KamikazeSpin1 = KamikazeWall(200, -64, 7, 0.1, KamikazeSpinGravity(-120))
local KamikazeSpin2 = KamikazeWall(-200, 64, 7, 0.1, KamikazeSpinGravity(120))

local function KamikazeMenace()
	util.DoEventsAbsolute({ { 0.0, KamikazeSpin1 },
				{ 1.0, KamikazeSpin2 }, })
end

local function EmitTransporter(pos, deliverTime, velocity)
	return function() plane.Transporter(pos, deliverTime, velocity) end
end

local function TransporterAttack()
	local pos = { x = 400, y = -50 }
	util.DoEventsAbsolute({				      
		{ 0.0, EmitTransporter(pos, 3.0, { x = -90,  y = 50 }) },
		{ 1.0, EmitTransporter(pos, 3.0, { x = -50,  y = 70 }) },
		{ 2.0, EmitTransporter(pos, 3.0, { x = -95,  y = 60 }) },
		{ 3.0, EmitTransporter(pos, 3.0, { x = -50,  y = 70 }) },
		{ 4.0, EmitTransporter(pos, 3.0, { x = -90,  y = 45 }) }, })
end

local function HeavyRain(from, to, step, starCount)
	local function Alternate()
		for x = from + step, to, 2 * step do
			rocket.Ballistic(x, starCount)
		end
	end
	return function()
		eapi.AddTimer(staticBody, 0.05, Alternate)
		for x = from, to, 2 * step do
			rocket.Ballistic(x, starCount)
		end
	end
end

local function DiagonalHeavyRain(from, to, step)
	local function Go()
		rocket.Ballistic(from)
		if (from < to) == (from + step < to) then
			eapi.AddTimer(staticBody, 0.1, Go)
			from = from + step
		end
	end
	return Go
end

local function RocketAttack()
	util.DoEventsRelative({ {  0.0, HeavyRain(-384, 384, 128) },
				{  2.5, HeavyRain(-320, 320, 128) },
				{  2.0, DiagonalHeavyRain(-384, 400, 128) },
				{  1.5, DiagonalHeavyRain(320, -320, -128) }, })
end

local function DeadlyDeliveries(Continuation)
	local function SecondDelivery()
		cargo.Ship({ x = 464, y = 100 }, Continuation)
	end
	cargo.Ship({ x = 464, y = -50 }, SecondDelivery)
end

local function StrangeUmbrella()
	local function Plane(yoffset, delay)
		return function()
			plane.Retreating({ x = 450, y = yoffset }, delay)
		end
	end
	local function TopCargo()
		cargo.ShipTop({ x = 464, y = 100 }, cargo.Scissor)
	end
	local function BottomCargo()
		cargo.ShipBottom({ x = 464, y = -50 }, cargo.Scissor)
	end
	util.DoEventsRelative({ { 0.0, TopCargo },
				{ 1.0, Plane(190, 0.05) },
				{ 1.0, BottomCargo },
				{ 1.0, Plane(20, 0.05) },
				{ 1.0, Plane(190, 0.05) },
				{ 1.0, Plane(20, 0.05) },
				{ 1.0, Plane(190, 0.05) },
				{ 0.5, HeavyRain(-384, 384, 48, 1) }, })
end

local function PlaneFormation()
	plane.Formation({ x = 432, y = 150 })
end

local function MassTransport(totalCount) -- TODO: use this somewhere
	local count = 0
	local function Repeat()
		EmitTransporter({ x = count * 64, y = -50 },
				3.5 - 0.1 * count,
				{ x = -util.Random(65, 70),
				  y =  util.Random(40, 60) })()
		if count < totalCount then
			eapi.AddTimer(staticBody, 0.01, Repeat)
			count = count + 1
		end
	end
	return Repeat
end

local function PusherPlanes(count)
	local lastY = 0
	local function Emit()
		count = count - 1
		local y = (lastY + 50 + 200 * util.Random()) % 300
		local pos = { x = 464, y = y - 100 }
		plane.Retreating(pos, 0.05, plane.WidePattern)
		lastY = y
		if count > 0 then 
			eapi.AddTimer(staticBody, 0.5, Emit)
		end
	end
	return Emit
end

local function HandshakeWithTank(Continuation)
	tank.Turret(nil, Continuation)
end

local function SecondDateWithTank(Continuation)
	tank.Turret(nil, Continuation, tank.TypeB)
end

local function InterleavedPlanes()
	util.DoEventsRelative({ {  0.0, RetreatingWave3(plane.WidePattern) },
				{  0.0, KamikazeWave2 },
				{  1.0, RetreatingWave2(plane.WidePattern) },
				{  0.0, KamikazeWave3Sparse } })
end

local function RocketClutch(x, count, spacing, interval)
	local i = 0
	count = count or 3
	spacing = spacing or 32
	interval = interval or 0.1
	local function Shoot()
		if i == 0 then
			rocket.Ballistic(x)
		else
			rocket.Ballistic(x + spacing * i)
			rocket.Ballistic(x - spacing * i)
		end
		i = i + 1
		if i < count then
			eapi.AddTimer(staticBody, interval, Shoot)
		end
	end
	return Shoot
end

local function RocketPlaneFrenzy()
	util.DoEventsRelative({ {  0.0, RocketClutch(200) },
				{  0.5, RocketClutch(-20) },
				{  1.0, RocketClutch(-240) },
				{  4.5, InterleavedPlanes },
				{  2.0, InterleavedPlanes },
				{  2.0, InterleavedPlanes }, })
end

local function BossScript()
	util.DoEventsRelative({ {  0.0, menu.Warning },
				{  2.0, bulletPooper.Tow },
				{  0.0, menu.StopMusic(4.0) },
				{  4.0, menu.BossMusic },
				{  3.0, bulletPooper.Start }, })
end

local function MasterScript()
	util.DoEventsRelative({ {  1.0, menu.Mission(1) },
				{  2.5, KamikazeAttack },
				{  7.0, RetreatingAttack },
				{  7.0, KamikazeMenace },
				{  1.0, TransporterAttack },
				{ 10.0, RocketAttack },
				{ 12.0, DeadlyDeliveries },
				{  nil, util.Noop },
				{  2.5, PlaneFormation },
				{  1.5, StrangeUmbrella },
				{ 14.0, PusherPlanes(12) },
				{  8.0, HandshakeWithTank },
				{  nil, MassTransport(6) },
				{  7.0, SecondDateWithTank },
				{  nil, util.Noop }, -- delay
				{  3.0, RocketPlaneFrenzy },
				{ 16.0, BossScript }, })
end

if state.boss then
	BossScript()
else
	MasterScript()
end
util.StopMusic()
