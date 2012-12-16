local wingFile = { "image/v-wing.png", filter = true }
local wingImg = eapi.ChopImage(wingFile, { 64, 128 })

local cannonFile = { "image/space-cannon.png", filter = true }
local cannonImg = eapi.ChopImage(cannonFile, { 64, 32 })

local cannonBulletVel = { x = 150, y = 0 }

local function CannonShoot(obj, BulletFn)
	local pos = actor.GetPos(obj)
	local vel = vector.Rotate(cannonBulletVel, 180 + obj.angle)
	pos = vector.Add(pos, vector.Normalize(vel, 32))
	vel = vector.Add(vel, eapi.GetVel(obj.parent.body))
	BulletFn(pos, vel)
end

local function ThreeLine(pos, vel)
	bullet.Line(pos, vel, bullet.Magenta, 3, 0.95, 1.05)
end

local function ShootI(obj)
	util.Delay(obj.body, 0.125, CannonShoot, obj, ThreeLine)
	eapi.Animate(obj.tile, eapi.ANIM_CLAMP, 32, 0)
end

local function TrailExplosions(obj)
	local offset = eapi.GetPos(obj.body, obj.parent.body)
	local vel = vector.Scale(eapi.GetVel(obj.parent.body), 0.5)
	local function Emit()
		local pos = eapi.GetPos(obj.parent.body)
		local body = explode.Small(vector.Add(pos, offset), obj.z + 1)
		eapi.SetVel(body, vector.Rnd(vel, 24))
		eapi.AddTimer(obj.parent.body, 0.1, Emit)
	end
	Emit()
end

local function Explode(obj)
	TrailExplosions(obj)
	eapi.Unlink(obj.body)
	eapi.SetVel(obj.body, eapi.GetVel(obj.parent.body))
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	powerup.ArcOfStars(obj, 3)
	actor.Delete(obj)
	obj.parent.Dec()
end

local function Rotate(obj)
	util.RotateTile(obj.tile, obj.angle)
end

local function Put(parent, offset, placement, PatternFn)
	offset = vector.Rotate(offset, parent.angle)
	local init = { health = 7,
		       class = "Mob",
		       sprite = cannonImg,
		       offset = { x = -48, y = -16 },
		       parentBody = parent.body,
		       placement = placement,
		       angle = parent.angle,
		       bb = actor.Square(8),
		       Shoot = Explode,
		       parent = parent,
		       pos = offset,
		       z = 1, }
	local obj = actor.Create(init)	
	parent.children[obj] = obj
	PatternFn(obj)
	Rotate(obj)
end

local flameSize = { x = 32, y = 32 }
local function Flame(parent, offset, angle)
	local img = player.flame
	local tile = eapi.NewTile(parent.body, offset, flameSize, img, -0.1)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, util.Random())
	util.RotateTile(tile, parent.angle + angle)
end

local function VWing(pos, vel, PatternFn)
	local init = { class = "Garbage",
                       sprite = wingImg,
		       bb = actor.Square(4),
		       offset = { x = -32, y = -64 },
		       angle = 180 + vector.Angle(vel),
		       velocity = vel,
		       children = { },
		       cannons = 4,
		       pos = pos,
		       z = 0, }
	local obj = actor.Create(init)	
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, 16, 0)
	Rotate(obj)

	obj.Dec = function()
		obj.cannons = obj.cannons - 1
		if obj.cannons == 0 then			
			explode.Area(vector.null, obj.body, 10, 64, 0.02)
			actor.DelayedDelete(obj, 0.6)
		end
	end

	Put(obj, { x =   6, y =  48 },  2, PatternFn)
	Put(obj, { x = -12, y =  20 },  1, PatternFn)
	Put(obj, { x = -12, y = -20 }, -1, PatternFn)
	Put(obj, { x =   6, y = -48 }, -2, PatternFn)

	Flame(obj, { x = -24, y = -34 }, 150)
	Flame(obj, { x = -28, y = -42 }, 160)
	Flame(obj, { x = -32, y = -48 }, 170)

	Flame(obj, { x = -24, y =   2 }, 210)
	Flame(obj, { x = -28, y =  10 }, 200)
	Flame(obj, { x = -32, y =  16 }, 190)
end

local function SimpleI(obj)
	local delay = 0.4 * (math.abs(obj.placement) - 1)
	util.Repeater(util.Closure(ShootI, obj), 10, 1, obj.body, delay)
end

local function TightVWing(pos, vel)
	VWing(pos, vel, SimpleI)
end

local function SparseI(obj)
	obj.angle = obj.angle - 10 * obj.placement
	SimpleI(obj)
end

