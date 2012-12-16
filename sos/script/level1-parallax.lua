-- 
-- ROAD
--
local p = { }
local frame = { { 0, 0 }, { 1024, 480 } }
local road = eapi.NewSpriteList("image/level1/layer0.png", frame)
p[0] = parallax.Continuous({ x = -464, y = -240 }, -10.0, road, -700, 1024)

-- 
-- LAYER1 
--
local pos = { x = -464, y = -208 }
local l1 = eapi.ChopImage("image/level1/layer1.png", { 1024, 256 })
p[1] = parallax.Continuous(pos, -20.0, l1, -500, 950, 0.2)
parallax.Animate(p[1], eapi.ANIM_REVERSE_LOOP, 16)

-- 
-- LAYER2
--
local pos = { x = -464, y = -60 }
local l2 = eapi.ChopImage("image/level1/layer2.png", { 1024, 128 })
p[2] = parallax.Continuous(pos, -30.0, l2, -350, 900, 0.2)
parallax.Animate(p[2], eapi.ANIM_REVERSE_LOOP, 16)

-- 
-- LAYER3
--
local pos = { x = -464, y = -40 }
local l3 = eapi.ChopImage("image/level1/layer3.png", { 1024, 128 })
p[3] = parallax.Continuous(pos, -40.0, l3, -300, 1000, 0.2)
parallax.Animate(p[3], eapi.ANIM_REVERSE_LOOP, 16)

-- 
-- BACKGROUND
--
local l4 = eapi.NewSpriteList("image/level1/layer4.png", frame)
p[4] = parallax.Continuous({ x = -464, y = -240 }, -50.0, l4, -50, 1024)

parallax.GetSparkVel = function()
	return vector.Scale(eapi.GetVel(p[4].body), 10)
end

parallax.level1 = p
