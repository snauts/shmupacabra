local splinterCount = 4

local explodeName = { "image/explode.png", filter = true }
local sprite = eapi.ChopImage(explodeName, { 64, 64 })
local spriteOffset = { x = -32, y = -32 }

local decal = eapi.ChopImage({ "image/decal.png", filter = true }, { 128, 64 })

local splinterName = { "image/splinter.png", filter = true }
local splinter = eapi.ChopImage(splinterName, { 64, 64 })

local size = { { x = 12, y = 12 },
	       { x = 16, y = 16 },
	       { x = 20, y = 20 } }

local offset = { { x =  -6, y =  -6 },
		 { x =  -8, y =  -8 }, 
		 { x = -10, y = -10 } }
		 
local function Splinter(parentBody, z, angle)
	local i = util.Random(1, 3)
	local x = 50 + 100 * util.Random()
	local vel = vector.Rotate({ x = x, y = 0 }, angle)
	local body = actor.CloneBody(parentBody, nil, vel)
	eapi.SetAcc(body, { x = 0, y = -300 })	
	local tile = eapi.NewTile(body, offset[i], size[i], splinter, z or 0)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, 0.25, 0.75)
	eapi.Animate(tile, eapi.ANIM_LOOP, 64, util.Random())
	util.RandomRotateTile(tile)
	return body
end

local splinterFraction =  1.0 / (splinterCount - 1)
local function Splinters(parentBody, z, startAngle, stepAngle)
	local bag = { }
	stepAngle = (stepAngle or 180) * splinterFraction
	startAngle = startAngle or 0
	for i = 0, splinterCount - 1, 1 do
		bag[i] = Splinter(parentBody, z, startAngle + i * stepAngle)
	end
	eapi.AddTimer(bag[0], 1, function() util.DestroyTable(bag) end)
end

local bangTime = 0.25
local bangSize = { x = 256, y = 256 }
local bangOffset = { x = -128, y = -128 }
local function Bang(tile, z)
	tile = eapi.Clone(tile)	
	util.SetTileAlpha(tile, 0.8)
	eapi.SetDepth(tile, z - 0.01)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, bangSize, bangTime, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, bangOffset, bangTime, 0)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, bangTime, 0)
	util.RandomRotateTile(tile)
end

local semiInvisible = { r = 1, g = 1, b = 1, a = 0.5 }
local function AnimateExplosion(tile, size, offset)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size, 1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset, 1, 0)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, semiInvisible, 1, 0)
end

local explodeSize = { x = 128, y = 128 }
local explodeOffset = { x = -64, y = -64 }
local function Simple(pos, parentBody, z, noSplinter)
	local body = actor.CloneBody(parentBody, pos)
	local tile = eapi.NewTile(body, spriteOffset, nil, sprite, z or 0)	
	eapi.Animate(tile, eapi.ANIM_CLAMP, 64, 0)
	AnimateExplosion(tile, explodeSize, explodeOffset)
	util.PlaySound(gameWorld, "sound/explode.ogg", 0.1, 0, 0.5)
	if not noSplinter then Splinters(body, z) end
	util.DelayedDestroy(body, 1)
	util.RandomRotateTile(tile)
	Bang(tile, z)
	return body
end

local smallSize = { x = 32, y = 32 }
local smallOffset = { x = -16, y = -16 }
local smallExpandSize = { x = 64, y = 64 }
local smallExpandOffset = { x = -32, y = -32 }

local function Small(pos, z)
	local speed = 0.8 + 0.2 * util.Random()
	local body = eapi.NewBody(gameWorld, pos)
	local tile = eapi.NewTile(body, smallOffset, smallSize, sprite, z or 0)
	AnimateExplosion(tile, smallExpandSize, smallExpandOffset)
	eapi.Animate(tile, eapi.ANIM_CLAMP, (1 / speed) * 64 + 1, 0)
	util.DelayedDestroy(body, speed)
	util.RandomRotateTile(tile)
	return body
end

local spacing = 16.0
local function Area(pos, parentBody, z, radius, interval)
	local body = actor.CloneBody(parentBody)
	local random = 360 * util.Random()
	local count = 0	
	z = z or 0
	local function Do()
		local distance = spacing * math.sqrt(count)
		local offset = { x = distance, y = 0 }
		offset = vector.Rotate(offset, random + count * util.fibonacci)
		Simple(vector.Floor(vector.Add(pos, offset)), body, z)
		count = count + 1
		z = z - 0.0001
		if distance <= radius then
			eapi.AddTimer(body, interval, Do)
		else
			eapi.Destroy(body)
		end
	end
	Do()
end

