local function Add(a, b)
	return { x = a.x + b.x, y = a.y + b.y }
end

local function Sub(a, b)
	return { x = a.x - b.x, y = a.y - b.y }
end

local function Scale(v, f)
	return { x = v.x * f, y = v.y * f }
end

local function Reverse(v)
	return { x = -v.x, y = -v.y }
end

local function Length(v)
	return math.sqrt(v.x * v.x + v.y * v.y)
end

local function Distance(a, b)
	return Length(Sub(a, b))
end

local function Normalize(v, amount)
	amount = amount or 1.0
	local len = Length(v)
	if len == 0 then
		return v
	else
		return Scale(v, amount / len)
	end
end

local function Round(v, idp)
	local mult = 10^(idp or 0)
	return {x=math.floor(v.x * mult + 0.5) / mult,
		y=math.floor(v.y * mult + 0.5) / mult}
end

local function IsZero(v)
	return (v.x == 0 and v.y == 0)
end

local function Check(v)
	return type(v) == "table" and v.x and v.y
end

local function Offset(v, x, y)
	return { x = v.x + x, y = v.y + y }
end

local function Radians(theta)
	return theta * (math.pi / 180)
end

local function Degrees(theta)
	return 180 * theta / math.pi
end

local function Rotate(v, theta)
	theta = Radians(theta)
	local cs = math.cos(theta)
	local sn = math.sin(theta)
	return { x = v.x * cs - v.y * sn,
		 y = v.x * sn + v.y * cs }
end

local function Floor(p)
	return { x = math.floor(p.x), y = math.floor(p.y) }
end

local function Print(p)
	print("v: " .. p.x .. " " .. p.y)
end

local function Rnd(vel, amount)
	local function Rnd() return amount * (util.Random() - 0.5) end
	return vector.Offset(vel, Rnd(), Rnd())
end			     

local function Angle(v)
	return 180 * math.atan2(v.y, v.x) / math.pi
end

local null = { x = 0, y = 0 }

vector = {
	Angle = Angle,
	Print = Print,
	Rnd = Rnd,
	Add = Add,
	Sub = Sub,
	Floor = Floor,
	Scale = Scale,
	Reverse = Reverse,
	Round = Round,
	Length = Length,
	Distance = Distance,
	Normalize = Normalize,
	Radians = Radians,
	Degrees = Degrees,
	Check = Check,
	Offset = Offset,
	Rotate = Rotate,
	null = null,
}
return vector
