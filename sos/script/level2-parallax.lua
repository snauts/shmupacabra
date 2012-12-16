local p = { }

local direction = parallax.direction or -1
local cloudFile = parallax.clouds or "image/clouds.png"
local skyFile = parallax.sky or "image/pink-sun.png"

local function CloudParallax(i, pos)
	local z = -20.0 - i * 10.0
	local frame = { { 0, i * 256 }, { 1024, 256 } }
	local img = eapi.NewSpriteList(cloudFile, frame)
	local speed = direction * 700 / math.pow(2, i)
	p[i] = parallax.Continuous(pos, z, img, speed, 1024)
end

CloudParallax(0, { x = -464, y = -270 })
CloudParallax(1, { x = -464, y = -290 })
CloudParallax(2, { x = -464, y = -290 })
CloudParallax(3, { x = -464, y = -300 })

local frame = { { 0, 0 }, { 800, 480 } }
local bgImg = eapi.NewSpriteList(skyFile, frame)
eapi.NewTile(staticBody, { x = -400, y = -240 }, nil, bgImg, -100)

parallax.level2 = p
