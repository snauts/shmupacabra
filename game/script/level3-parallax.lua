local function ChangeAttributes(flip)
	return function(tile)
		eapi.SetColor(tile, { r = 1, g = 1, b = 1, a = 0.5 })
                eapi.FlipX(tile, flip)
                eapi.FlipY(tile, flip)
	end
end

local function CloudParallax(i, x, y, p, width, flip)
	local z = -20.0 - i * 10.0
	local frame = { { x, 0 }, { width, 480 } }
	local img = eapi.NewSpriteList("image/space-dust.png", frame)
	local speed = -800 / math.pow(2, i) - 100
	local pos = { x = -400, y = y }
	local size = { x = 800, y = 480 }
	p[i] = parallax.Continuous(pos, z, img, speed, 800, nil, size)
	util.Map(ChangeAttributes(flip), p[i].tile)
end

parallax.Dust = function(p, y)
	CloudParallax(5,   0, y, p, 800)
	CloudParallax(4,   0, y, p, 400, true)
	CloudParallax(3, 400, y, p, 200, true)
	CloudParallax(2, 600, y, p, 100, true)
	CloudParallax(1, 700, y, p,  50, true)
	CloudParallax(0, 750, y, p,  25, true)
end

parallax.pTop = { }
parallax.Dust(parallax.pTop, -240)

local deckStore = { }
local deckSize = { x = 1024, y = 128 }
local deck = eapi.ChopImage({ "image/deck.png", filter = true }, { 1024, 128 })

local function Tint(amount)
	return function(tile)
		eapi.SetColor(tile, util.Gray(amount))
	end
end

local function AnimateDeck(start)
	return function(tile)
		eapi.Animate(tile, eapi.ANIM_LOOP, 48 - 16 * start, start)
	end
end

local function DeckParallax(i, y, p)
	local z = -19.0 - i * 0.1
	local scale = 1 / math.pow(2, i)
	local speed = -200 * scale - 400
	local size = vector.Scale(deckSize, scale)
	local pos = { x = 0, y = y }
	local width = 896 * scale	
	p[i] = parallax.Continuous(pos, z, deck, speed, width, nil, size)
	util.Map(AnimateDeck(i), p[i].tile)
	util.Map(Tint(1 - i * 0.25), p[i].tile)
end

local deckHeight = 108
local deckBody = nil

parallax.AddDeckParallax = function(h)
	local bb = { l = -400, r = 400, b = -240, t = -216 }
	deckBody = eapi.NewBody(gameWorld, { x = 0, y = -h })
	eapi.NewShape(deckBody, nil, bb, "Street")
	DeckParallax(0, -240 - h, deckStore)
	DeckParallax(1, -178 - h, deckStore)
	DeckParallax(2, -148 - h, deckStore)
end

local function SetDeckHorizontalVel(speed)
	return function(deck)
		local x = eapi.GetVel(deck.body).x
		eapi.SetVel(deck.body, { x = x, y = speed })
	end
end

local function StopDeckUpMovement()
	util.Map(SetDeckHorizontalVel(0), deckStore)
	eapi.SetVel(deckBody, vector.null)
end

parallax.MoveDeckUp = function()
	parallax.AddDeckParallax(deckHeight)
	util.Map(SetDeckHorizontalVel(deckHeight), deckStore)
	eapi.AddTimer(staticBody, 1, StopDeckUpMovement)
	eapi.SetVel(deckBody, { x = 0, y = deckHeight })
end

parallax.GetSparkVel = function()
	return eapi.GetVel(deckStore[1].body)
end

local rib = eapi.ChopImage({ "image/ribs.png", filter = true }, { 128, 1024 })

local frame = { { 97, 1 }, { 30, 30 } }
util.gradient = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)

local function SetOrange(tile)
	local red = { r = 1.0, g = 0.75, b = 0 }
	eapi.SetColor(tile, { r = 1.0, g = 0.25, b = 0 })
	eapi.AnimateColor(tile, eapi.ANIM_REVERSE_LOOP, red, 0.15, 0)
end

local function SetTransparent(tile)
	local transparent = { r = 1.0, g = 1.0, b = 1.0, a = 0.5 }
	eapi.SetColor(tile, { r = 1.0, g = 1.0, b = 1.0, a = 1.0 })
	eapi.AnimateColor(tile, eapi.ANIM_REVERSE_LOOP, transparent, 0.15, 0)
end

local function RotatingHeat(pos, z, freq, angularSpeed, TileFn)
	local body = eapi.NewBody(staticBody, { x = 32, y = 0 })
	eapi.SetStepC(body, eapi.STEPFUNC_ROT, angularSpeed * math.pi)
	heat.Put(body, pos, z, freq, TileFn)
