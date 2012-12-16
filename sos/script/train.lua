local wheels = eapi.ChopImage("image/wheels.png", { 128, 64 })
local frame = { { 0, 64 }, { 768, 128 } }
local cart = eapi.NewSpriteList("image/rails.png", frame)
local frame = { { 0, 192 }, { 768, 128 } }
local shadow = eapi.NewSpriteList("image/rails.png", frame)

local function AddWheel(obj, pos)
	local tile = eapi.NewTile(obj.body, pos, nil, wheels, obj.z - 0.1)
	eapi.Animate(tile, eapi.ANIM_LOOP, 64, util.Random())
end

local function WheelOcclusion(obj, pos, height, sprite)
	local size = { x = 600, y = height }
	local tile = eapi.NewTile(obj.body, pos, size, sprite, obj.z - 0.2)
	eapi.SetColor(tile, util.Gray(0))
end

local function AddShadow(obj)	
	local pos = { x = -16, y = -56 } 
	local tile = eapi.NewTile(obj.body, pos, nil, shadow, obj.z - 0.3)
	util.SetTileAlpha(tile, 0.8)
end

local function SwapShapes(obj)
	actor.DeleteShape(obj)
	actor.MakeShape(obj, { b = 12, t = 24, l = 704, r = 716 })
end

local function Put(vel, swapTime)
	local init = { class = "Mob",		       
		       sprite = cart,
		       health = 1000000,
		       pos = { x = 400, y = -226 },
		       bb = { b = 12, t = 76, l = 48, r = 64 },
		       velocity = vel or train.velocity,
		       Shoot = util.Noop,
		       z = -17, }
	local obj = actor.Create(init)
	AddWheel(obj, { x = 168, y = 0 })
	AddWheel(obj, { x = 552, y = 0 })
	WheelOcclusion(obj, { x = 128, y = 24 }, 40, util.white)
	WheelOcclusion(obj, { x = 128, y = 16 }, 8, util.gradient)
	local bb = { b = 12, t = 76, l = 64, r = 760 }
	eapi.NewShape(obj.body, nil, bb, "Street")
	util.Delay(obj.body, swapTime or 5, SwapShapes, obj)
	AddShadow(obj)
	return obj
end

train = {
	Put = Put,
	velocity = { x = -200, y = 0 },
}
return train
