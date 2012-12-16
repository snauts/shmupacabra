local size = util.GetCameraSize()

local function Cage(type, x1, x2)
	local bb = { { b = -x1 * size.y, t = -x2 * size.y,
		       l = -x1 * size.x, r =  x1 * size.x },
		     { t =  x1 * size.y, b =  x2 * size.y,
		       l = -x1 * size.x, r =  x1 * size.x },
		     { l = -x1 * size.x, r = -x2 * size.x,
		       b = -x2 * size.y, t =  x2 * size.y },
		     { r =  x1 * size.x, l =  x2 * size.x,
		       b = -x2 * size.y, t =  x2 * size.y }, }
	for i = 1, 4, 1 do eapi.NewShape(staticBody, nil, bb[i], type) end
end

local half = vector.Scale(size, 0.5)
local function OffScreen(pos)
	return pos.x < -half.x or pos.x > half.x
	    or pos.y < -half.y or pos.y > half.y
end

local store = { }

local function Square(x)
	return { b = -x, t = x, l = -x, r = x }
end

local function GetPos(actor, relativeTo)
	return eapi.GetPos(actor.body, relativeTo or gameWorld)
end

local function MakeSimpleTile(obj, z)
	local offset = obj.offset
	local size = obj.spriteSize
	return eapi.NewTile(obj.body, offset, size, obj.sprite, z or obj.z)
end

local function MakeTile(obj)
	obj.tile = MakeSimpleTile(obj)
	return obj.tile
end

local function SwapTile(obj, sprite, animType, FPS, startTime)
	eapi.Destroy(obj.tile)
	obj.sprite = sprite
	actor.MakeTile(obj)
	eapi.Animate(obj.tile, animType, FPS, startTime)
end

local function MakeShape(obj, bb)
	local shape = eapi.NewShape(obj.body, nil, bb or obj.bb, obj.class)
	obj.shape[shape] = shape
	store[shape] = obj
	return shape
end

local function Create(actor)
	actor.shape = { }
	actor.blinkIndex = 0
	local parent = actor.parentBody or gameWorld
	actor.body = eapi.NewBody(parent, actor.pos)
	actor.blinkTime = eapi.GetTime(actor.body)
	if actor.bb and actor.class then
		MakeShape(actor)
	end
	if actor.sprite then
		MakeTile(actor)
	end
	if actor.velocity then
		eapi.SetVel(actor.body, actor.velocity)
	end
	return actor	
end


local function DeleteShapeObject(shape)
	eapi.Destroy(shape)
	store[shape] = nil
end

local function DeleteShape(actor)
	util.Map(DeleteShapeObject, actor.shape)
	actor.shape = { }
end

local function Delete(actor)
	if actor.destroyed then return end
	util.Map(Delete, actor.children)
	util.MaybeCall(actor.OnDelete, actor)
	DeleteShape(actor)
	eapi.Destroy(actor.body)
	actor.destroyed = true
end

local function Link(child, parent)
	if parent.children == nil then parent.children = { } end
	eapi.Link(child.body, parent.body)
	parent.children[child] = child
end

local function ReapActor(rShape, aShape)
	Delete(store[aShape])
end

local function Blocker(bShape, aShape, box)
        local actor = store[aShape]
        local pos = GetPos(actor)

	local movex = math.abs(box.l) > math.abs(box.r) and box.r or box.l
	local movey = math.abs(box.b) > math.abs(box.t) and box.t or box.b

	if math.abs(movex) > math.abs(movey) then
		movex = 0
	else
		movey = 0
	end

	eapi.SetPos(actor.body, vector.Offset(pos, -movex, -movey))
	return false, actor
end

local function CloneBody(body, pos, vel)
	pos = vector.Add(eapi.GetPos(body, gameWorld), pos or vector.null)
	vel = vector.Add(eapi.GetVel(body), vel or vector.null)
	local clone = eapi.NewBody(gameWorld, pos)
	eapi.SetAcc(clone, vector.Scale(vel, -1))
	eapi.SetVel(clone, vel)
	return clone
end

Cage("Reaper", 0.9, 0.8)

local function SimpleCollide(type1, type2, Func, priority, update)
	update = (update == nil) and true or update
	local function Callback(shape1, shape2, resolve)
		if not resolve then return end
		Func(shape1, shape2, resolve)
	end
	eapi.Collide(gameWorld, type1, type2, Callback, update, priority or 10)
end

SimpleCollide("Reaper", "Mob", ReapActor)
SimpleCollide("Reaper", "Star", ReapActor)
SimpleCollide("Reaper", "Shoot", ReapActor)
SimpleCollide("Reaper", "Bullet", ReapActor)
SimpleCollide("Reaper", "Garbage", ReapActor)

