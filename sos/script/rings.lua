local ringsFile = { "image/rings.png", filter = true }
local ringsImg = eapi.ChopImage(ringsFile, { 256, 256 })

local function End(obj)
	local pos = eapi.GetPos(obj.body)
	player.KillFlash(0.5, { r = 1, g = 1, b = 1, a = 0.75 })
	eapi.PlaySound(gameWorld, "sound/boom.ogg")
	for i = 1, 50, 1 do
		local x = util.Random(416, 544)
		local y = pos.y + util.Random(-32, 32)
		local vel = { x = -util.Random(384, 768), y = 0 }
		vel = vector.Rotate(vel, util.Random(-5, 5))
		local star = powerup.Star({ x = x, y = y }, vel)
		eapi.SetAcc(star.body, vector.null)
	end
	actor.Delete(obj)
end

local offset = { x = 48, y = 0 }
local function Explosions(obj)
	for angle = 0, 359, 90 do
		local arm = vector.Rotate(offset, angle + 22.5)
		explode.Simple(arm, obj.body, obj.z + 1)
	end
	util.Delay(obj.body, 0.1, Explosions, obj)
end

local function Explode(obj)
	if obj.neighbor then
		obj.neighbor.neighbor = nil
		obj.neighbor = nil
	else
		eapi.AddTimer(staticBody, 1, obj.Continuation or util.Noop)
	end
	actor.DeleteShape(obj)
	eapi.SetStepC(obj.body, eapi.STEPFUNC_STD)
	eapi.Unlink(obj.body)
	eapi.SetVel(obj.body, vector.null)
	local pos = eapi.GetPos(obj.body)
	eapi.SetAcc(obj.body, { x = 2 * (400 - pos.x + 128), y = 0 })
	eapi.Animate(obj.tile, eapi.ANIM_LOOP, obj.dir * 128, 0)
	util.Delay(obj.body, 1, End, obj)
	eapi.Destroy(obj.parentBody)
	obj.parentBody = nil
	Explosions(obj)
end

local function Sparks(body, obj)
	if obj.parentBody and obj.neighbor and obj.type > 0 then
		local pos = eapi.GetPos(body)
		eapi.PlaySound(gameWorld, "sound/spark.ogg")
		local vel = bullet.HomingVector(pos, player.Pos())
		bullet.FullArc(pos, vel, bullet.CyanSpin, 10)
		vel = vector.Scale(vector.Rotate(vel, 18), 1.3)
		bullet.FullArc(pos, vel, bullet.CyanSpin, 10)
	end
end

local function Put(dir, Continuation)
	local eye = false
	local stop = false
	local start = false
	local ampliture = 64
	local rushInTime = 2
	local startSpeed = 448
	local rushInAcc = startSpeed / rushInTime
	local body = eapi.NewBody(gameWorld, { x = 640, y = 32 })
	eapi.SetVel(body, { x = -startSpeed, y = -dir * ampliture })
	local function SetAcc()
		eapi.SetAcc(body, { x = rushInAcc, y = dir * ampliture })
		dir = -dir
	end	
	local function Switch()
		eapi.AddTimer(body, 2, Switch)
		SetAcc()
	end
	Switch()
	local function Invert(obj)
		if not obj.parentBody then return end
		local velocity = eapi.GetVel(body)
		velocity.y = -velocity.y
		eapi.SetVel(body, velocity)
		SetAcc()
	end
	local init = { health = 500,
		       class = "Mob",		       
		       sprite = ringsImg,
		       parentBody = body,
		       offset = { x = -128, y = -144 },
		       pos = { x = -dir * 96, y = 0 },
		       Continuation = Continuation,
		       Shoot = Explode,
		       type = dir,
		       jerk = 16,
		       dir = 1,
		       z = 1, }
	local Pattern = nil
	local obj = actor.Create(init)
	local bulletsInRow = 5
	local counter = bulletsInRow
	local eyeVector = bullet.velocity
	local function EyePattern()
		if stop then return end
		local z = bullet.z_epsilon
		local pos = eapi.GetPos(obj.body, gameWorld)
		bullet.FullArc(pos, eyeVector, bullet.Eye, 5)
		eyeVector = vector.Rotate(eyeVector, 2)
		bullet.z_epsilon = z
		if counter > 1 then
			eapi.AddTimer(obj.body, 0.04, Pattern)
			counter = counter - 1
		else
			eapi.AddTimer(obj.body, 0.4, Pattern)
			counter = bulletsInRow
		end
	end
	local function AkuPattern()
		if stop then return end
		local pos = eapi.GetPos(obj.body, gameWorld)
		local vel = bullet.HomingVector(player.Pos(), pos)
		bullet.Arc(pos, vel, bullet.Aku, 3, 45)
		eapi.AddTimer(obj.body, 1.2, Pattern)
	end
	Pattern = function()
		if not obj.parentBody then return end
		if eye then EyePattern() else AkuPattern() end
	end
	local function MakeShapes()
		actor.MakeShape(obj, { b = -88, t = 56, l = -48, r = 48 })
		actor.MakeShape(obj, { b = -64, t = 32, l = -88, r = 88 })	
		eapi.SetVel(body, { x = 0, y = eapi.GetVel(body).y })
		eapi.SetAcc(body, { x = 0, y = -dir * ampliture })
		rushInAcc = 0
		start = true
		Pattern()
	end
	eapi.AddTimer(obj.body, rushInTime, MakeShapes)
	eapi.SetStepC(obj.body, eapi.STEPFUNC_ROT, 0.5 * math.pi)
	local function Resume()
		stop = false
		Pattern()
	end
	local function Stop()
		stop = true
		eapi.AddTimer(obj.body, 1.5, Resume)
	end
	local function Reverse()
		if start then 
			Sparks(body, obj)
			if not obj.neighbor then return end
		end
		eapi.SetVel(obj.body, vector.Scale(eapi.GetVel(obj.body), -1))
		eapi.Animate(obj.tile, eapi.ANIM_LOOP, obj.dir * 32, 0)
		eapi.AddTimer(obj.body, 4, Reverse)
		eapi.AddTimer(obj.body, 3, Stop)
		obj.dir = -obj.dir
		eye = not eye
		Invert(obj)
	end
	Reverse()
	return obj
end

local function Launch(Continuation)
	local ring1 = Put(-1, Continuation)
	local ring2 = Put(1, Continuation)
	ring1.neighbor = ring2
	ring2.neighbor = ring1
end

rings = {
	Launch = Launch,
}
return rings
