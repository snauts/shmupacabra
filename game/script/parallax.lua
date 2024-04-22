local size = util.GetCameraSize()

local bb = { b = -2 * size.y, t = 2 * size.y, l = -3 * size.x, r = -2 * size.x }
eapi.NewShape(staticBody, nil, bb, "Collector")
local bb = { b = -2 * size.y, t = 2 * size.y, l =  2 * size.x, r =  3 * size.x }
eapi.NewShape(staticBody, nil, bb, "Collector")

local parallaxStore = { }

local function Delete(parallax)
	parallaxStore[parallax.shape] = nil
	eapi.Destroy(parallax.body)
end

local function AddShape(parallax, bb)
	if parallax.shape then 
		parallaxStore[parallax.shape] = nil
		eapi.Destroy(parallax.shape)
	end
	parallax.shape = eapi.NewShape(parallax.body, nil, bb, "Parallax")
	parallaxStore[parallax.shape] = parallax
end

local function Simple(pos, z, img, speed, callback, tile)
	local parallax = { }
	callback = callback or Delete
	parallax.body = eapi.NewBody(gameWorld, pos)
	AddShape(parallax, { l = 0, r = 32, b = 0, t = 32 })
	parallax.tile = tile or eapi.NewTile(parallax.body, nil, nil, img, z)
	eapi.SetVel(parallax.body, { x = speed, y = 0 })
	parallax.callback = callback
	return parallax
end

local function Continuous(pos, z, sprite, speed, width, step, scale)
	step = step or 0.0001
	local function Shift(parallax)
		local pos = actor.GetPos(parallax)
		local dir = util.Sign(eapi.GetVel(parallax.body).x)
		local newPos = vector.Offset(pos, -dir * width, 0)
		eapi.SetPos(parallax.body, newPos)
	end
	local parallax = Simple(pos, z, sprite, speed, Shift, { })
	local side = math.ceil(size.x / width) * width
	AddShape(parallax, { l = -side, r = -side + 32, b = 0, t = 32 })
	for i = -side - width, 2 * side - width, width do
		z = z + step
		local pos = { x = i, y = 0 }
		local tile = eapi.NewTile(parallax.body, pos, scale, sprite, z)
		table.insert(parallax.tile, tile)
	end
	parallax.side = side
	return parallax
end

local function CollectParallax(cShape, pShape)
	local parallax = parallaxStore[pShape]
	parallax.callback(parallax)
end

local function Animate(parallax, type, FPS)
	local function Animate(tile)
		eapi.Animate(tile, type, FPS, 0)
	end
	util.Map(Animate, parallax.tile)
end

local function Flip(px, bb)
	local vel = eapi.GetVel(px.body)
	eapi.SetAcc(px.body, vector.Scale(vel, -2))
	local function Stop() eapi.SetAcc(px.body, vector.null) end
	eapi.AddTimer(px.body, 1, Stop)
	parallax.AddShape(px, bb)
end

local function Reverse(px)
	Flip(px, { l = px.side - 32, r = px.side, b = 0, t = 32 })
end

local function Straight(px)
	Flip(px, { l = -px.side, r = -px.side + 32, b = 0, t = 32 })
end

local function Ascend(p)
	for i = 0, 3, 1 do
		local vel = eapi.GetVel(p[i].body)
		eapi.SetAcc(p[i].body, { x = 2 * vel.x, y = 0.25 * vel.x })
		util.Delay(p[i].body, 0.25, util.StopAcc, p[i])
	end
end

actor.SimpleCollide("Collector", "Parallax", CollectParallax)

parallax = {
	Ascend = Ascend,
	Simple = Simple,
	Delete = Delete,
	Animate = Animate,
	Reverse = Reverse,
	Straight = Straight,
	AddShape = AddShape,
	Continuous = Continuous,
}
return parallax
