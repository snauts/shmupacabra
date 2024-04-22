dofile("script/parallax.lua")
dofile("script/level3-parallax.lua")
dofile("script/player.lua")
dofile("script/powerup.lua")
dofile("script/bullet.lua")
dofile("script/cannon.lua")
dofile("script/serpent.lua")
dofile("script/plane.lua")
dofile("script/magnetic.lua")
dofile("script/flower.lua")
dofile("script/rings.lua")
dofile("script/heat.lua")
dofile("script/train.lua")
dofile("script/cargo.lua")
dofile("script/tank.lua")
dofile("script/broken.lua")
dofile("script/core.lua")

player.Start(true)

local function WaveOfCirclers()
	local aimOffset = { x = -128, y = 0 }
	local function Cross(obj)
		local pos = actor.GetPos(obj)
		local aimPos = vector.Add(player.Pos(), aimOffset)
		local vel = bullet.HomingVector(aimPos, pos)
		local center = bullet.Cyan(pos, vel)
		local offset = { x = 12, y = 0 }

		for i = 0, 3, 1 do
			local childPos = vector.Add(pos, offset)
			local child = bullet.Cyan(childPos, vector.null)
			actor.Link(child, center)
			eapi.SetStepC(child.body, eapi.STEPFUNC_ROT, 12)
			offset = vector.Rotate(offset, 90)
		end
		aimOffset = vector.Rotate(aimOffset, 60)
	end
	for i = -45, 45, 5 do
		local vel = vector.Rotate(plane.velocity, i)
		eapi.AddTimer(staticBody, 0.03 * (i + 45), function()
			plane.Circler({ x = 432, y = i * 4}, vel, Cross)
		end)
	end
end

local function TemporalEnding()
	local function Text(str, offset)
		local pos = util.TextCenter(str, util.defaultFontset)
		pos = vector.Offset(pos, 0, offset)
		util.PrintOrange(pos, str, nil, nil, 0)
	end		
	Text("You are awsome, thank you for playing!", 8)
	Text("Please check back later for more action.", -8)
	menu.Box({ x = 400, y = 64 }, { x = -200, y = -32 })
end

local function ThreeTightVWings()
	local vel = { x = -150, y = 0 }
	local function TightVWing(pos)
		return util.Closure(cannon.TightVWing, pos, vel)
	end
	util.DoEventsRelative({ { 0.0, TightVWing({ x = 464, y = 150 }) },
				{ 0.0, TightVWing({ x = 464, y = -150 }) },
				{ 1.0, TightVWing({ x = 464, y = 0 }) } })
end

local function TwoSparseVWings()
	local vel = { x = -150, y = 0 }
	cannon.SparseVWing({ x = 464, y = 100 }, vel)
	cannon.SparseVWing({ x = 464, y = -100 }, vel)
end

local function FlipingVWings()
	local baseVel = { x = -150, y = 0 }
	local function Launch(pos, angle, dir)
		return function()
			local vel = vector.Rotate(baseVel, angle)
			cannon.FlipingVWing(pos, vel, dir)
		end
	end
	util.DoEventsRelative({
		{ 0.0, Launch({ x = 464, y = 200 }, 20, 1) },
		{ 1.5, Launch({ x = 464, y = -200 }, -20, -1) },
	})
end


local function Zippers(dir)
	local x = 0
	local function Emit()
		local pos = { x = 400 - x, y = dir * (300 + x * 0.15) }
		local vel = { x = -100 + x, y = -dir * 300 }		
		plane.Leaver(pos, vel, vector.null)
		x = x + 50
	end
	return util.Closure(util.Repeater, Emit, 9, 0.15)
end

local function Crawler(pos, vel)
	return function() serpent.Crawl(pos, vel) end
end
local function Crawlers()
	util.DoEventsRelative({ 
		{ 0.0, Crawler({ x = 464, y =  250 }, { x = -150, y = -150 }) },
		{ 3.0, Crawler({ x = 464, y = -250 }, { x = -150, y =  150 }) },
	})
