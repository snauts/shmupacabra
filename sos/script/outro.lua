dofile("script/parallax.lua")

parallax.direction = 1
parallax.sky = "image/blue-sun.png"
parallax.clouds = "image/blue-clouds.png"
dofile("script/level2-parallax.lua")
dofile("script/player.lua")

local function FixParallax(p)
	parallax.AddShape(p, { l = -2048, r = -2048 + 32, b = 0, t = 32 })
end
util.Map(FixParallax, parallax.level2)

local tile = util.WhiteScreen(150)
eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 1 })
local function FadeIn()
	local transparentBlack = { r = 0, g = 0, b = 0, a = 0 }
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, transparentBlack, 2, 0)
	eapi.AddTimer(staticBody, 2, function() eapi.Destroy(tile) end)
end
eapi.AddTimer(staticBody, 1, FadeIn)

local function DrawPlayer()
	local playerImg = eapi.ChopImage("image/player.png", { 64, 64 })
	local burnerName = { "image/player-flame.png", filter = true }
	local burnerImg = eapi.ChopImage(burnerName, { 64, 64 })

	local body = eapi.NewBody(gameWorld, { x = 320, y = -200 })
	local tile = eapi.NewTile(body, { x = -32, y = -32 }, nil, playerImg, 0)
	eapi.SetFrame(tile, 16)
	eapi.FlipX(tile, true)

	local size = { x = 24, y = 30 }
	local offset = { x = 18, y = -17 }
	local tile = eapi.NewTile(body, offset, size, burnerImg, -0.5)
	eapi.Animate(tile, eapi.ANIM_LOOP, 32, 0)
	local subPos = vector.Offset(offset, 2, 0)
	eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, subPos, 0.07, 0)
	local subSize = vector.Offset(size, 8, 0)
	eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, subSize, 0.07, 0)
end
DrawPlayer()

player.EnablePause()

local textVel = { x = 0, y = 32 }

local function ScrollLine(str)
	local pos = util.TextCenter(str, util.defaultFontset)
	local body = eapi.NewBody(gameWorld, vector.Offset(pos, 0, -256))
	util.PrintOrange(vector.null, str, 100, body, 0)
	util.DelayedDestroy(body, 512 / textVel.y)
	eapi.SetVel(body, textVel)
end

local function ScrollTable(strTable)
	local index = 1
	local function Emit()
		if strTable[index] then 
			ScrollLine(strTable[index])
			eapi.AddTimer(staticBody, 0.5, Emit)
			index = index + 1
		end		
	end
	Emit()
end

local function Scroll(strTable)
	return function() ScrollTable(strTable) end
end

local function FadingText(yOffs, str, font)
	local function FadeIn(tile)
		local color = eapi.GetColor(tile)
		eapi.SetColor(tile, util.invisible)
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, color, 2, 0)
	end
	local pos = vector.Offset(util.TextCenter(str, font), 0, yOffs)
	local tiles = util.PrintOrange(pos, str, 100, staticBody, 0, font)
	util.Map(FadeIn, tiles)
end

local function TheEnd()
	FadingText(0, txt.theEnd, util.bigFontset)	
end

local function ThankYou()
	FadingText(-32, txt.thankYou, util.defaultFontset)
end

local function FadeOut()
	local tile = util.WhiteScreen(150)
	local black = { r = 0, g = 0, b = 0, a = 1 }
	eapi.SetColor(tile, { r = 0, g = 0, b = 0, a = 0 })
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, black, 2, 0)
	eapi.FadeMusic(2.0)
end

local function Title()
	util.Goto("title")
end

local rainbowColors = {
	{ r = 1.00, g = 0.00, b = 0.00, a = 0.0 },
	{ r = 1.00, g = 0.00, b = 0.00, a = 0.8 },
	{ r = 1.00, g = 1.00, b = 0.00, a = 0.8 },
	{ r = 0.00, g = 1.00, b = 0.00, a = 0.8 },
	{ r = 0.00, g = 1.00, b = 1.00, a = 0.8 },
	{ r = 0.00, g = 0.00, b = 1.00, a = 0.8 },
	{ r = 0.44, g = 0.13, b = 0.38, a = 0.8 },
	{ r = 0.44, g = 0.13, b = 0.38, a = 0.0 },
}
local starOffset = { x = -4, y = -8 }
local starDstOffset = { x = -12, y = -8 }
local starName = { "image/white-star.png", filter = true }
local starImg = eapi.ChopImage(starName, { 16, 16 })
local delay = 0.2

local function RainbowStar(pos, vel)
	local index = 1
	local body = eapi.NewBody(gameWorld, pos)
	local tile = eapi.NewTile(body, starOffset, nil, starImg, -25)	
	eapi.Animate(tile, eapi.ANIM_LOOP, 48, util.Random())
	eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, starDstOffset, 0.2, 0)
	util.RandomRotateTile(tile)
	eapi.SetVel(body, vel)

	local function Colorize()
		local src = rainbowColors[index]
		local dst = rainbowColors[index + 1]
		if src and dst then
			eapi.SetColor(tile, src)
			eapi.AnimateColor(tile, eapi.ANIM_CLAMP, dst, delay, 0)
			eapi.AddTimer(body, delay, Colorize)
			index = index + 1
		else
			eapi.Destroy(body)
		end
	end
	Colorize()
end

local baseVel = { x = 0, y = -64 }
local function StarRain(body, baseAngle)
	local pos = eapi.GetPos(body)
	local function Emit()
		local angle = util.Random(-10, 10)
		local vel = vector.Rotate(baseVel, angle - baseAngle)
		RainbowStar(pos, vel)
		eapi.AddTimer(body, 0.1, Emit)
	end
	Emit()
end

local function RainBow()
	local delay = 0
	local center = { x = 0, y = -200 }
	local arm = { x = 0, y = 400 }
	for angle = -80, 80, 1 do
		local pos = vector.Rotate(arm, -angle)
		pos = vector.Add(center, pos)
		local body = eapi.NewBody(gameWorld, pos)
		util.Delay(body, delay, StarRain, body, angle)
		delay = delay + 0.05
	end
end

eapi.PlayMusic("sound/outro.ogg", 1, 0.5, 0.1)

util.DoEventsRelative( { {  3.0, Scroll(txt.storyEnd) }, 
			 { 20.0, Scroll(txt.credits) },
			 { 28.0, TheEnd }, 
			 {  1.0, ThankYou }, 
--			 {  4.0, RainBow },
			 {  6.0, FadeOut },
			 {  3.0, Title }, })
