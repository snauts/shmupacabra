local slowVel = { x = -100, y = 0 }

local boss1BodyFile = { "image/boss1-done.png", filter = true }
local boss1BodyImg = eapi.ChopImage(boss1BodyFile, { 320, 384 })

local function Boss1Shadow(obj)
	local z = obj.z - 0.1
	local size = { x = 384, y = 32 }
	local pos =  { x = -192, y = -156 }
	local tile = eapi.NewTile(obj.body, pos, size, util.radial, z)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 1 })
end

local function Boss1Shapes(obj)
	actor.MakeShape(obj, { b = -140, t = -64, l = -128, r = 128 })
	actor.MakeShape(obj, { b = -64, t = 0, l = -112, r = 112 })
	actor.MakeShape(obj, { b = 0, t = 32, l = -96, r = 80 })
	actor.MakeShape(obj, { b = 32, t = 64, l = -80, r = 64 })
	actor.MakeShape(obj, { b = 64, t = 80, l = -48, r = 32 })
	actor.MakeShape(obj, { b = 80, t = 100, l = -32, r = 16 })
	actor.MakeShape(obj, { b = 100, t = 120, l = -16, r = 0 })
end

local function Boss1Remove(obj)
	actor.DeleteShape(obj)
	actor.MakeShape(obj, { b = -8, t = 8, l = -8, r = 8 })
end

local wheelPos = {
	{ x = 70, y = -35 },
	{ x = 112, y = -40 },
	{ x = 138, y = -75 },
	{ x = 132, y = -116 },
	{ x = 98, y = -134 },
	{ x = 55, y = -130 },
	{ x = 30, y = -100 },
	{ x = 35, y = -59 },
}

