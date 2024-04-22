local spinTailFile = { "image/spin-tail.png", filter = true }
local spinTailImg = eapi.ChopImage(spinTailFile, { 64, 128 })

local crawlerFile = { "image/crawler.png", filter = true }
local crawlerImg = eapi.ChopImage(crawlerFile, { 64, 128 })

local function AnimateAngle(obj, angle, duration)
	actor.AnimateAngle(obj.tile, angle, duration)
end

local size = { x = 64, y = 8 }
local offset = { x = -48, y = -4 }
local function ExplodeSerpent(obj)
	eapi.AnimateSize(obj.tile, eapi.ANIM_CLAMP, size, 0.5, 0)
	eapi.AnimatePos(obj.tile, eapi.ANIM_CLAMP, offset, 0.5, 0)
	powerup.StarFromMob(obj)(actor.GetPos(obj), vector.null)
	explode.Simple(vector.null, obj.body, obj.z + 2)
	actor.DeleteShape(obj)
	obj.done = true

	if obj.num == 0 then
		local function Consume()
			actor.Delete(obj)
			if obj.child then
				eapi.AddTimer(obj.child.body, 0.05, Consume)
				ExplodeSerpent(obj.child)
				obj = obj.child
			end
		end
		Consume()
	end
end

local function ExplodeCrawler(obj)
	explode.Area(vector.null, obj.body, 10, 24, 0.05)
	powerup.ArcOfStars(obj, 2)
	obj.exploded = true
	actor.Delete(obj)
end

local function Element(pos, vel, height, i, follow, attributes)
	local init = { angle = vector.Angle(vel),
		       spriteSize = { x = 64, y = height },
		       offset = { x = -48, y = -0.5 * height },
		       bb = actor.Square(16),
		       velocity = vel,
		       num = i,
		       pos = pos,
		       z = 5 - i * 0.001, }
	util.InjectTable(init, attributes)
	local obj = actor.Create(init)
	util.RotateTile(obj.tile, obj.angle)
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 32, i * 0.01)
	actor.AnimateToVelocity(obj, 1, AnimateAngle)
	eapi.FlipX(obj.tile, true)

	if follow then
		local speed = vector.Length(vel)
		local function Follow()
			if not follow.destroyed then
				local src = actor.GetPos(obj)
				local dst = actor.GetPos(follow)
				local vel = vector.Sub(dst, src)
				vel = vector.Scale(vel, 10)
				eapi.SetVel(obj.body, vel)
				eapi.AddTimer(obj.body, 0.1, Follow)
			elseif follow.exploded then
				util.MaybeCall(attributes.Detach, obj)
			end			
		end
		Follow()
	end

	return obj
end

local wiggleAngle = 0
local function CyanStrongWiggle(pos, vel)
	local projectile = bullet.Cyan(pos, vel)
	local offset = vector.Rotate({ x = 1.5, y = 0 }, wiggleAngle)
	eapi.SetPos(projectile.tile, vector.Add(offset, { x = -16, y = -16 }))
	wiggleAngle = wiggleAngle + 45
end

local function Ripple(pos, vel, angle)
	if actor.OffScreen(pos) then return end
	vel = vector.Rotate(vel, angle)
	vel = vector.Normalize(vel, bullet.speed)
	bullet.Line(pos, vel, CyanStrongWiggle, 7, 0.8, 1.0)
end

local function Wiggle(obj)
	local body = obj.body
	local arm = vector.Rotate(obj.velocity, 90)
	local acc = vector.Normalize(arm, 200)
	local starting = vector.Scale(acc, -0.5)
	eapi.SetVel(body, vector.Add(obj.velocity, starting))
	local function Acc()
		eapi.SetAcc(body, acc)
		acc = vector.Scale(acc, -1)
		eapi.AddTimer(body, 1, Acc)
	end
	Acc()	
end

local function RippleAndWiggle(obj)
	local body = obj.body
	local function Piu()
		local pos = eapi.GetPos(body)
		local vel = eapi.GetVel(body)
		pos = vector.Add(pos, vector.Normalize(vel, 16))
		for i = 90, 150, 30 do
			Ripple(pos, vel, i)
			Ripple(pos, vel, -i)
		end
		if not obj.done then
			eapi.AddTimer(body, 0.2, Piu)
		end
	end
	Wiggle(obj)
	Piu()
end

local function RippleAndWiggleHead(i, obj)
	if i == 0 then
		obj.health = 50
		RippleAndWiggle(obj)
	end
end

local function PutSerpent(pos, vel, attributes, Behavior)
	local obj = nil
	for i = 0, 20, 1 do
		local parent = obj
		local height = 128 - math.abs(i - 10) * 6
		obj = Element(pos, vel, height, i, obj, attributes)
		pos = vector.Add(pos, vector.Normalize(vel, -1))
		if parent then parent.child = obj end
		Behavior(i, obj)
	end
end

local function Put(pos, vel)
	local attributes = {
		health = 5,
		class = "Mob",
		sprite = spinTailImg,
		Shoot = ExplodeSerpent,		
	}
	PutSerpent(pos, vel, attributes, RippleAndWiggleHead)
end

local function CrawlerPattern(obj, dir)
	local scale = -3
	local step = 1
	local pos = actor.GetPos(obj)			
	if actor.OffScreen(pos) then return end
	local function ScaledCyan(pos, vel)
		bullet.z_epsilon = scale * 0.001
		vel = vector.Scale(vel, 1.0 + dir * 0.025 * scale)
		bullet.CyanCustom(pos, vel, 0.7 + 0.1 * scale)
		scale = scale + step
	end
	local vel = bullet.HomingVector(player.Pos(), pos)
	vel = vector.Rotate(vel, 30 * dir * (obj.num - 10))
	bullet.Arc(pos, vel, ScaledCyan, 7, 10)
end

local function RippleAndWiggleHead(i, obj)
	local flip = 1
	for x = 0, 3, 1 do
		local delay = 1.0 + x + i * 0.1
		util.Delay(obj.body, delay, CrawlerPattern, obj, flip)
		flip = -flip
	end
	if i == 0 then 	Wiggle(obj) end
end

local function Detach(obj)
	local pos = eapi.GetPos(obj.body)
	local speed = vector.Length(obj.velocity)
	obj.velocity = bullet.HomingVector(player.Pos(), pos, speed)
	Wiggle(obj)
end

local function Crawl(pos, vel)
	local attributes = {
		health = 5,
		class = "Mob",
		Detach = Detach,
		sprite = crawlerImg,
		Shoot = ExplodeCrawler,		
	}
	PutSerpent(pos, vel, attributes, RippleAndWiggleHead)
end

serpent = {
	Put = Put,
	Crawl = Crawl,
	Wiggle = Wiggle,
	Raw = PutSerpent,
}
return serpent
