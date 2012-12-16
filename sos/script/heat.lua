local speed = 0.1
local amplitude = 8
local stripCount = 64
local stripHeight = 512 / stripCount

local heatImg = eapi.ChopImage("image/heat.png", { 1024, stripHeight })

local function AddStrip(i, body, z, pos1, pos2, freq, TileFn)
	local fraction = (stripCount - i) / stripCount
	local start = ((freq or 1) * speed * fraction) % (2 * speed) 
	local tile = eapi.NewTile(body, pos1, nil, heatImg, z)	
	eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, pos2, speed, start)
	util.MaybeCall(TileFn, tile)
	eapi.SetFrame(tile, i)
end

local function Put(body, offset, z, freq, TileFn)
	for i = 0, stripCount - 1, 1 do
		local y = 512 - (i + 1) * stripHeight
		local pos1 = vector.Add(offset, { x = 0, y = y })
		local pos2 = vector.Add(offset, { x = -amplitude, y = y })
		AddStrip(i, body, z, pos1, pos2, freq, TileFn)
	end
end

heat = {
	Put = Put,
}
return heat