local function Boss1Wheel(obj)
	local gravity = { x = 0, y = -150 }
	local interval = 0.01
	local counter = 0
	local total = 600
	local angle = 160
	local function Emit()
		local base = actor.GetPos(obj)
		local index = (counter % #wheelPos) + 1
		local pos = vector.Add(base, wheelPos[index])
		local projectile = bullet.Magenta(pos, slowVel)
		local function Fire()
			local rnd = util.Random(-20, 20)
			local vel = vector.Rotate(bullet.velocity, angle + rnd)
			eapi.SetVel(projectile.body, vector.Add(vel, slowVel))
			eapi.SetAcc(projectile.body, gravity)
			angle = angle - (140 / total)
		end
		eapi.AddTimer(projectile.body, 5 * interval, Fire)
		if counter < total then
			eapi.AddTimer(obj.body, interval, Emit)
			counter = counter + 1
		end
	end
	Emit()
end

local bulletPos = {
	{ x = -82, y = 35 },
	{ x = -70, y = 35 },
	{ x = -22, y = 32 },
	{ x = -10, y = 28 },
	{ x =   2, y = 24 },
	{ x =  52, y = 34 },
	{ x =  64, y = 36 },
	{ x = -76, y = 45 },
	{ x = -64, y = 45 },
	{ x = -14, y = 40 },
	{ x =  -2, y = 36 },
	{ x =  44, y = 44 },
	{ x =  56, y = 46 },
	{ x = -68, y = 55 },
	{ x = -56, y = 55 },
	{ x = -18, y = 52 },
	{ x =  -6, y = 48 },
	{ x =   6, y = 44 },
	{ x =  36, y = 54 },
	{ x =  48, y = 56 },
	{ x = -10, y = 60 },
	{ x =   2, y = 56 },
	{ x = -33, y = 84 },
	{ x =  12, y = 84 },
	{ x = -28, y = 94 },
	{ x = -16, y = 94 },
	{ x =  -4, y = 94 },
	{ x =   8, y = 94 },
	{ x = -22, y = 104 },
	{ x = -10, y = 104 },
	{ x =   2, y = 104 },
	{ x = -16, y = 114 },
	{ x =  -4, y = 114 },
}

local bulletStore = { }

local function LaunchBullets(obj)
	if not obj.destroyed then
		local vel = vector.Rotate(obj.origin, 85)
		eapi.SetVel(obj.body, vector.Scale(vel, 2))
	end
end

local Boss1Pattern

local function Boss1Jerk(obj)
	eapi.Animate(obj.tile, eapi.ANIM_CLAMP, obj.dir * 16, 0)
	util.Map(LaunchBullets, bulletStore)
	obj.dir = -obj.dir

	if obj.counter > 0 then 
		util.Delay(obj.body, 0.25, Boss1Pattern, obj)
		obj.counter = obj.counter - 1
	end
end

Boss1Pattern = function(obj)
	local index = 1
	bulletStore = { }
	local function Bullet()
		local pos = bulletPos[index]
		if pos then 
			local basePos = actor.GetPos(obj)
			local realPos = vector.Add(basePos, pos)
			local projectile = bullet.Cyan(realPos, slowVel)
			bulletStore[projectile] = projectile
			eapi.AddTimer(obj.body, 0.02, Bullet)
			projectile.origin = pos
			index = index + 1
		else
			Boss1Jerk(obj)
		end
	end
	Bullet()
end

local function Boss1Start()
	local init = { health = 1000000,
		       class = "Mob",		       
		       sprite = boss1BodyImg,
		       offset = { x = -160, y = -192 },
		       velocity = slowVel,
		       Shoot = util.Noop,
		       pos = { x = 400 + 256, y = -8 },
		       counter = 5,
		       jerk = 8,
		       dir = 1,
		       z = -1, }
	local obj = actor.Create(init)
	util.RotateTile(obj.tile, 88)
	eapi.FlipX(obj.tile, true)

	for i = 1, 3, 1 do Boss1Shadow(obj) end
	util.Delay(obj.body, 1.5, Boss1Shapes, obj)
	util.Delay(obj.body, 3.5, Boss1Pattern, obj)
	util.Delay(obj.body, 4.5, Boss1Wheel, obj)
	util.Delay(obj.body, 11.5, Boss1Remove, obj)

	local function Pos()
		local x = util.Random(-64, 64)
		return vector.Offset(actor.GetPos(obj), x, 32)
	end		
	explode.Smoke(obj, 0.0, 0.01, { x = 0, y = 150 }, 512, -2, Pos,
		      { x = 64, y = 64 }, { x = -32, y = -32 },
		      { x = 128, y = 128 }, { x = -64, y = -64 })
end

local function Boss1()
	local obj = train.Put(slowVel, 10)
	eapi.AddTimer(obj.body, 1.5, Boss1Start)
end

local frame = { { 0, 224 }, { 256, 160 } }
local boss2File = { "image/boss2-misc.png", filter = true }
local boss2BodyImg = eapi.NewSpriteList(boss2File, frame)

local doorFile = { "image/boss2-doors.png", filter = true }
local doorImg1 = eapi.ChopImage(doorFile, { 256, 80 }, 28, 1)
local doorImg2 = eapi.ChopImage(doorFile, { 256, 80 }, 32, 1)

local sheetFile = { "image/boss2-sheet.png", filter = true }
local sheetImg = eapi.ChopImage(sheetFile, { 64, 64 })

local ribFile = { "image/boss2-rib.png", filter = true }
local ribImg = eapi.ChopImage(ribFile, { 192, 64 })

local eyeOpenFile = { "image/boss2-open.png", filter = true }
local eyeOpenImg = eapi.ChopImage(eyeOpenFile, { 88, 88 })

local function TransformBoss2Tile(tile)
	eapi.SetColor(tile, util.Gray(0.5))
	util.RotateTile(tile, -12)
	eapi.FlipX(tile, true)
end

local function Boss2Door(obj)
	local offset = { x = -80, y = -80 }
	local tile = eapi.NewTile(obj.body, offset, nil, doorImg1, 0)
	TransformBoss2Tile(tile)
end

local function Boss2Shadow(obj)
	local z = obj.z - 0.1
	local size = { x = 256, y = 32 }
	local pos =  { x = -96, y = -64 }
	local tile = eapi.NewTile(obj.body, pos, size, util.radial, z)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 1 })
end

local function Boss2Rib(obj, pos, angle, anim, z)
	local offset = { x = 0, y = -32 }
	local body = eapi.NewBody(obj.body, pos)
	local tile = eapi.NewTile(body, offset, nil, ribImg, z)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, anim)
	eapi.SetColor(tile, util.Gray(0.5))
	util.RotateTile(tile, angle)
end

local function AfterBurnerPattern(obj)
	local angle = 0
	local flip = true
	local baseVel = vector.Rotate(bullet.velocity, 10)
	local baseArm = vector.Scale(baseVel, 0.2)
	local basePos = vector.Sub({ x = 175, y = 2 }, baseArm)
	local function Emit()
		angle = ((angle + 15 + (30 * util.golden)) % 30) - 15
		local pos = vector.Rotate(baseArm, 2 * angle)
		pos = vector.Add(basePos, pos)
		pos = vector.Add(actor.GetPos(obj), pos)
		local Fn = flip and bullet.Magenta or bullet.CyanBig
		local p = Fn(pos, vector.Rotate(baseVel, angle))
		eapi.SetAcc(p.body, vector.Rotate({ x = -100, y = 0 }, angle))
		eapi.AddTimer(obj.body, 0.01, Emit)
		flip = not flip
	end
	Emit()
end

local eyeCenter = vector.Rotate({ x = 32, y = 0 }, 180 - 12)

local function EyePos(obj)
	return vector.Add(actor.GetPos(obj), eyeCenter)
end

