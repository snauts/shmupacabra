local bulletSpeed = 250
local bulletVelocity = { x = bulletSpeed, y = 0 }

local function Explode(bullet)
	eapi.Animate(bullet.tile, eapi.ANIM_CLAMP, 64, 0)
	actor.DelayedDelete(bullet, 0.25)
	bullet.exploding = true
end

local bigSize = { x = 64, y = 64 }
local spinSize = { x = 64, y = 32 }
local longSize = { x = 48, y = 24 }
local defaultSize = { x = 32, y = 32 }
local function SimpleBullet(pos, velocity, sprite, size)
	size = size or defaultSize
	local init = { sprite = sprite,
		       class = "Bullet",
		       spriteSize = size,
		       offset = vector.Scale(size, -0.5),
		       bb = actor.Square(0.0625 * (size.x + size.y)),
		       velocity = velocity,
		       Explode = Explode,
		       pos = pos,
		       z = 20 + bullet.z_epsilon, }
	bullet.z_epsilon = bullet.z_epsilon + 0.0001
	local bullet = actor.Create(init)
	return bullet
end

local function EmitBullet(pos, velocity, sprite, size)
	local bullet = SimpleBullet(pos, velocity, sprite, size)
	util.AnimateRotation(bullet.tile, 0.1)
	return bullet
end

local animSize = { x = 56, y = 28 }
local animPos = vector.Scale(animSize, -0.5)
local function LongBullet(pos, velocity, sprite)
	local bullet = SimpleBullet(pos, velocity, sprite, longSize)
	util.RotateTile(bullet.tile, vector.Angle(velocity))
	eapi.AnimateSize(bullet.tile, eapi.ANIM_REVERSE_LOOP, animSize, 0.05, 0)
	eapi.AnimatePos(bullet.tile, eapi.ANIM_REVERSE_LOOP, animPos, 0.05, 0)
	return bullet
end

local function LoadBullet(file)	
	return eapi.ChopImage({ file, filter = true }, { 64, 64 })
end

local cyan = LoadBullet("image/cyan-bullet.png")
local function Cyan(pos, velocity)
	return EmitBullet(pos, velocity, cyan)
end
local function CyanBig(pos, velocity)
	return EmitBullet(pos, velocity, cyan, bigSize)
end
local function CyanSpin(pos, velocity)
	return EmitBullet(pos, velocity, cyan, spinSize)
end
local function CyanLong(pos, velocity)
	return LongBullet(pos, velocity, cyan)
end
local function CyanCustom(pos, velocity, scale)
	return EmitBullet(pos, velocity, cyan, vector.Scale(bigSize, scale))
end

local magenta = LoadBullet("image/magenta-bullet.png")
local function Magenta(pos, velocity)
	return EmitBullet(pos, velocity, magenta)
end
local function MagentaBig(pos, velocity)
	return EmitBullet(pos, velocity, magenta, bigSize)
end
local function MagentaSpin(pos, velocity)
	return EmitBullet(pos, velocity, magenta, spinSize)
end
local function MagentaLong(pos, velocity)
	return LongBullet(pos, velocity, magenta)
end
local function MagentaCustom(pos, velocity, scale)
	return EmitBullet(pos, velocity, magenta, vector.Scale(bigSize, scale))
end

local function HomingVector(dstPos, srcPos, speed)
	speed = speed or bulletSpeed
	return vector.Normalize(vector.Sub(dstPos, srcPos), speed)
end

local function LoadTexture(fileName)
	return eapi.ChopImage(fileName, { 32, 32 })
end

local cyanBlob = LoadTexture({ "image/cyan-blob.png", filter = true })

local cyanExplodeAngle = 0
local function CyanExplode(pos)
	local aim = vector.Rotate(bulletVelocity, cyanExplodeAngle)
	cyanExplodeAngle = (cyanExplodeAngle + 9) % 360
	for fi = 1, 8, 1 do
		bullet.Line(pos, aim, Cyan, 3, 0.9, 1.1)
		aim = vector.Rotate(aim, 45)
	end
end

local function BlobExplode(bullet)
	eapi.PlaySound(gameWorld, "sound/plop.ogg")
	util.MaybeCall(bullet.FollowUp)
end

local function CyanBlob(pos, velocity, detonateTime, animStart, FollowUp)
	local init = { sprite = cyanBlob,
		       offset = { x = -16, y = -16 },
		       OnDelete = BlobExplode,
		       bb = actor.Square(8),
		       velocity = velocity,
		       FollowUp = FollowUp,
		       class = "Bullet",
		       pos = pos,
		       z = 12, }
	local bullet = actor.Create(init)
	animStart = animStart or util.Random()
	eapi.Animate(bullet.tile, eapi.ANIM_LOOP, 32, animStart)
	bullet.Explode = function()
		local pos = actor.GetPos(bullet)
		actor.Delete(bullet)
		CyanExplode(pos)
	end
	eapi.AddTimer(bullet.body, detonateTime or 1, bullet.Explode)
	return bullet