local function SparseVWing(pos, vel)
	VWing(pos, vel, SparseI)
end

local function Angle(obj)
	obj.angle = obj.startAngle - 30 * obj.fraction * obj.placement
	return vector.Radians(obj.angle)
end
	
local function ShootC(obj)
	util.Delay(obj.body, 0.025, CannonShoot, obj, ThreeLine)
	eapi.Animate(obj.tile, eapi.ANIM_CLAMP, 160, 0)
end

local function RotateC(obj)
	obj.fraction = obj.fraction + obj.step
	if obj.fraction <= 0.0001 or obj.fraction >= 0.9999 then
		obj.step = -obj.step
	end
	util.Delay(obj.body, 0.1, ShootC, obj)
	eapi.AnimateAngle(obj.tile, eapi.ANIM_CLAMP,
			  vector.null, Angle(obj), 0.1, 0)
end

local function Flip(dir)
	return function(obj)
		obj.step = -dir * 0.1
		obj.fraction = 0.5 + dir * 0.5
		obj.startAngle = obj.angle
		local RFn = util.Closure(RotateC, obj)
		util.Repeater(RFn, 9, 0.2, obj.body, 0)
	end
end

local function FlipingVWing(pos, vel, dir)
	VWing(pos, vel, Flip(dir))
end

local function SimpleExplode(obj)
	explode.Area(vector.null, obj.body, 10, 32, 0.05)
	powerup.ArcOfStars(obj, 3)
	actor.Delete(obj)
end

local function GetDir(obj)
	local target = player.Pos()
	local pos = actor.GetPos(obj)
	return vector.Sub(target, pos)
end

local function AnimateAngle(obj, angle, duration)
	for i = 1, 3, 1 do
		actor.AnimateAngle(obj.tiles[i], angle - 180, duration)
	end
end

local shootInterval = 1.5
local offsetBase = { x = 32, y = 0 }
local function Attack(obj)
	local function EmitBullet()
		local offset = vector.Rotate(offsetBase, obj.angle)
		local pos = vector.Add(actor.GetPos(obj), offset)
		if pos.x > -220  then
			local aim = vector.Add(player.Pos(), obj.jitter)
			bullet.AimLine(pos, aim, bullet.Cyan, 3, 0.95, 1.0)
			eapi.AddTimer(obj.body, shootInterval, EmitBullet)
			for i = 1, 3, 1 do
				local tile = obj.tiles[i]
				eapi.Animate(tile, eapi.ANIM_CLAMP, 32, 0)
			end
		end
	end
	eapi.AddTimer(obj.body, obj.start, EmitBullet)
end

local period = 0.2
local amplitude = 400
local function Oscilate(obj)	
	local function Swing()
		eapi.SetAcc(obj.body, { x = 0, y = amplitude * obj.swingDir })
		eapi.AddTimer(obj.body, period, Swing)
		obj.swingDir = -obj.swingDir
	end
	Swing()
end

local black = { r = 0, g = 0, b = 0, a = 0.9 }
local white = { r = 1, g = 1, b = 1, a = 0.5 }
local function AddOutline(obj, color, offset)
	local tile = eapi.Clone(obj.tile)
	eapi.Blend(tile, "alpha")
	eapi.SetDepth(tile, obj.z - 0.1)
	eapi.SetColor(tile, color)
	eapi.SetPos(tile, offset)
	return tile
end

local function Simple(pos, vel, precision, start)
	vel = vector.Add(vel, { x = 0, y = 0.25 * period * amplitude })
	local init = { health = 3,
		       class = "Mob",
		       sprite = cannonImg,
		       offset = { x = -48, y = -16 },
		       jitter = vector.Rnd(vector.null, precision or 0),
		       bb = actor.Square(8),
		       Shoot = SimpleExplode,
		       start = start or 1.0,
		       velocity = vel,
		       GetDir = GetDir,
		       swingDir = -1,
		       angle = 180,
		       tiles = { },
		       pos = pos,
		       z = 1, }
	local obj = actor.Create(init)
	obj.tiles[1] = obj.tile
	obj.tiles[2] = AddOutline(obj, black, { x = -49, y = -18 })
	obj.tiles[3] = AddOutline(obj, white, { x = -47, y = -14 })
	actor.AnimateToVelocity(obj, 1, AnimateAngle)
	eapi.SetAcc(obj.body, { x = 0, y = 0.5 * amplitude })
	util.Delay(obj.body, 0.5 * period, Oscilate, obj)
	Attack(obj)
end

cannon = {
	Simple = Simple,
	TightVWing = TightVWing,
	SparseVWing = SparseVWing,
	FlipingVWing = FlipingVWing,
	interval = shootInterval,
}
return cannon