end

local function Gradient(h, y)
	local size = { x = 800, y = h }
	local pos = { x = -400, y = y }
	local tile = eapi.NewTile(staticBody, pos, size, util.gradient, -35)
	eapi.SetColor(tile, util.Gray(0))
	return tile
end

local function HeatWave()
	local tile = util.WhiteScreen(-50)
	eapi.SetColor(tile, { r = 0.25, g = 0, b = 0 })	
	RotatingHeat({ x = -576, y = -224 }, -40, 16, 0.61, SetTransparent)
	RotatingHeat({ x = -448, y = -224 }, -45, 8, 0.41, SetOrange)
	eapi.FlipY(Gradient(8, -136), true)
	Gradient(256, 112)
end

parallax.columns = { }
local function Column(i)
	local z = -19.05 - i * 0.1
	local amount = 1.0 - i * 0.25
	local width = 512 / math.pow(2, i)
	local speed = -200 / math.pow(2, i) - 400
	local pos = { x = -400, y = 240 - 1024 + i * 256 }
	local size = vector.Scale({ x = 128, y = 1024 }, amount)
	local p = parallax.Continuous(pos, z, rib, speed, width, nil, size)
	util.Map(Tint(amount), p.tile)
	util.Map(AnimateDeck(i), p.tile)
	parallax.columns[i] = p
end

local function WallAway(body)
	eapi.Destroy(body)
end

parallax.MotherShipInterior = function()
	util.Map(parallax.Delete, parallax.pTop)
	for i = 0, 2, 1 do Column(i) end
	HeatWave()
end

local function WallFullCover(body)
	util.Delay(body, 1.7, WallAway, body)
	parallax.MotherShipInterior()
end

local function WallSound()
	local function Screech() 
		eapi.PlaySound(gameWorld, "sound/slam.ogg")
	end	
	eapi.PlaySound(gameWorld, "sound/screech.ogg")
	eapi.AddTimer(staticBody, 1, Screech)
end

parallax.Wall = function()			
	local body = eapi.NewBody(gameWorld, { x = 500, y = 0 })
	eapi.SetVel(body, { x = -600, y = 0 })
	local function Column(pos, i)
		local z = -19.01 - i * 0.00001
		local tile = eapi.NewTile(body, pos, nil, rib, z)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, -i / 32)
		return tile
	end
	for i = 0, 12, 1 do
		local downPos = { x = i * 75, y = -780 }
		local tile = Column({ x = i * 75, y = -200 }, i + 1)
		eapi.AnimatePos(tile, eapi.ANIM_CLAMP, downPos, 1, i / 8)
		eapi.AddTimer(body, i / 8, WallSound)
	end
	util.Delay(body, 1.7, WallFullCover, body)
end

local frame = { { 0, 0 }, { 1024, 64 } }
local rails = eapi.NewSpriteList("image/rails.png", frame)
parallax.Rails = function(xPos)
	return function()
		local z = -18.5
		local step = -0.0001
		local pos = { x = xPos, y = -240 }
		local p = parallax.Continuous(pos, z, rails, -600, 921, step)
		parallax.rails = p		 
	end
end

local function StopParallax(p, time)
	local vel = eapi.GetVel(p.body)
	local acc = vector.Scale(vel, -1 / time)
	eapi.SetAcc(p.body, acc)
	local function Finalize()
		eapi.SetVel(p.body, vector.null)
		eapi.SetAcc(p.body, vector.null)
	end
	eapi.AddTimer(p.body, time, Finalize)
end

local function SideOcclusion(i, w, z, time)
	local size = { x = 480, y = w }
	local offset = { x = -240, y = 400 - w }
	local tile = eapi.NewTile(staticBody, offset, size, util.gradient, z)
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0 })
	local targetColor = { r = 0, g = 0, b = 0, a = 0.7 }
	local animColor = { r = 0, g = 0, b = 0, a = 1.0 }
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, targetColor, time, 0)
	local function Flicker()
		eapi.AnimateColor(tile, eapi.ANIM_REVERSE_LOOP,
				  animColor, 0.4, 0.4 * i / 4)
	end
	eapi.AddTimer(staticBody, time, Flicker)
	util.RotateTile(tile, -90)
end

parallax.StopInterior = function(time)
	for i = 0, 3, 1 do
		local z = (i == 0 and 50) or (-19.055 - (i - 1) * 0.1)
		SideOcclusion(i, math.pow(2, i + 6), z, time)
	end
	local function SlowDown(p) StopParallax(p, time) end
	util.Map(SlowDown, parallax.columns)
	util.Map(SlowDown, deckStore)
end