end

local function CyanBlobZoom(pos, velocity, detonateTime, z)
	local targetSize = { x = 32, y = 32 }
	local targetOffset = { x = -16, y = -16 }
	local init = { sprite = cyanBlob,
		       offset = { x = -8, y = -8 },
		       spriteSize = { x = 16, y = 16 },
		       velocity = velocity,
		       pos = pos,
		       z = z, }
	local bullet = actor.Create(init)
	eapi.SetColor(bullet.tile, util.Gray(0.3))
	eapi.Animate(bullet.tile, eapi.ANIM_CLAMP, 32, 0)
	eapi.AnimateSize(bullet.tile, eapi.ANIM_CLAMP, targetSize, 1, 0)
	eapi.AnimatePos(bullet.tile, eapi.ANIM_CLAMP, targetOffset, 1, 0)
	eapi.AnimateColor(bullet.tile, eapi.ANIM_CLAMP, util.Gray(1.0), 1, 0)
	local function Foreground()
		CyanBlob(actor.GetPos(bullet), velocity, detonateTime, 0)
		actor.Delete(bullet)
	end
	eapi.AddTimer(bullet.body, 1, Foreground)
end

local function CyanBlobSmall(body, z, offset)
	local tile = eapi.NewTile(body, offset, { x = 16, y = 16 }, cyanBlob, z)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
	eapi.SetColor(tile, util.Gray(0.3))
	return tile
end

local function BulletHitsStreet(sShape, bShape)
	local bullet = actor.store[bShape]
	eapi.SetAcc(bullet.body, vector.null)
	eapi.SetVel(bullet.body, vector.null)
	bullet.Explode(bullet)
end

local function Line(pos, vel, BulletFn, count, min, max)
	bullet.z_epsilon = 0
	local step = (max - min) / (count - 1)
	for i = 1, count, 1 do
		BulletFn(pos, vector.Scale(vel, min + step * i))
	end
end

local function AimLine(pos, aim, BulletFn, count, min, max, speed)
	Line(pos, HomingVector(aim, pos, speed), BulletFn, count, min, max)
end

local function Arc(pos, vel, BulletFn, count, angle)
	bullet.z_epsilon = 0
	if count == 1 then
		BulletFn(pos, vel)
	else
		local step = angle / (count - 1)
		local angle = -angle / 2
		for i = 1, count, 1 do
			BulletFn(pos, vector.Rotate(vel, angle))
			angle = angle + step
		end
	end
end

local function FullArc(pos, vel, BulletFn, count)
	Arc(pos, vel, BulletFn, count, 360 - (360 / count))
end

local function AimArc(pos, aim, BulletFn, count, angle, speed)
	Arc(pos, HomingVector(aim, pos, speed), BulletFn, count, angle)
end

actor.SimpleCollide("Street", "Bullet", BulletHitsStreet)

local function BulletBomb(bombShape, bulletShape)
	local bullet = actor.store[bulletShape]
	bullet.Explode(bullet)
end

local frame = { { 33, 33 }, { 30, 30 } }
util.ring = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)
local function Sweep(speed)
	local body = eapi.NewBody(gameWorld, { x = 400, y = 0 })	
	for y = -280, 240, 40 do
		local x = util.Random(0, 80)
		local bb = { l = -x, r = 256, b = y, t =  y + 40}
		local shape = eapi.NewShape(body, nil, bb, "Bomb")
	end
	eapi.SetVel(body, { x = speed or -1600, y = 0 })
	util.DelayedDestroy(body, 0.55)
	
	local offset = { x = 384, y = -16 }
	local tile = eapi.NewTile(staticBody, offset, nil, util.ring, 25)
	util.ChangeTileAlpha(tile, 0.2)
	
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, { x = 2048, y = 2048 }, 0.5, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, { x = -624, y = -1024 }, 0.5, 0)
	eapi.AddTimer(staticBody, 0.5, util.DestroyClosure(tile))
end

local function Cloud(pos, vel, BulletFn, count, radius)
	local angle = 0
	for i = 1, count, 1 do
		local j = i / count
		local nominal = { x = 1 - j * j, y = 0 }
		angle = angle + 30 + 300 * util.Random()
		local offset = vector.Rotate(nominal, angle)
		BulletFn(pos, vector.Add(vel, vector.Scale(offset, radius)))
	end
end

local function AimCloud(pos, aim, BulletFn, count, radius, speed)
	Cloud(pos, HomingVector(aim, pos, speed), BulletFn, count, radius)
end

actor.SimpleCollide("Bomb", "Bullet", BulletBomb)

local function Pupil(pos, vel)
	local angle = 2 * math.pi
	local obj = Magenta(pos, vel)
	local offset = { x = 3, y = 0 }
	eapi.AnimateAngle(obj.tile, eapi.ANIM_LOOP, offset, angle, 0.1, 0)
	return obj
