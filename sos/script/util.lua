local function RandomElement(table)
	return table[util.Random(1, #table)]
end

local function Length(table)
	local count = 0
	for _, _ in pairs(table) do
		count = count + 1
	end
	return count
end

local function LoadFont(fontFile, gradientFile, spriteSize)
	spriteSize = spriteSize or { x = 8, y = 16 }
	local fontset = {
		sprites = eapi.ChopImage(fontFile, spriteSize),
		gradient = eapi.ChopImage(gradientFile, spriteSize),
		size = spriteSize
	}
	return fontset
end

local function Gray(level)
        return { r = level, b = level, g = level }
end

local function Print(pos, text, color, z, body, fontset, image)
	body = body or staticBody
	color = color or Gray(1.0)
	fontset = fontset or util.defaultFontset
	local sprites = fontset[image or "sprites"]
	local p = { x = pos.x, y = pos.y }

	local function PutChar(p)
		local tile = eapi.NewTile(body, p, fontset.size, sprites, z)
                eapi.SetColor(tile, color)
		return tile
	end

	local textTiles = {}
	for c = 1, #text do
		local tile = PutChar(p)
		local char = string.sub(text, c, c)
		eapi.SetFrame(tile, string.byte(char, 1) - 32);		
		p.x = p.x + fontset.size.x
		table.insert(textTiles, tile)
 	end
	return textTiles
end

local function PrintShadow(pos, str, color, z, body, shadow, fontset)
	z = z or 100
	shadow = shadow or 0.2
	color = color or util.Gray(1.0)
	fontset = fontset or util.defaultFontset
	local shadowColor = util.Gray(shadow)
	local shadowOffset = fontset.size.x / 8
	local tiles1 = Print(pos, str, color, z, body, fontset)
	pos = vector.Offset(pos, shadowOffset, -shadowOffset)
	local tiles2 = Print(pos, str, shadowColor, z - 0.01, body, fontset)
	return util.JoinTables(tiles1, tiles2)
end

local function PrintGradient(pos, str, c1, c2, z, body, shadow, fontset)
	z = z or 100
	fontset = fontset or util.defaultFontset
	local tiles1 = PrintShadow(pos, str, c1, z, body, shadow, fontset)
	local tiles2 = Print(pos, str, c2, z + 0.01, body, fontset, "gradient")
	return util.JoinTables(tiles1, tiles2)
end

local yellow = { r = 1.0, g = 1.0, b = 0.0 }
local orange = { r = 1.0, g = 0.3, b = 0.0 }
local function PrintOrange(pos, str, z, body, shadow, fontset)
	return PrintGradient(pos, str, yellow, orange, z, body, shadow, fontset)
end

local red = { r = 0.6, g = 0.0, b = 0.0 }
local function PrintRed(pos, str, z, body, shadow, fontset)
	return PrintGradient(pos, str, orange, red, z, body, shadow, fontset)
end

local cameraSize = nil
local function GetCameraSize()
	return cameraSize
end

local function WorldBounds(size)
	cameraSize = size
	size = vector.Scale(size, 4)
	return { l = -size.x, b = -size.y, r = size.x, t = size.y }
end

local function Filter()
	local pos = { x = -400, y = -240 }
	local filter = eapi.ChopImage({ "image/filter.png" }, { 800, 480 })
	local filterWorld = eapi.NewWorld("Menu", 5, util.sceneSize, 40)
	eapi.NewCamera(filterWorld, vector.null, nil, nil, 2)
	local tile = eapi.NewTile(filterWorld, pos, nil, filter, 0)
	eapi.Animate(tile, eapi.ANIM_LOOP, 16, 0)
end

local function KeyDown(Fn)
	return function(keyDown)
		if not keyDown then return end
		Fn()
	end
end

local function Closure(Fn, arg1, arg2, arg3)
	return function() Fn(arg1, arg2, arg3) end
end

local function DestroyClosure(obj)
	return function() eapi.Destroy(obj) end
end

local function DelayedDestroy(body, time)
	eapi.AddTimer(body, time, DestroyClosure(body))
end

local function DestroyTable(table)
	if type(table) == "table" then util.Map(eapi.Destroy, table) end
end

local function TransitionEffect()
	local pos = { x = -400, y = -240 }
	local body = eapi.NewBody(gameWorld, vector.null)
	local sprite = eapi.NewSpriteList("framebuffer")
	local tile = eapi.NewTile(body, pos, nil, sprite, 200)
	eapi.SetVel(body, { x = -1650, y = 0 })
	DelayedDestroy(body, 0.5)
end

local function PreloadSound(file)
        if type(file) == "table" then
		util.Map(PreloadSound, file)
        else
                handle = eapi.PlaySound(false, file, 0, 0)
                eapi.FadeSound(handle, 0)
        end
end

local function PreloadMusic(file)
	eapi.PlayMusic(file, nil, 0, 0)
	eapi.FadeMusic(0)
end

local function Preload()
	PreloadMusic("sound/boss-music.ogg")
	PreloadMusic("sound/level1.ogg")
	PreloadMusic("sound/title.ogg")

	PreloadSound({ "sound/explode.ogg",
		       "sound/available.ogg",		       
		       "sound/success.ogg",		       
		       "sound/warning.ogg",
		       "sound/screech.ogg",
		       "sound/strech.ogg",
		       "sound/cannon.ogg",
		       "sound/charge.ogg",
		       "sound/shoot.ogg",
		       "sound/loose.ogg",
		       "sound/burst.ogg",
		       "sound/laser.ogg",
		       "sound/spark.ogg",
		       "sound/plop.ogg",
		       "sound/life.ogg",
		       "sound/boom.ogg",
		       "sound/slam.ogg",
		       "sound/hit.ogg",
		       "sound/jet.ogg",
		       "sound/star.ogg", })
end

local sceneSize = { l = -4000, r = 4000, b = -2400, t = 2400 }

local function LoadDefaultFont()
	return LoadFont("image/default-font.png", "image/gradient-font.png")
end

local function LoadBigFont()
	local size = { x = 32, y = 64 }
	return LoadFont("image/big-font.png", "image/big-gradient.png", size)
end

local sounds = { }
local function PlaySound(world, file, interval, repeats, volume, fadeIn)
	local currTime = eapi.GetTime(world)
	local prevTime = sounds[file]
	if not prevTime or currTime - prevTime > interval then
		sounds[file] = currTime
		return eapi.PlaySound(gameWorld, file, repeats, volume, fadeIn)	
	end
end

local fontMap = { }
fontMap[0xc4] = { }
fontMap[0xc5] = { }

fontMap[0xc4][0x80] = string.char(128)
fontMap[0xc4][0x8c] = string.char(129)
fontMap[0xc4][0x92] = string.char(130)
fontMap[0xc4][0xa2] = string.char(131)
fontMap[0xc4][0xaa] = string.char(132)
fontMap[0xc4][0xb6] = string.char(133)
fontMap[0xc4][0xbb] = string.char(134)

fontMap[0xc5][0x85] = string.char(135)
fontMap[0xc5][0xa0] = string.char(136)
fontMap[0xc5][0xaa] = string.char(137)
fontMap[0xc5][0xbd] = string.char(138)

fontMap[0xc4][0x81] = string.char(144)
fontMap[0xc4][0x8d] = string.char(145)
fontMap[0xc4][0x93] = string.char(146)
fontMap[0xc4][0xa3] = string.char(147)
fontMap[0xc4][0xab] = string.char(148)
fontMap[0xc4][0xb7] = string.char(149)
fontMap[0xc4][0xbc] = string.char(150)

fontMap[0xc5][0x86] = string.char(151)
fontMap[0xc5][0xa1] = string.char(152)
fontMap[0xc5][0xab] = string.char(153)
fontMap[0xc5][0xbe] = string.char(154)

fontMap[0xe2] = { }
fontMap[0xe2][0x86] = { }
fontMap[0xe2][0x86][0x92] = string.char(156)
fontMap[0xe2][0x86][0x90] = string.char(157)
fontMap[0xe2][0x86][0x93] = string.char(158)
fontMap[0xe2][0x86][0x91] = string.char(159)

local function ConvertString(text)
	local index = 1
	local newText = ""
	local function GetChar(table)
		local char = string.sub(text, index, index)
		local byte = string.byte(char, 1)
		local entry = table[byte]
		index = index + 1
		if not entry then
			return char
		elseif type(entry) == "table" then
			return GetChar(entry)
		elseif type(entry) == "string" then
			return entry
		else
			return string.char(143) -- star
		end
	end
	while index <= #text do
		newText = newText .. GetChar(fontMap)
	end
	return newText
end

local function Convert(table)
	for i, value in pairs(table) do
		if type(value) == "table" then
			Convert(value)
		elseif type(value) == "string" then
			table[i] = ConvertString(value)
		else
			assert(true, "String Table Error")
		end
	end 	
end

local function Goto(scene)
	eapi.DrawToTexture("framebuffer")

	sounds = { }
	eapi.Clear()
	input.UnbindAll()
	state.level = scene

	-- step = 5 milliseconds, quad tree depth = 16
	gameWorld = eapi.NewWorld("Game", 5, sceneSize, 40)

	util.defaultFontset = LoadDefaultFont()
	util.bigFontset = LoadBigFont()

	camera = eapi.NewCamera(gameWorld, vector.null, nil, nil)
	eapi.SetBoundary(camera, WorldBounds(eapi.GetSize(camera)))
	eapi.SetColor(camera, util.Gray(0.0))

	staticBody = eapi.GetBody(gameWorld)
	if Cfg.scanlines then Filter() end
	eapi.RandomSeed(42)

	dofile("script/menu.lua")
	dofile("script/actor.lua")
	dofile("script/player.lua")
	dofile("script/explode.lua")
	Convert(dofile("script/text.en.lua"))
	dofile("script/" .. scene .. ".lua")

	TransitionEffect()
end

local function RandomError(a, b)
	a = a or "nil"
	b = b or "nil"
	print("Error in util.random(" .. a .. ", " .. b .. ")")
	assert(true, "RandomError")
end

local function Random(a, b)
	local rnd = eapi.Random() / 2147483648
	if a then
		if not(b) then
			b = a
			a = 1
		end
		if (a > b) then 
			RandomError(a, b)
		end
		return math.floor(rnd * (b - a + 1)) + a
	elseif b then
		RandomError(a, b)
	else
		return rnd
	end
end

local function JoinTables(a, b)
	local result = { }
	for _, v in pairs(a) do
		table.insert(result, v)
	end
	for _, v in pairs(b) do
		table.insert(result, v)
	end
	return result
end

local function InjectTable(dst, src)
	for i, v in pairs(src) do
		dst[i] = v
	end
	return dst
end

local function Sign(val)
	return (((val > 0) and 1) or -1)
end

local function MaybeCall(fn, ...)
	return ((type(fn) == "function") and fn(...)) or fn
end

local function Map(Fn, a, b)
	if a == nil then return nil end
	local new = { }
	for i, _ in pairs(a) do
		new[i] = (b and Fn(a[i], b[i])) or Fn(a[i])
	end
	return new
end

local function Member(item, table)
	for _, v in pairs(table) do
		if item == v then return v end
	end 
	return false
end

local function FileExists(n)
        local f = io.open(n)
        if f == nil then
                return false
        else
                io.close(f)
                return true
        end
end

local function DoEventsRelative(events, body)
        local i = 1
	local control = { }
        body = body or staticBody
        local function Do()
		if control.stop then return end
                events[i][2](Do)
                i = i + 1
                if events[i] and events[i][1] then
                        eapi.AddTimer(body, events[i][1], Do)
                end
        end
        eapi.AddTimer(body, events[1][1], Do)
	return control
end

local function DoEventsAbsolute(events, body)
	for i = #events, 2, -1 do
		events[i][1] = events[i][1] - events[i - 1][1]
	end
	return DoEventsRelative(events, body)
end

local function SetColorAlpha(color, alpha)
	color.a = alpha
	return color
end

local function SetTileAlpha(tile, alpha)
        local color = eapi.GetColor(tile)
        color.a = alpha
        eapi.SetColor(tile, color)
end

local function ChangeTileAlpha(tile, alpha)
        local color = eapi.GetColor(tile)
	if color.a > 0 then 
		color.a = alpha
		eapi.SetColor(tile, color)
	end
end

local function HideTile(tile)
	SetTileAlpha(tile, 0)
end

local function ShowTile(tile, alpha)
	SetTileAlpha(tile, alpha or 1)
end

local function RotateTile(tile, angle)
        eapi.SetAngle(tile, vector.Radians(angle))
end

local function RandomRotateTile(tile)
	RotateTile(tile, 360 * Random())
end

local function PadZeros(number, amount)
	local padded = ""
	amount = amount - 1
	local base = math.pow(10, amount)
	for i = 1, amount, 1 do
		if number / base >= 1.0 then break end
		padded = padded .. "0"
		base = base / 10
	end
	return padded .. number
end

local function WhiteScreen(z, body)
	body = body or staticBody
	local size = { x = 2000, y = 2000 }
	local offset = { x = -1000, y = -1000 }
	return eapi.NewTile(body, offset, size, util.white, z or 150)
end

local function Div(a, b)
	return math.floor(a / b)
end

local function TextCenter(text, fontset)
	return { x = -fontset.size.x * string.len(text) / 2,
		 y = -fontset.size.y / 2 }
end

local function BigTextCenter(text, body, shadow)
	local pos = TextCenter(text, util.bigFontset)
	return util.PrintOrange(pos, text, 100, body, shadow, util.bigFontset)
end

local function Repeater(FN, times, interval, body, delay)
	body = body or staticBody
	local function Repeat()
		FN()
		times = times - 1
		if times > 0 then
			eapi.AddTimer(body, interval, Repeat)
		end
	end
	eapi.AddTimer(body, delay or 0, Repeat)
end

local function StopMusic(time)
	eapi.FadeMusic(time or 0.5)
end

local function FixAngle(angle)
	return (angle + 180) % 360 - 180
end

local function FormatBool(value)
	return value and "true" or "false"
end

local function FormatPrimitive(value)
        if type(value) == "nil" then
                return "nil"
        end
        if type(value) == "string" then
                return "\"" .. value .. "\""
        end
        if type(value) == "number" then
                return "" .. value
        end
        if type(value) == "boolean" then
		return FormatBool(value)
        end
        error("unknown type")
end

local function Format(value)
        if type(value) == "table" then
                local str = "{"
                for name, value in pairs(value) do
                        if type(name) == "string" then
                                str = str .. name .. "="
                        end
                        str = str .. Format(value) .. ","
                end
                return str .. "}"
        else
                return FormatPrimitive(value)
        end
end

local function Level(i, noMusic)
	local function GoLevel()
		state.levelNum = i
		util.Goto("level" .. i)
	end
	return function()
		if state.boss then
			GoLevel()
		else
			util.Goto("story")
			if not noMusic then menu.TitleMusic() end
			local text = txt["story" .. i]
			story.Roll(text, GoLevel)
		end
	end
end

local function NewGame()
	state.lastLives = player.startLives
	state.lastStars = 0
	state.boss = false
	util.Level(1, true)()
end

local function AnimateRotation(tile, speed, angle)
	angle = angle or 0
	util.RotateTile(tile, angle)
	angle = vector.Radians(angle + 360)
	eapi.AnimateAngle(tile, eapi.ANIM_LOOP, vector.null, angle, speed, 0)
end

local function Delay(body, time, Fn, obj1, obj2)
	eapi.AddTimer(body, time, function() Fn(obj1, obj2) end)
end

local function StopAcc(obj, acc)
	eapi.SetAcc(obj.body, acc or vector.null)
end

util = {
	yellow = yellow,
	orange = orange,
	FixAngle = FixAngle,
	Noop = function() end,
	sceneSize = sceneSize,
	golden = 0.61803399,
	fibonacci = 222.4922364,
	SetColorAlpha = SetColorAlpha,
	invisible = { r = 1, g = 1, b = 1, a = 0 },
	DoEventsRelative = DoEventsRelative,
	DoEventsAbsolute = DoEventsAbsolute,
	RandomRotateTile = RandomRotateTile,
	ChangeTileAlpha = ChangeTileAlpha,
	AnimateRotation = AnimateRotation,
	DelayedDestroy = DelayedDestroy,
	DestroyClosure = DestroyClosure,
	BigTextCenter = BigTextCenter,
	RandomElement = RandomElement,
	GetCameraSize = GetCameraSize,
	PrintGradient = PrintGradient,
	DestroyTable = DestroyTable,
	SetTileAlpha = SetTileAlpha,
	PrintShadow = PrintShadow,
	WhiteScreen = WhiteScreen,
	PrintOrange = PrintOrange,
	InjectTable = InjectTable,
	FormatBool = FormatBool,
	TextCenter = TextCenter,
	FileExists = FileExists,
	JoinTables = JoinTables,
	PlaySound = PlaySound,
	StopMusic = StopMusic,
	MaybeCall = MaybeCall,
	RotateTile = RotateTile,
	PrintRed = PrintRed,
	Repeater = Repeater,
	PadZeros = PadZeros,
	HideTile = HideTile,
	ShowTile = ShowTile,
	StopAcc = StopAcc,
	Closure = Closure,
	KeyDown = KeyDown,
	Preload = Preload,
	NewGame = NewGame,
	Format = Format,
	Member = Member,
	Random = Random,
	Length = Length,
	Level = Level,
	Delay = Delay,
	Goto = Goto,
	Sign = Sign,
	Gray = Gray,
	Div = Div,
	Map = Map,
}
return util
