local menuCamera = nil

local function TintScreen()
	local tile = util.WhiteScreen(-100, menu.world)
	local color = { r = 0, g = 0, b = 0, a = 0 }
	eapi.SetColor(tile, util.SetColorAlpha(util.Gray(0), 0))
	local fadeColor = util.SetColorAlpha(util.Gray(0), 0.75)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, fadeColor, 0.25, 0)		
end

local function Text(pos, text)
	return util.PrintOrange(pos, text, 100, menu.world, 0)
end

local function GetWidth(items)
	local width = 0
	for i = 1, #items, 1 do
		width = math.max(string.len(items[i].text), width)
	end
	return (width + 2) * util.defaultFontset.size.x
end

local function Box(size, offset, body, light, alpha)
	local tiles = { }
	local function Save(tile)
		tiles[tile] = tile
	end
	alpha = alpha or 0.2
	light = light or 0.8
	body = body or staticBody
	local function Rectangle(size, offset, light)
		local tile = eapi.NewTile(body, offset, size, util.white, 95)
		eapi.SetColor(tile, util.SetColorAlpha(util.Gray(light), alpha))
		return tile
	end
	Save(Rectangle(size, offset, light))
	local sizeA = { x = size.x + 4, y = 2 }
	local sizeB = { x = 2, y = size.y }

	Save(Rectangle(sizeA, vector.Offset(offset, -2, -2), light - 0.3))
	Save(Rectangle(sizeB, vector.Offset(offset, -2,  0), light + 0.3))

	Save(Rectangle(sizeA, vector.Offset(offset, -2, size.y), light + 0.3))
	Save(Rectangle(sizeB, vector.Offset(offset, size.x, 0),  light - 0.3))
	return tiles
end

local function SmallAlpha(tile)
	util.SetTileAlpha(tile, 0.2)
end