end

local function MagneticBombs()
	local vel = { x = -200, y = 0 }
	local function P(h)
		return { x = 432, y = h }
	end
	local function Bomb(h)
		return util.Closure(magnetic.Bomb, P(h), vel)
	end
	local planeH = -200
	local function Plane()
		plane.Retreating(P(planeH), 0.1, plane.ThreadPattern)
		planeH = planeH + 100
	end
	util.Repeater(Plane, 5, 0.5)
	util.DoEventsRelative({ { 0.00, Bomb(0) },
				{ 0.45, Bomb(150) },
				{ 1.50, Bomb(-150) }, })
end

local function MoveDustDown()
	local count = 2
	local New = util.Noop
	local function Move(yVel)
		return function(p)
			local vel = eapi.GetVel(p.body)
			eapi.SetVel(p.body, { x = vel.x, y = yVel })
		end
	end
	
	local function FinalStop()
		util.Map(Move(0), parallax.pTop)
		util.Map(Move(0), parallax.pBottom)
		util.Map(parallax.Delete, parallax.pTop)
		parallax.pTop = parallax.pBottom
	end

	local function Stop()
		util.Map(parallax.Delete, parallax.pTop)
		parallax.pTop = parallax.pBottom
		New()
	end

	local function LastStop()
		if count == 0 then
			eapi.AddTimer(staticBody, 3, parallax.MoveDeckUp)
		end
		return FinalStop
	end

	New = function()
		count = count - 1		
		parallax.pBottom = { }
		parallax.Dust(parallax.pBottom, -720)	
		util.Map(Move(120), parallax.pBottom)
		local StopFn = (count == 0) and LastStop() or Stop
		eapi.AddTimer(staticBody, 4, StopFn)
	end

	util.Map(Move(120), parallax.pTop)
	New()
end

local function DiagonalBombs()
	local vel = { x = -120, y = 120 }
	local function Bomb(x, y, delay)
		local pos = { x = x, y = y }
		return util.Closure(magnetic.Bomb, pos, vel, delay)
	end
	util.DoEventsRelative({ { 0.0, Bomb(300, -273, 2.0) },
				{ 0.5, Bomb(400, -273, 1.8) },
				{ 0.7, Bomb(432, -150, 1.9) },
				{ 0.8, Bomb(300, -273, 1.8) },
				{ 0.5, Bomb(400, -273, 2.0) },})
end

local function SuperJets(count)
	local h = 0
	local delay = 1.0
	local function Fn()
		h = (h + util.Random(120, 320)) % 440
		local pos = { x = 432, y = h - 200 }
		local aim = bullet.HomingVector(player.Pos(), pos)
		plane.SuperJet(pos, vector.Normalize(aim, 800))		
		delay = math.max(0.25, delay - 0.05)
		if count > 0 then
			eapi.AddTimer(staticBody, delay, Fn)
			count = count - 1
		end
	end
	return Fn
end

local function LaunchFlowers(spd)
	local function Flower(h, angle)
		return function()
			local vel = vector.Rotate({ x = -100, y = 0 }, angle)
			flower.Put({ x = 464, y = h }, vel, angle + 90, spd, 1)
		end
	end
	return function()
		util.DoEventsRelative({ {  0.0, Flower(200, 20) }, 
					{  1.0, Flower(-200, -20) },
					{  1.0, Flower(200, 20) }, 
					{  1.0, Flower(-200, -20) },
					{  1.0, Flower(200, 20) }, 
					{  1.0, Flower(-200, -20) }, })
	end
end

local function CannonArmada()
	local h = 200
	local q = cannon.interval
	local vel = { x = -100, y = 0 }
	local function DropCannon()
		cannon.Simple({ x = 448, y = h - 200 }, vel, 256, q)
		h = (h + util.golden * 400) % 400
		q = (q - 0.1) % cannon.interval
	end
	util.Repeater(DropCannon, 50, 0.1)
