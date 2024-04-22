local rocketFile = { "image/rocket.png", filter = true }
local rocketImg = eapi.ChopImage(rocketFile, { 32, 64 })

local function RocketExplode(mob)
	local vel = { x = 0, y = 300 }
	if eapi.GetVel(mob.body).y < 0 then
		vel = vector.Scale(vel, -1)
	end
	explode.Line(vector.null, mob.body, 10, { x = 0, y = 24 }, 3, 0.05)	
	bullet.Arc(actor.GetPos(mob), vel, powerup.Star, mob.starCount, 30)
	actor.Delete(mob)
end

local function RocketAttributes(tile, x)
	eapi.FlipY(tile, true)
	eapi.SetColor(tile, util.Gray(x))
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, 0)
end

local shapes = { }
local function RocketCleanup(obj)
	if obj.touch then 
		shapes[obj.touch] = nil
	end
end

local function RocketVsMob(rShape, mShape)
	player.DamageMob(actor.store[mShape], 3)
	local rocket = shapes[rShape]
	rocket.Shoot(rocket)
end

actor.SimpleCollide("Rocket", "Mob", RocketVsMob)

local function Ballistic(x, starCount)
	local init = { health = 3,
		       class = "Mob",
		       sprite = rocketImg,
		       starCount = starCount or 2,
		       offset = { x = -8, y = -16 },
		       spriteSize = { x = 16, y = 32 },
		       decalOffset = { x = 0, y = -10 },
		       velocity = { x = 0, y = 500 },
		       OnDelete = RocketCleanup,
		       pos = { x = x, y = -200 },
		       Shoot = RocketExplode,
		       z = -25.0, }
	local obj = actor.Create(init)
	eapi.SetAcc(obj.body, { x = 0, y = -250 })
	RocketAttributes(obj.tile, 0.4)
	
	local size = { x = 24, y = 24 }
	local offset = { x = -36, y = -12 }
	local tile = eapi.NewTile(obj.body, offset, size, player.flame, -25.5)
	RocketAttributes(tile, 0.4)
	util.RotateTile(tile, 90)

	local function Reverse()
		obj.z = 2.0
		obj.spriteSize = nil
		obj.offset = { x = -16, y = -32 }
		actor.SwapTile(obj, rocketImg, eapi.ANIM_LOOP, 32)
		obj.bb = { b = -24, t = 24, l = -8, r = 8 }
		obj.touch = eapi.NewShape(obj.body, nil, obj.bb, "Rocket")
		shapes[obj.touch] = obj
		actor.MakeShape(obj)
		eapi.Destroy(tile)
	end
	eapi.AddTimer(obj.body, 2.0, Reverse)
end

local smokeFile = { "image/smoke.png", filter = true }
local smokeImg = eapi.ChopImage(smokeFile, { 64, 64 })

local size = { x = 48, y = 48 }
local offset = { x = -24, y = -24 }
local smallSize = { x = 4, y = 4 }
local smallOffset = { x = -2, y = -2 }
local black = { r = 0.0, g = 0.0, b = 0.0, a = 0.5 }
local blackFade = { r = 0.0, g = 0.0, b = 0.0, a = 0.0 }

local function Puff(pos)
	local body = eapi.NewBody(gameWorld, pos)
	local tile = eapi.NewTile(body, smallOffset, smallSize, smokeImg, -1)

	eapi.SetColor(tile, black)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, blackFade, 1, 0)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size, 1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset, 1, 0)
	eapi.Animate(tile, eapi.ANIM_CLAMP, 32, util.Random())
	eapi.SetAcc(body, vector.Rnd(vector.null, 32))
	util.RandomRotateTile(tile)
	util.DelayedDestroy(body, 1)
end

local function AngledExplode(mob)	
	local vel = eapi.GetVel(mob.body)
	local explodePos = vector.Normalize(vel, 24)
	local explodeStep = vector.Scale(explodePos, -1)
	explode.Line(explodePos, mob.body, 10, explodeStep, 3, 0.05)	
	bullet.Arc(actor.GetPos(mob), vel, powerup.Star, mob.starCount, 30)
	actor.Delete(mob)
end

local function RocketShape(obj, pos)
	actor.MakeShape(obj, actor.BoxAtPos(pos, 8))
end

local shapeOffset = { x = 16, y = 0 }
local function Launch(pos, vel, acc, starCount, z)
	local init = { class = "Mob",
		       sprite = rocketImg,
		       starCount = starCount or 2,
		       offset = { x = -16, y = -32 },
		       spriteSize = { x = 32, y = 64 },
		       Shoot = AngledExplode,
		       velocity = vel,
		       health = 3,
		       pos = pos,
		       z = z or 2.0, }
	local obj = actor.Create(init)
	local tileAngle = vector.Angle(acc)
	util.RotateTile(obj.tile, tileAngle - 90)
	RocketAttributes(obj.tile, 1.0)
	eapi.SetAcc(obj.body, acc)

	RocketShape(obj, vector.null)
	RocketShape(obj, vector.Rotate(shapeOffset, tileAngle))
	RocketShape(obj, vector.Rotate(shapeOffset, tileAngle - 180))

	explode.Smoke(obj, 0.2, 0.02, vector.null, 32, -1, 
		      function() return actor.GetPos(obj) end,
		      { x = 4, y = 4 }, { x = -2, y = -2 },
		      { x = 48, y = 48 }, { x = -24, y = -24 })
end

local function Smoking(pos, vel, starCount)
	local init = { sprite = rocketImg,
		       offset = { x = -8, y = -16 },
		       spriteSize = { x = 16, y = 32 },
		       velocity = vel,
		       pos = pos,
		       z = -25.0, }
	local obj = actor.Create(init)
	local tileAngle = vector.Angle(vel)
	eapi.SetAcc(obj.body, vector.Scale(vel, -0.5))
	util.RotateTile(obj.tile, tileAngle - 90)
	RocketAttributes(obj.tile, 0.7)
	
	local size = { x = 24, y = 24 }
	local offset = { x = -36, y = -12 }
	local tile = eapi.NewTile(obj.body, offset, size, player.flame, -25.5)
	util.RotateTile(tile, tileAngle)
	RocketAttributes(tile, 0.7)

	local function Reverse()
		local acc = { x = 0.5 * vel.x, y = -0.5 * vel.y }
		Launch(actor.GetPos(obj), vector.null, acc, starCount)
		actor.Delete(obj)
	end
	eapi.AddTimer(obj.body, 2.0, Reverse)	
end

rocket = {
	Launch = Launch,
	Smoking = Smoking,
	Ballistic = Ballistic,
	flameImg = flameImg,
	img = rocketImg,
}
return rocket