local function DelayedDelete(obj, time)
	eapi.AddTimer(obj.body, time, function() Delete(obj) end)
	DeleteShape(obj)
end

local function SiblingHit(obj)
	if obj.sibling then
		obj.health = math.min(obj.health, obj.sibling.health)
		obj.sibling.health = obj.health
	end
end

local function SiblingShoot(ShootFunction)
	return function(obj, fromSibling)
		ShootFunction(obj)
		if obj.sibling and not fromSibling then
			obj.sibling.Shoot(obj.sibling, true)
		end
	end
end

local function Rush(obj, time, fastSpeed, slowSpeed)
	eapi.SetVel(obj.body, { x = -fastSpeed, y = 0 })
	eapi.SetAcc(obj.body, { x = fastSpeed, y = 0 })
	local function Constant()
		if obj.crashing then return end
		eapi.SetVel(obj.body, { x = -slowSpeed, y = 0 })
		eapi.SetAcc(obj.body, vector.null)
	end
	eapi.AddTimer(obj.body, time, Constant)
	return Constant
end

local function Jerk(tile, amount)
	eapi.SetPos(tile, vector.Rnd(eapi.GetPos(tile), amount))
end

local blinkColors = { }
blinkColors[0]  = { r = 0.8, g = 0.8, b = 0.0, a = 0.5 }
blinkColors[1]  = { r = 0.8, g = 0.4, b = 0.0, a = 0.5 }
blinkColors[2]  = { r = 0.8, g = 0.0, b = 0.0, a = 0.5 }
local function Blink(obj)
	local time = eapi.GetTime(obj.body)
	if time - obj.blinkTime > 0.1 then
		obj.blinkTime = time
		obj.blinkIndex = (obj.blinkIndex + 1) % 3		
		local tile = eapi.Clone(obj.tile)
		local epsilon = obj.blinkZ or 0.001
		eapi.SetDepth(tile, eapi.GetDepth(tile) + epsilon)
		eapi.SetColor(tile, blinkColors[obj.blinkIndex])
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, .05, 0)
		eapi.AddTimer(obj.body, 0.1, util.DestroyClosure(tile))
		if obj.jerk then Jerk(tile, obj.jerk) end
		eapi.Blend(tile, "alpha")
	end
end

local function GetDir(body)
	if eapi.__GetStep(body) == eapi.STEPFUNC_STD then
		return eapi.GetVel(body)
	elseif eapi.__GetStep(body) == eapi.STEPFUNC_ROT then
		local pos = eapi.GetPos(body)
		local vel = eapi.GetVel(body)
		local angle = (vel.x > 0) and 90 or -90
		return vector.Rotate(pos, angle)
	end
end

local function AnimateAngle(tile, angle, duration)
	eapi.AnimateAngle(tile, eapi.ANIM_CLAMP, vector.null,
			  vector.Radians(angle), duration, 0)
end

local function GetDirAdvanced(obj)
	if obj.GetDir then
		return obj.GetDir(obj)
	else
		return GetDir(obj.body)
	end
end

local function AnimateToVelocity(obj, turnSpeed, AnimateFn)
	local function AdjustAngle()
		AnimateToVelocity(obj, turnSpeed, AnimateFn)
	end
	local vel = GetDirAdvanced(obj)
	local diff = util.FixAngle(vector.Angle(vel) - obj.angle)
	local absDiff = math.abs(diff)
	if absDiff > 1 then
		local duration = turnSpeed * absDiff / 360		
		local scale = math.min(1, 0.1 / duration)
		obj.angle = obj.angle + scale * diff
		AnimateFn(obj, obj.angle, 0.1)
	end
	eapi.AddTimer(obj.body, 0.1, AdjustAngle)
end

local function BoxAtPos(pos, size)
	return { l = pos.x - size, r = pos.x + size,
		 b = pos.y - size, t = pos.y + size }		 
end

util.white = eapi.ChopImage("image/misc.png", { 16, 16 }, 4, 2)

actor = {
	store = store,
	BoxAtPos = BoxAtPos,
	CloneBody = CloneBody,
	MakeShape = MakeShape,
	SiblingHit = SiblingHit,
	SiblingShoot = SiblingShoot,
	SimpleCollide = SimpleCollide,
	DelayedDelete = DelayedDelete,
	AnimateToVelocity = AnimateToVelocity,
	AnimateAngle = AnimateAngle,
	DeleteShape = DeleteShape,
	OffScreen = OffScreen,
	MakeTile = MakeTile,
	SwapTile = SwapTile,
	Blocker = Blocker,
	GetDir = GetDir,
	Create = Create,
	Square = Square,
	Delete = Delete,
	GetPos = GetPos,
	Blink = Blink,
	Link = Link,
	Rush = Rush,
	Cage = Cage,
}
return actor
