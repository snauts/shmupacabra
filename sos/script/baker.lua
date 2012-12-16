local bakerFile = { "image/baker.png", filter = true }
local bakerImg = eapi.ChopImage(bakerFile, { 32, 32 })

local bakerLidFile = { "image/baker-lid.png", filter = true }
local bakerLidImg = eapi.ChopImage(bakerLidFile, { 32, 32 })

local offset1 = { x = -6, y = -2 }
local size1 = { x = 4, y = 4 }

local offset2 = { x = -19, y = -16 }
local size2 = { x = 32, y = 32 }

local bulletOffset = { x = -4, y = 0 }

local function BakeBullet(obj)
	local z = obj.z + 0.5
	local tile = eapi.NewTile(obj.body, offset1, size1, obj.bulletImg, z)

	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, size2, 1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, offset2, 1, 0)
	util.RotateTile(tile, obj.angle)

	local function Detach()
		local pos = actor.GetPos(obj)
		pos = vector.Add(pos, vector.Rotate(bulletOffset, obj.angle))
		local projectile = obj.Bullet(pos, eapi.GetVel(obj.body))
		util.MaybeCall(obj.BulletVel, projectile, obj)
		obj.bulletTile = nil
		eapi.Destroy(tile)
	end
	eapi.AddTimer(obj.body, 1, Detach)
	obj.bulletTile = tile
end		

local function Explode(obj)
	explode.Simple(vector.null, obj.body, obj.z + 2)
	powerup.StarFromMob(obj)(actor.GetPos(obj), vector.null)
	actor.Delete(obj)
end

local function Put(pos, angle, delay, BulletVel)
	local init = { health = 2,
		       class = "Mob",
		       sprite = bakerImg,
		       offset = { x = -16, y = -16 },
		       bb = actor.Square(8),
		       BulletVel = BulletVel,
		       Bullet = bullet.Cyan,
		       bulletImg = bullet.cyanImg,
		       Shoot = Explode,
		       angle = angle,
		       pos = pos,
		       z = 5, }
	local obj = actor.Create(init)	
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 32, 0)
	util.RotateTile(obj.tile, angle)

	local z = obj.z + 1
	obj.lid = eapi.NewTile(obj.body, obj.offset, nil, bakerLidImg, z)
	util.RotateTile(obj.lid, angle)

	local function Open()
		eapi.AddTimer(obj.body, 0.5, function() BakeBullet(obj) end)
		eapi.Animate(obj.lid, eapi.ANIM_CLAMP, 32, 0)
	end
	eapi.AddTimer(obj.body, delay or 1, Open)
	return obj
end

local function AnimateAngle(obj, angle, duration)
	actor.AnimateAngle(obj.tile, angle, duration)
	actor.AnimateAngle(obj.lid, angle, duration)
	if obj.bulletTile then
		actor.AnimateAngle(obj.bulletTile, angle, duration)
	end
end

local function MakeTurnable(obj, speed)
	actor.AnimateToVelocity(obj, speed or 1, AnimateAngle)
end

local function Turning(pos, vel, delay, BulletVel)
	local obj = Put(pos, vector.Angle(vel), delay, BulletVel)
	obj.bulletImg = bullet.magentaImg
	obj.Bullet = bullet.Magenta
	eapi.SetVel(obj.body, vel)
	MakeTurnable(obj)
	return obj
end

local function Following(pos, target, delay, BulletVel)
	local dst = actor.GetPos(target)
	local vel = vector.Sub(dst, pos)
	local obj = Turning(pos, vel, delay, BulletVel)

	local function Follow()
		if not target.destroyed then
			local src = actor.GetPos(obj)
			local dst = actor.GetPos(target)
			local vel = vector.Sub(dst, src)
			local len = vector.Length(vel)
			local scale = math.min(10, 0.2 * len)
			vel = vector.Scale(vel, scale)
			eapi.SetVel(obj.body, vel)
 			eapi.AddTimer(obj.body, 0.1, Follow)
		else
			local vel = eapi.GetVel(obj.body)
			vel = vector.Normalize(vel, 100)
			eapi.SetAcc(obj.body, vel)
		end
	end
	Follow()

	return obj
end

baker = {
	Put = Put,
	Turning = Turning,
	Following = Following,
	MakeTurnable = MakeTurnable,
}
return baker