end

local function JumpingFlowers()
	train.Put()
	local jumpTime = 0.5
	local function Add(delay, pos)
		local function Jumping()
			flower.Jump(pos, jumpTime)
			jumpTime = jumpTime + 0.2
		end
		eapi.AddTimer(staticBody, delay, Jumping)
	end
	for i = 0.0, 2.4, 0.4 do 
		Add(0.3 + i, { x = 464, y = -100 })
		Add(0.5 + i, { x = 464, y = -110 })
	end
end

local function StandingTank(pos, Pattern, angle)
	tank.Turret(pos, nil, Pattern, true, train.velocity, 130 - angle, 100)
end

local function TransportingTanks()
	train.Put()
	local delay = 0.4
	local function Tank(angle, spiral)
		return function()
			local Pattern = tank.TypeC(180 - angle, spiral)
			StandingTank({ x = 496, y = -108 }, Pattern, angle)
		end
	end
	eapi.AddTimer(staticBody, 0.0 + delay, Tank(10, -1))
	eapi.AddTimer(staticBody, 1.0 + delay, Tank(40, 1))
	eapi.AddTimer(staticBody, 2.0 + delay, Tank(50, 1))
end

local function CaravanOfCargos()
	train.Put()
	local function PutCargo()
		cargo.Transported({ x = 464, y = -86 })
	end
	for i = 0, 2, 1 do
		eapi.AddTimer(staticBody, i + 0.4, PutCargo)
	end
end

local function ServeChestnuts()
	train.Put()
	local function Add(delay, pos, tint, jumpTime)
		local function Jumping()
			magnetic.Chestnut(pos, jumpTime - delay, tint)
		end
		eapi.AddTimer(staticBody, delay, Jumping)
	end
	for i = 0.0, 2.4, 0.4 do 
		local jump = 2.4 + math.abs(i - 1.2)		
		Add(0.3 + i, { x = 464, y = -115 }, 0.7, jump)
		Add(0.5 + i, { x = 464, y = -125 }, 1.0, jump + 0.2)
	end
end

local function DestroyTracks()
	magnetic.Flaming({ x = 400, y = 300 }, { x = -400, y = -492 })
end

local function BossScript()
	util.DoEventsRelative({ {  0.0, menu.FadingWarning },
				{  4.0, menu.BossMusic },
				{  0.0, analDestructor.Launch }, })
end

local function MasterScript()
	util.DoEventsRelative({ {  1.0, menu.Mission(3) }, 
				{  2.5, WaveOfCirclers },
				{  4.0, ThreeTightVWings },
				{  3.5, Zippers(1) },
				{  2.5, TwoSparseVWings },
				{  3.0, Zippers(-1) },
				{  2.2, FlipingVWings },
				{  3.0, Crawlers },
				{  8.0, MagneticBombs },
				{  8.0, MoveDustDown },
				{  0.1, DiagonalBombs },
				{  8.0, LaunchFlowers(0) },
				{ 10.0, SuperJets(25) },
				{ 12.0, LaunchFlowers(2) },
				{  9.5, CannonArmada },
				{ 10.0, rings.Launch }, 
				{  nil, util.Noop },
				{  1.0, parallax.Wall },
				{  2.0, parallax.Rails(2240) },
				{  1.0, TransportingTanks },
				{  5.0, CaravanOfCargos },
				{  5.0, JumpingFlowers },
				{  6.0, ServeChestnuts },
				{  4.0, broken.Boss1 },
				{ 10.0, broken.Boss2 },
				{ 18.0, DestroyTracks },
				{  2.0, menu.StopMusic(2.0) },
				{  2.0, BossScript }, })
end

if state.boss then
	parallax.AddDeckParallax(0)
	parallax.MotherShipInterior()
	BossScript()
else
	MasterScript()
end
util.StopMusic()