local function Menu(items, box, index)	
	local w = GetWidth(items)
	if box then
		local boxW = w + 32
		local boxH = #items * util.defaultFontset.size.y
		boxH = boxH + 0.5 * (boxH - util.defaultFontset.size.y)
		util.DestroyTable(box)
		box = menu.Box({ x = boxW, y = boxH + 16 },
			       { x = -boxW / 2, y = -228 },
			       menu.world)
	end
	for i = 1, #items, 1 do
		util.DestroyTable(items[i].tiles)
		local text = ((index == i) and "> " or "  ") .. items[i].text
		local y = 1.5 * (#items - i) * util.defaultFontset.size.y
		items[i].tiles = Text({ x = -w / 2, y = -220 + y }, text)
		if items[i].grayOut then
			util.Map(SmallAlpha, items[i].tiles)
		end
	end
	return box
end

local function Show(items, pause, tint, box)
	local index = 1
	if pause then eapi.Pause(gameWorld) end
        menu.world = eapi.NewWorld("Menu", 5, util.sceneSize, 40)
	menuCamera = eapi.NewCamera(menu.world, vector.null, nil, nil, 1)
	if tint then TintScreen() end
	local function Move(dir)
		return function()
			local oldIndex = index
			index = math.max(1, index + dir)
			index = math.min(#items, index)
			if oldIndex ~= index then
				eapi.PlaySound(gameWorld, "sound/plop.ogg")
			end
			box = Menu(items, box, index)
		end
	end
	local function Select()
		if items[index].grayOut then
			eapi.PlaySound(gameWorld, "sound/warning.ogg", 0, 1)
		else
			eapi.PlaySound(gameWorld, "sound/inv-plop.ogg")
			util.MaybeCall(items[index].Func)
		end
	end
	local function BindAll()
		player.DisableAllInput()
		input.Bind("Up", false, util.KeyDown(Move(-1)))
		input.Bind("Down", false, util.KeyDown(Move(1)))
		input.Bind("UIUp", false, util.KeyDown(Move(-1)))
		input.Bind("UIDown", false, util.KeyDown(Move(1)))
		input.Bind("Shoot", false, util.KeyDown(Select))
		input.Bind("Select", false, util.KeyDown(Select))
		input.Bind("Skip", false, util.KeyDown(Select))
	end
	BindAll()
	Move(0)(true)
	return { BindAll = BindAll }
end

local function Stop()
	eapi.Resume(gameWorld)
	eapi.Destroy(menu.world)
end

local function Back()
	input.Bind("Up")
	input.Bind("Down")
	input.Bind("UIUp")
	input.Bind("UIDown")
	input.Bind("Shoot")
	input.Bind("Select")
	input.Bind("Skip")
	Stop()
	player.EnableAllInput()
end

local function FadingText(text, times, length, fadeTime, MiscFN, TextFN, yOff)
	TextFN = TextFN or util.BigTextCenter
	local function FadeOut(tile)
		local color = util.SetColorAlpha(eapi.GetColor(tile), 0)
		eapi.AnimateColor(tile, eapi.ANIM_CLAMP, color, length, 0)
	end
	local function WarningText()
		local body = eapi.NewBody(gameWorld, { x = 0, y = yOff or 160 })
		local tiles = TextFN(text, body, 0)
		local function Fade()
			util.DelayedDestroy(body, length)
			util.Map(FadeOut, tiles)
			util.MaybeCall(MiscFN)
		end
		eapi.AddTimer(body, fadeTime or 0.0, Fade)
	end
	util.Repeater(WarningText, times, 0.5)
end

local function TextFx(text, Fn)
	local count = string.len(text)
	local width = util.bigFontset.size.x * (count - 1)
	local flip = 4
	for x = -0.5 * width, 0.5 * width, 32 do 
		Fn({ x = x, y = 156 + flip })
		flip = -flip
	end
end

local function LetterExplode(pos)
	explode.Simple(pos, staticBody, 10.0)
end

local function TextExplode(text)
	TextFx(text, LetterExplode)
end

local function LetterBurst(pos)
	local body = eapi.NewBody(gameWorld, pos)
	explode.Circular(powerup.SmallStar, body, 10, 150)
	eapi.Destroy(body)
end

local function TextBurst(text)
	TextFx(text, LetterBurst)
end

local function WarningSound()
	eapi.PlaySound(gameWorld, "sound/warning.ogg", 0, 1)
end

local function IssueWarning(times)
	FadingText(txt.warning, times, 0.5, 0.0, WarningSound)
	state.boss = true
	player.SaveGame()
end

local function StrechSound()
	eapi.PlaySound(gameWorld, "sound/warning.ogg", 0, 1)
	eapi.PlaySound(gameWorld, "sound/strech.ogg", 0, 1)
end

local function FadeWarning()
	parallax.StopInterior(3)
	FadingText(txt.warning, 1, 3, 0.0, StrechSound)
	local function Print(text, body, shadow)
		local pos = util.TextCenter(text, util.defaultFontset)
		return util.PrintOrange(pos, text, 100, body, shadow)
	end
	FadingText(txt.eol, 1, 3, 0.0, nil, Print, 128)
end

local function FadingWarning()
	eapi.AddTimer(staticBody, 1, FadeWarning)
	IssueWarning(2)	
end

local function Warning()
	IssueWarning(17)
end

local function CompletionMessage(msg)
	menu.FadingText(msg, 1, 2, 1)
	eapi.PlaySound(gameWorld, "sound/success.ogg", 0, 1)
	TextBurst(msg)
end

local function Success()
	CompletionMessage(txt.success)
end

local function Blinking(text, offset, interval)
	local ret = { }
	local stop = false
	local alpha = 1.0
	local pos = util.TextCenter(text, util.defaultFontset)
	local tiles = util.PrintOrange(vector.Add(pos, offset), text)
	local function SetAlpha(tile)
		util.SetTileAlpha(tile, alpha)
	end
	local function Blink()
		if stop then
			util.DestroyTable(tiles)
		else
			util.Map(SetAlpha, tiles)
			local delay = (alpha > 0.5) and 1.0 or 0.75
			alpha = (alpha > 0.5) and 0.2 or 1.0
			eapi.AddTimer(staticBody, interval * delay, Blink)
		end
	end
	Blink()
	ret.Stop = function() 
		stop = true
	end
	ret.SetRate = function(val)
		interval = val
	end
	return ret	
end

local function BossMusic()
	eapi.PlayMusic("sound/boss-music.ogg", nil, 0.5, 0.1)
end

local function Mission(num)
	return function()
		local text = txt["mission" .. num]
		menu.FadingText(text, 1, 1, 1.5)
		eapi.PlayMusic("sound/level1.ogg", nil, 0.5, 0.1) -- FIXME
		menu.TextExplode(text)
	end
end

local function TitleMusic()
	eapi.PlayMusic("sound/title.ogg", nil, nil, 0.5)
end

local function StopMusic(time)
	return util.Closure(util.StopMusic, time)
end

menu = {
	FadingText = FadingText,
	TextExplode = TextExplode,
	FadingWarning = FadingWarning,
	CompletionMessage = CompletionMessage,
	TitleMusic = TitleMusic,
	BossMusic = BossMusic,
	StopMusic = StopMusic,
	Blinking = Blinking,
	Warning = Warning,
	Success = Success,
	Mission = Mission,
	Back = Back,
	Stop = Stop,
	Show = Show,
	Box = Box,
}
return menu
