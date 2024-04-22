local speed = 0.05

local function Print(pos, text)
	return util.PrintOrange(pos, text, 0, staticBody, 0.25)
end

local function PrintTable(textTable, pos, rows, chars)
	local tiles = { }
	for i = 1, rows - 1, 1 do
		local rowTiles = Print(pos, textTable[i])
		pos = vector.Offset(pos, 0, -1.5 * util.defaultFontset.size.y)
		tiles = util.JoinTables(tiles, rowTiles)
	end
	local subString = string.sub(textTable[rows], 1, chars)
	local lastTiles = Print(pos, subString)
	tiles = util.JoinTables(tiles, lastTiles)
	return tiles
end

local function TextCenter(textTable)
	local width = 0
	for i = 1, #textTable, 1 do
		width = math.max(string.len(textTable[i]), width)
	end
	local size = util.defaultFontset.size
	return { x = -0.5 * width * size.x, y = 0.75 * #textTable * size.y }
end

local function Roll(textTable, Done)
	local row = 1
	local chars = 0
	local tiles = { }
	local finished = false
	local timeInterval = speed
	local pos = TextCenter(textTable)

	local function Advance()
		util.DestroyTable(tiles)
		tiles = PrintTable(textTable, pos, row, chars)

		chars = chars + 1
		if chars > string.len(textTable[row]) then
			row = row + 1
			chars = 0
		end
		if row > #textTable then
			finished = true
		else
			eapi.AddTimer(staticBody, timeInterval, Advance)
		end
	end
	Advance()

	local function Skip(keyDown)
		if keyDown and finished then
			util.MaybeCall(Done)
		elseif keyDown then
			finished = true
			row = #textTable
			chars = string.len(textTable[row])
		end
	end
	input.Bind("Shoot", false, Skip)
	input.Bind("Skip", false, Skip)
	input.Bind("Select", false, Skip)
	input.Bind("UIPause", false, Skip)
end

story = {
	Roll = Roll,
}
return story