end

local aku = {
	-- lower lip
	{ { x = -25, y = -15 }, Cyan },
	{ { x = -15, y = -18 }, Cyan },
	{ { x =  -5, y = -20 }, Cyan },
	{ { x =   5, y = -20 }, Cyan },
	{ { x =  15, y = -18 }, Cyan },
	{ { x =  25, y = -15 }, Cyan },
	-- tongue
	{ { x = -15, y = -10 }, MagentaLong, "tongue", 0.4 },
	{ { x = -10, y = -12 }, MagentaLong, "tongue", 0.6 },
	{ { x =  -5, y = -13 }, MagentaLong, "tongue", 0.8 },
	{ { x =   0, y = -14 }, MagentaLong, "tongue", 1.0 },
	{ { x =   5, y = -13 }, MagentaLong, "tongue", 0.8 },
	{ { x =  10, y = -12 }, MagentaLong, "tongue", 0.6 },
	{ { x =  15, y = -10 }, MagentaLong, "tongue", 0.4 },
	-- left eye brow
	{ { x =  -5, y = 20 }, MagentaLong },
	{ { x = -25, y = 20 }, MagentaLong },
	{ { x = -10, y = 30 }, MagentaLong },
	{ { x = -20, y = 30 }, MagentaLong },
	{ { x = -15, y = 40 }, MagentaLong },
	-- left eye
	{ { x = -15, y = 10 }, CyanBig },
	{ { x = -15, y =  5 }, Pupil },
	-- right eye brow
	{ { x =  5, y = 20 }, MagentaLong },
	{ { x = 25, y = 20 }, MagentaLong },
	{ { x = 10, y = 30 }, MagentaLong },
	{ { x = 20, y = 30 }, MagentaLong },
	{ { x = 15, y = 40 }, MagentaLong },
	--  right eye
	{ { x = 15, y = 10 }, CyanBig },
	{ { x = 15, y =  5 }, Pupil },
	-- upper lip
	{ { x = -30, y = -12 }, Cyan },
	{ { x = -20, y =  -8 }, Cyan },
	{ { x = -10, y =  -5 }, Cyan },
	{ { x =   0, y =  -5 }, Cyan },
	{ { x =  10, y =  -5 }, Cyan },
	{ { x =  20, y =  -8 }, Cyan },
	{ { x =  30, y = -12 }, Cyan },
	
}

local function Aku(pos, vel)
	local amount = 10
	local angle = 90 + vector.Angle(vel)
	local speed = vector.Length(vel)
	local function PutBullet(entry)
		local bPos = vector.Add(pos, vector.Rotate(entry[1], angle))
		if entry[3] == "tongue" then
			for i = 0, amount * entry[4], 1 do
				local speedVary = 70 * entry[4] * i / amount
				local bVel = vector.Normalize(vel, speedVary)
				bVel = vector.Rotate(bVel, entry[1].x)
				entry[2](bPos, vector.Add(vel, bVel))
			end
		else 
			entry[2](bPos, vel)
		end
	end		
	util.Map(PutBullet, aku)
end

local function Eye(pos, vel)
	CyanBig(pos, vel)
	Pupil(pos, vel)
end

local function Dummy(pos, vel)
	return actor.Create({ pos = pos, 
			      velocity = vel,
			      class = "Garbage",
			      bb = actor.Square(2) })
end

local function Planetoid(pos, vel)
	local center = EmitBullet(pos, vel, magenta, bigSize)
	local childPos = vector.Add(pos, { x = 16, y = 0 })
	local child = Magenta(childPos, vector.null)
	actor.Link(child, center)
	eapi.SetStepC(child.body, eapi.STEPFUNC_ROT, 3)
	return center
end

bullet = {
	Eye	= Eye,
	Aku     = Aku,
	Sweep	= Sweep,
	Cloud	= Cloud,
	AimArc	= AimArc,
	Arc	= Arc,
	FullArc = FullArc,
	AimLine	= AimLine,
	Line	= Line,
	Cyan	= Cyan,
	Dummy	= Dummy,
	Magenta	= Magenta,
	CyanBig	= CyanBig,
	CyanLong = CyanLong,
	AimCloud = AimCloud,
	CyanBlob = CyanBlob,
	Planetoid = Planetoid,
	CyanCustom = CyanCustom,
	MagentaBig = MagentaBig,
	CyanBlobZoom = CyanBlobZoom,
	MagentaCustom = MagentaCustom,
	CyanBlobSmall = CyanBlobSmall,
	LoadTexture = LoadTexture,
	HomingVector = HomingVector,
	velocity = bulletVelocity,
	MagentaSpin = MagentaSpin,
	MagentaLong = MagentaLong,
	speed = bulletSpeed,
	magentaImg = magenta,
	CyanSpin = CyanSpin,
	cyanImg = cyan,
	z_epsilon = 0,
}
return bullet