local function Line(pos, parentBody, z, direction, count, interval)
	local body = actor.CloneBody(parentBody)
	local function Do()
		Simple(pos, body, z)
		pos = vector.Add(pos, direction)
		count = count - 1
		if count > 0 then
			eapi.AddTimer(body, interval, Do)
		else
			eapi.Destroy(body)
		end
	end
	Do()
end

local function Decal(parentBody, offset)
	local body = actor.CloneBody(parentBody, offset)
	eapi.SetAcc(body, vector.null)
	local rand = util.Random(48, 64)
	local size = { x = 2 * rand, y = rand }
	local offset = { x = -rand, y = -rand / 2}
	local tile = eapi.NewTile(body, offset, size, decal, -9.9)
	eapi.FlipX(tile, util.Random() > 0.5)	
	util.DelayedDestroy(body, 1.2)
end

local function Circular(CreatorFn, parent, count, magnitude)
	local step = 360 / count
	local angle = step * util.Random()
	local direction = { x = magnitude or 100, y = 0 }
	for i = 1, count, 1 do
		CreatorFn(parent, vector.Rotate(direction, angle))
		angle = angle + step
	end
end

local function Continuous(obj)
	local bb = obj.bb
	local function Explode()
		local pos = { x = bb.l + util.Random(0, bb.r - bb.l),
			      y = bb.b + util.Random(0, bb.t - bb.b) }
		Simple(pos, obj.body, obj.z + 1)
		eapi.AddTimer(obj.body, 0.2 + 0.2 * util.Random(), Explode)
	end
	Explode()
end

local function Bulk(mob, size, width)
	for i = -width, width, size do
		explode.Area({ x = i, y = 0 }, mob.body, mob.z + 1, size, 0.05)
	end
end

local function Flame(body, vel, z)
	z = z or 0
	local img = player.flame
	local angle = 180 + vector.Angle(vel)
	local offset = { x = -32, y = -32 }
	local size = { x = 64, y = 64 }
	local animOffset = { x = -1, y = -1 }
	local animSize = { x = 2, y = 2 }
	local blackness = { r = 0.4, b = 0.4, g = 0.4, a = 0 }
	local halfInvisible = { r = 1, b = 1, g = 1, a = 0.5 }
	local function Emit()
		local pos = vector.Rnd(eapi.GetPos(body, gameWorld), 8)
		local flameBody = eapi.NewBody(gameWorld, pos)
		local tile = eapi.NewTile(flameBody, offset, size, img, z)
		eapi.SetColor(tile, halfInvisible)
 		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, blackness, 1, 0)
		eapi.AnimatePos(tile, eapi.ANIM_CLAMP, animOffset, 1, 0)
		eapi.AnimateSize(tile, eapi.ANIM_CLAMP, animSize, 1, 0)
		util.RotateTile(tile, angle)
		eapi.SetVel(flameBody, vel)
		util.DelayedDestroy(flameBody, 1)
		eapi.AddTimer(body, 0.05, Emit)
	end
	Emit()
end

local smokeFile = { "image/smoke.png", filter = true }
local smokeImg = eapi.ChopImage(smokeFile, { 64, 64 })

local black = { r = 0.0, g = 0.0, b = 0.0, a = 0.5 }
local blackFade = { r = 0.0, g = 0.0, b = 0.0, a = 0.0 }

local function Smoke(obj, start, interval, velocity, jitter,
		     z, PosFN, sSize, sPos, size, offset)
		     
	local function Puff()
		eapi.AddTimer(obj.body, interval, Puff)
		local body = eapi.NewBody(gameWorld, PosFN())
		local tile = eapi.NewTile(body, sPos, sSize, smokeImg, z)
		eapi.SetColor(tile, black)
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, blackFade, 1, 0)
		eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size, 1, 0)
		eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset, 1, 0)
		eapi.Animate(tile, eapi.ANIM_CLAMP, 32, util.Random())
		eapi.SetAcc(body, vector.Rnd(vector.null, jitter))
		eapi.SetVel(body, velocity)
		util.RandomRotateTile(tile)
		util.DelayedDestroy(body, 1)
	end
	
	eapi.AddTimer(obj.body, start, Puff)
end

local function Tail(body, interval, noise, z)
	local function Emit()
		eapi.AddTimer(body, interval, Emit)
		local jitter = vector.Rnd(vector.null, noise)
		local explosionBody = explode.Simple(jitter, body, z, true)
		eapi.SetAcc(explosionBody, vector.null)
		eapi.SetVel(explosionBody, vector.null)
	end
	Emit()
end

explode = {
	Continuous = Continuous,
	Splinters = Splinters,
	Circular = Circular,
	Simple = Simple,
	Decal = Decal,
	Small = Small,
	Flame = Flame,
	Smoke = Smoke,
	Tail = Tail,
	Line = Line,
	Area = Area,
	Bulk = Bulk,
}
return explode