local function EyeSlam(obj)
	local function Slam()
		local angle = 0
		local pos = EyePos(obj)
		eapi.PlaySound(gameWorld, "sound/spark.ogg")
		while angle < 360 do
			local vel = { x = 0, y = -util.Random(200, 300) }
			bullet.CyanLong(pos, vector.Rotate(vel, angle))
			angle = angle + util.Random(5, 10)
		end
	end
	eapi.Animate(obj.eye, eapi.ANIM_CLAMP, -128, 0)
	eapi.AddTimer(obj.body, 0.25, Slam)
end

local spiralCount = 12
local initialAngle = 20
local spiralInterval = 1 / spiralCount
local spiralStep = (180 + 2 * initialAngle) / spiralCount

local function EyePattern(obj)
	local count = spiralCount
	local vel = vector.Rotate(bullet.velocity, -initialAngle)
	local function Emit()
		bullet.Planetoid(EyePos(obj), vel)
		vel = vector.Rotate(vel, spiralStep)
		if count > 0 then
			eapi.AddTimer(obj.body, spiralInterval, Emit)
			count = count - 1
		end
	end
	Emit()
	eapi.Animate(obj.eye, eapi.ANIM_CLAMP, 32, 0)
	util.Delay(obj.body, 1, EyeSlam, obj)
end

local function Boss2Eye(obj)
	local offset = { x = -70, y = -44 }
	local tile = eapi.NewTile(obj.body, offset, nil, eyeOpenImg, -0.9)
	TransformBoss2Tile(tile)
	obj.eye = tile
end

local function Boss2Start()
	local init = { health = 1000000,
		       class = "Mob",		       
		       sprite = boss2BodyImg,
		       offset = { x = -80, y = -80 },
		       bb = { b = -52, t = 44, l = -56, r = 16 },
		       velocity = slowVel,
		       Shoot = util.Noop,
		       pos = { x = 500, y = -95 },
		       blinkZ = 0.2,
		       jerk = 8,
		       z = -1, }
	local obj = actor.Create(init)
	actor.MakeShape(obj, { b = -52, t = 0, l = 16, r = 128 })
	actor.MakeShape(obj, { b = 0, t = 32, l = 16, r = 48 })
	Boss2Rib(obj, { x = 160, y = 20 }, 160, 0.000, -1.5)
	Boss2Rib(obj, { x = 178, y = 25 }, 150, 0.333, -1.6)
	Boss2Rib(obj, { x = 200, y = 30 }, 140, 0.666, -1.7)
	for i = 1, 2, 1 do Boss2Shadow(obj) end
	TransformBoss2Tile(obj.tile)
	Boss2Door(obj)
	Boss2Eye(obj)

	for i = 0, 4, 1.5 do 
		util.Delay(obj.body, 2 + i, EyePattern, obj)
	end
	
	util.Delay(obj.body, 4, AfterBurnerPattern, obj)

	local function Pos()
		local x = util.Random(24, 96)
		return vector.Offset(actor.GetPos(obj), x, -32)
	end		
	explode.Smoke(obj, 0.0, 0.01, { x = 0, y = 100 }, 512, -0.5, Pos,
		      { x = 32, y = 32 }, { x = -16, y = -16 },
		      { x = 64, y = 64 }, { x = -32, y = -32 })
end

local function Boss2LooseDoor(obj)
	local offset = { x = 448, y = 96 }
	local tile = eapi.NewTile(obj.body, offset, nil, doorImg2, 0)
	eapi.SetColor(tile, util.Gray(0.4))
	util.RotateTile(tile, -5)
end

local sheetTable = { { 0, 24 }, { -45, 20 }, { -155, 6 } }

local function Boss2Sheet(obj, pos)
	local offset = { x = -32, y = -32 }
	local body = eapi.NewBody(obj.body, pos)
	local sheet = util.RandomElement(sheetTable)
	local tile = eapi.NewTile(body, offset, nil, sheetImg, -2)
	local angle = sheet[1] + util.Random(-5, 5)
	eapi.SetColor(tile, util.Gray(0.5))
	eapi.SetFrame(tile, sheet[2])

	local speed = 0.1
	util.RotateTile(tile, angle)
	angle = vector.Radians(angle + 3)
	eapi.AnimateAngle(tile, eapi.ANIM_REVERSE_LOOP, vector.null,
			  angle, speed, 2 * speed * util.Random())
end

local function AddBoss2Sheets(obj)
	eapi.RandomSeed(111)
	for i = 1, 25, 1 do
		Boss2Sheet(obj, { x = util.Random(128, 480),
				  y = util.Random(68, 85) })
	end
end

local function Boss2()
	local obj = train.Put(slowVel, 10)
	eapi.AddTimer(obj.body, 2, Boss2Start)
	AddBoss2Sheets(obj)
	Boss2LooseDoor(obj)
end

broken = {
	Boss1 = Boss1,
	Boss2 = Boss2,
}
return broken
