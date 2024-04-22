local lives = 3
local slowSpeed = 150
local playerSpeed = 300
local bombDuration = 1.0
local extraLife = 200
local bombRecharge = 0.3
local bombRent = 60
local bombCost = 30
local animFPS = 64

local LEFT   = 1
local RIGHT  = 2
local UP     = 3
local DOWN   = 4

actor.Cage("Blocker", 0.6, 0.5)

local function PlayerSprite(name)
	return eapi.ChopImage(name, { 64, 64 })
end

local BOOST  = 1
local SMALL  = 2
local NORMAL = 3

local function AnimatePlayer(player, stop)
	local start = eapi.GetFrame(player.tile)
	local time = math.abs(stop - start) / animFPS
	eapi.AnimateFrame(player.tile, eapi.ANIM_CLAMP, start, stop, nil, time)
	eapi.AnimateFrame(player.glow, eapi.ANIM_CLAMP, start, stop, nil, time)
end

local function Animate(player)
	if player.dead then return end

	local vel = eapi.GetVel(player.body)
	if vel.y == 0 then
		AnimatePlayer(player, 16)
	elseif vel.y > 0 then
		AnimatePlayer(player, 31)
	elseif vel.y < 0 then
		AnimatePlayer(player, 0)
	end

	util.Map(util.HideTile, player.flame)
	if vel.x > 0.001 then
		util.ShowTile(player.flame[BOOST], player.alpha)
	elseif vel.x < -0.001 then
		util.ShowTile(player.flame[SMALL], player.alpha)
	else
		util.ShowTile(player.flame[NORMAL], player.alpha)
	end
end

local playerTable = { }

local function Get(index)
	return playerTable[index or util.Random(1, #playerTable)]
end

local function Pos(index)
	return actor.GetPos(Get(index))
end

local shootFile = { "image/shooting.png", filter = true }
local shootAnim = eapi.ChopImage(shootFile, { 64, 64 })

local function AddMuzzleTiles(player)
	player.muzzle = { }
	local z = player.z - 0.1
	local body = player.body
	local function AddTile(offset, startAnim)
		local tile = eapi.NewTile(body, offset, nil, shootAnim, z)
		eapi.Animate(tile, eapi.ANIM_LOOP, 64, startAnim)
		eapi.FlipX(tile, true)
		util.HideTile(tile)
		return tile
	end
	player.muzzle[1] = AddTile({ x = -20, y = -24 }, 0.0)
	player.muzzle[2] = AddTile({ x = -18, y = -40 }, 0.5)
end

local burnerName = { "image/player-flame.png", filter = true }
local burnerImg = eapi.ChopImage(burnerName, { 64, 64 })

local function AddFlames(player)
	player.flame = { }
	local z = player.z - 0.1
	local body = player.body
	local function Flame(offset, size)
		local tile = eapi.NewTile(body, offset, size, burnerImg, z)
                eapi.FlipX(tile, true)
		eapi.Animate(tile, eapi.ANIM_LOOP, 32, 0)
		local subPos = vector.Offset(offset, -5, 0)
		eapi.AnimatePos(tile, eapi.ANIM_REVERSE_LOOP, subPos, 0.07, 0)
		local subSize = vector.Offset(size, 8, 0)
		eapi.AnimateSize(tile, eapi.ANIM_REVERSE_LOOP, subSize, 0.07, 0)
		util.HideTile(tile)
		return tile
	end
	player.flame[NORMAL] = Flame({ x = -42, y = -17 }, { x = 24, y = 30 })
	player.flame[SMALL]  = Flame({ x = -36, y = -13 }, { x = 16, y = 22 })
	player.flame[BOOST]  = Flame({ x = -50, y = -19 }, { x = 36, y = 34 })
	util.ShowTile(player.flame[NORMAL], player.alpha)
end

local function PlayerTransparency(player, alpha)
	player.alpha = alpha
	local function ChangeAlpha(tile)
		util.ChangeTileAlpha(tile, alpha)
	end
	ChangeAlpha(player.tile)
	util.Map(ChangeAlpha, player.flame)
end

local function PlayerStartup(obj)	
	local function Unprotect()
		player.SetInvincible(obj, false)
	end
	local function Go()
		eapi.SetVel(obj.body, vector.null)
		eapi.SetAcc(obj.body, vector.null)
		eapi.AddTimer(obj.body, 3, Unprotect)
		player.EnableInput(obj)
	end
	eapi.SetAcc(obj.body, { x = -350, y = 0 })
	eapi.SetVel(obj.body, { x = 350, y = 0 })
	eapi.AddTimer(obj.body, 0.5, Go)
	player.SetInvincible(obj, true)
end

local function MakeGlow(obj)
	local color = util.invisible
	local glow = PlayerSprite("image/stripes.png")
	obj.glow = eapi.NewTile(obj.body, obj.offset, nil, glow, obj.z + 0.01)
	eapi.AnimateColor(obj.glow, eapi.ANIM_REVERSE_LOOP, color, 0.2, 0)
	eapi.SetFrame(obj.glow, 16)
end

local function Simple(pos, index)
	local obj = { class = "Player",
		      invCounter = 0,
		      shootTable = { },
		      pos = pos, z = 19,
		      bulletHeight = 8,
		      speed = playerSpeed,
		      vel = vector.null,
		      moves = { 0, 0, 0, 0 },
		      offset = { x = -32, y = -32 }, 
		      bb = { l = -4, r = 8, b = -4, t = 4 },
		      index = index,
		      alpha = 1.0 }

	obj = actor.Create(obj)
	obj.sprite = PlayerSprite("image/player.png")
	obj.tile = actor.MakeTile(obj)
	eapi.SetFrame(obj.tile, 16)
	AddMuzzleTiles(obj)
	AddFlames(obj)
	MakeGlow(obj)
	obj.RemoveShoot = function(shoot)
		obj.shootTable[shoot] = nil
	end
	playerTable[index] = obj
	return obj
end

local function Create(pos, index)
	local obj = Simple(pos, index)
	PlayerStartup(obj)
	return obj
end

local function DisableInput(player)
        input.Bind("Left")
        input.Bind("Right")
        input.Bind("Up")
        input.Bind("Down")
        input.Bind("Shoot")
        input.Bind("Bomb")
end

local function HidePlayer(player)
	actor.DeleteShape(player)
	util.HideTile(player.tile)
	util.HideTile(player.glow)
	util.Map(util.HideTile, player.flame)
	util.MaybeCall(player.DoShooting, nil, true)
	eapi.SetVel(player.body, vector.null)
	player.dead = true
	DisableInput(player)
end

local function PlayerStarBurst(player)
	local vel = { x = 0, y = 200 }
	for i = 1, 3, 1 do
		vel = vector.Scale(vector.Rotate(vel, 36), 1.0 + 0.2 * i)
		bullet.FullArc(actor.GetPos(player), vel, powerup.Star, 5)
	end
end

local function Quit()
	util.Goto("title")	
end

local function Resume()
	menu.Back()
	eapi.ResumeMusic()
end

local function Retry()
	util.Goto(state.level)
end

local function Menu(text)
	util.BigTextCenter(text, menu.world, 0)
	eapi.PauseMusic()
end

local function GameOver()	
	menu.Show({ { text = txt.retry, 
		      Func = Retry },
		    { text = txt.quit,
		      Func = Quit }, },
		  true, true, true)
	Menu(txt.gameOver)

	eapi.RandomSeed(os.time())
	local msg = util.RandomElement(txt.loose)
	local pos = util.TextCenter(msg, util.defaultFontset)
	util.PrintOrange(vector.Offset(pos, 0, -32), msg, nil, menu.world, 0)
end

local function GamePause()
	menu.Show({ { text = txt.resume, 
		      Func = Resume },
		    { text = txt.retry, 
		      Func = Retry },
		    { text = txt.quit,
		      Func = Quit }, },
		  true, true, true)
end

local function SimplePause()
	menu.Show({ { text = txt.resume, 
		      Func = Resume },
		    { text = txt.quit,
		      Func = Quit }, },
		  true, true, true)
end

local function Pause()
	if state.level == "outro" then
		SimplePause()
	else
		GamePause()
	end
	Menu(txt.paused)
end

local function KillFlash(delay, color)
	local tile = util.WhiteScreen(150)
	eapi.AddTimer(staticBody, delay, util.DestroyClosure(tile))
	eapi.SetColor(tile, color or { r = 1, g = 1, b = 1, a = 1 })
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, util.invisible, delay, 0)
end

local function KillPlayer(player)
	eapi.PlaySound(gameWorld, "sound/loose.ogg")
	explode.Area(vector.null, player.body, 10, 32, 0.05)
	PlayerStarBurst(player)
	HidePlayer(player)
	KillFlash(0.5)

	if powerup.DecLives() then
		actor.Delete(player)
		Create(player.pos, player.index)
	else
		eapi.AddTimer(staticBody, 2, GameOver)
	end
end

local function PlayerVsBullet(pShape, bShape)
	local player = actor.store[pShape]
	if not player.invincible then
		local bullet = actor.store[bShape]
		bullet.Explode(bullet)
		KillPlayer(player)
	end
end

local function PlayerVsMob(pShape, mShape)
	local playerObj = actor.store[pShape]
	if not playerObj.invincible then
		local mob = actor.store[mShape]
		player.DamageMob(mob, 10)
		KillPlayer(playerObj)
	end
end

local frame = { { 65, 33 }, { 30, 30 } }
local shield = eapi.NewSpriteList({ "image/misc.png", filter = true }, frame)
local function Ring(player)
	local interval = 0.2
	local body = player.body
	local white = util.Gray(1.0)
	local function AnimateRing(type, alpha)
		local color = util.SetColorAlpha(white, alpha)
		eapi.AnimateColor(player.ring, type, color, interval, 0)
	end
	local function AnimateClamp(Fn, data)
		Fn(player.ring, eapi.ANIM_CLAMP, data, interval, 0)
	end
	if player.invincible then
		local z = player.z + 0.01
		local size = { x = 80, y = 64 }
		local offset = { x = -48, y = -32 }
		if player.ring then eapi.Destroy(player.ring) end
		player.ring = eapi.NewTile(body, offset, size, shield, z)
		eapi.SetColor(player.ring, util.SetColorAlpha(white, 0.3))
		AnimateRing(eapi.ANIM_REVERSE_LOOP, 0.1)
	elseif player.ring then
		eapi.AddTimer(body, interval, util.DestroyClosure(player.ring))
		AnimateClamp(eapi.AnimateSize, { x = 160, y = 128 })
		AnimateClamp(eapi.AnimatePos, { x = -96, y = -64 })
		AnimateRing(eapi.ANIM_CLAMP, 0.0)
		player.ring = nil
	end
end

local function SetInvincible(player, value)
	player.invCounter = player.invCounter + (value and 1 or -1)
	player.invincible = (player.invCounter > 0)
	PlayerTransparency(player, player.invincible and 0.4 or 1.0)
	Ring(player)
end

actor.SimpleCollide("Player", "Mob", PlayerVsMob)
actor.SimpleCollide("Player", "Bullet", PlayerVsBullet)
actor.SimpleCollide("Blocker", "Player", actor.Blocker)

local function GetDirection(moves)
	return { x = moves[RIGHT] - moves[LEFT], y = moves[UP] - moves[DOWN] }
end

local function SetSpeed(player, speed)
	if speed then player.speed = speed end
	eapi.SetVel(player.body, vector.Normalize(player.vel, player.speed))
end

local function Move(player, axis)
	return function(keyDown)
		player.moves[axis] = (keyDown and 1) or 0
		player.vel = GetDirection(player.moves)
		SetSpeed(player)
		Animate(player)
	end
end

local shootInterval = 0.02
local bombVelocity = { x = 600, y = 0 }
local shootVelocity = { x = 1200, y = 0 }

local function ShootVelocity(angle)
	return vector.Rotate(shootVelocity, angle)
end

local function ShootPosition(player, height)
	local width = 8
	if not height then
		player.bulletHeight = -player.bulletHeight
		height = player.bulletHeight
		width = 20
	end
	return vector.Offset(actor.GetPos(player), width, height)
end

local function EmitShoot(sprite, offset, bb, player, angle, velocity, height)
	local init = { class = "Shoot",
		       sprite = sprite,		       		       
		       offset = offset,
		       pos = ShootPosition(player, height),
		       velocity = velocity or ShootVelocity(angle),
		       OnDelete = player.RemoveShoot,
		       player = player,
		       angle = angle,
		       tail = { },
		       bb = bb,
		       z = player.z - 0.1, }
	local shoot = actor.Create(init)
	eapi.Animate(shoot.tile, eapi.ANIM_LOOP, 64, 0)
	util.RotateTile(shoot.tile, angle)
	return shoot
end

local shootBox = actor.Square(2)
local shootOffset = { x = -32, y = -32 }
local bombTargetSize = { x = 128, y = 1024 }
local bombTargetOffset = { x = -80, y = -512 }
local bombTargetColor = { r = 1.0, g = 0.5, b = 0.5, a = 0.1 }
local function EmitBombShoot(...)
	local shoot = EmitShoot(shootAnim, shootOffset, shootBox, ...)

	local tile = eapi.Clone(shoot.tile)
	util.SetTileAlpha(tile, 0.9)
	eapi.SetDepth(tile, shoot.z - 0.01)	
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, bombTargetColor, 1, 0)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, bombTargetSize, 1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, bombTargetOffset, 1, 0)
	shoot.tail[1] = tile

	shoot.damage = 7
	return shoot
end

local flameOffset = { x = -32, y = -32 }
local flameBox = { l = 16, r = 24, b = -4, t = 4 } 
local flameFile = { "image/rocket-flame.png", filter = true }
local flameImg = eapi.ChopImage(flameFile, { 64, 64 })
local flameBurst = eapi.ChopImage("image/flame-burst.png", { 64, 64 })

local tailColor = { { r = 1.0, g = 0.9, b = 0.9, a = 0.75 },
		    { r = 1.0, g = 0.7, b = 0.7, a = 0.50 },
		    { r = 1.0, g = 0.5, b = 0.5, a = 0.25 }, }

local tailOffset = { { x =  -64, y = -32 },
		     { x =  -96, y = -32 },
		     { x = -128, y = -32 }, }

local tailTargetSize = { }
local tailTargetOffset = { }

for i = 1, 3, 1 do
	local q = 1.0 + 1.2 * i
	local offset = tailOffset[i]
	tailTargetSize[i] = vector.Scale({ x = 64, y = 64 }, q)
	tailTargetOffset[i] = { x = offset.x, y = offset.y * q }
end

local function AddTailTile(shoot, i)
	local offset = tailOffset[i]
	local z = shoot.z - 0.01 * i
	local tile = eapi.NewTile(shoot.body, offset, nil, shoot.sprite, z)
	eapi.Animate(tile, eapi.ANIM_LOOP, 64, 0)
	util.RotateTile(tile, shoot.angle)
	util.HideTile(tile)

	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, tailTargetSize[i], 1, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, tailTargetOffset[i], 1, 0)

	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, tailColor[i], 0.25, 0)
	shoot.tail[i] = tile
end

local function EmitDefaultShoot(...)
	local shoot = EmitShoot(flameImg, flameOffset, flameBox, ...)
	shoot.hitAngle = shoot.angle
	shoot.hitAnim = flameBurst
	shoot.damage = 1

	for i = 1, 3, 1 do AddTailTile(shoot, i) end

	return shoot
end

local function BurstSound()
	return eapi.PlaySound(gameWorld, "sound/burst.ogg", -1, 0.1)
end

local function Shoot(player)
	local sound = nil
	local pending = false
	local shooting = false
	local angleInc = 0.1
	local timers = { }
	local golden = 0
	local angle = 0
	local function Shooting()
		local finalAngle = 0
		if player.speed == slowSpeed then
			angle = angle + angleInc
			if math.abs(angle) > 1 then angleInc = -angleInc end
			finalAngle = angle + 1.0 * (util.Random() - 0.5)
		else
			golden = (golden + util.golden) % 1
			finalAngle = -(golden - 0.1) * player.bulletHeight
		end
		EmitDefaultShoot(player, finalAngle)
			
		if shooting then
			eapi.AddTimer(player.body, shootInterval, Shooting)
			util.Map(util.ShowTile, player.muzzle)
		else
			util.Map(util.HideTile, player.muzzle)
		end
		pending = shooting
	end
	local function Slow()
		SetSpeed(player, slowSpeed)
	end
	local function Stop()
		shooting = false
		if sound then
			eapi.FadeSound(sound, 0.1)
			sound = nil
		end
	end
	local function StopTimer(index)
		if timers[index] then
			eapi.CancelTimer(timers[index])
			timers[index] = nil
		end
	end
	local function AddTimer(index, FN)
		timers[index] = eapi.AddTimer(player.body, 0.2, FN)
	end
	player.DoShooting = function(keyDown, imediate)
		StopTimer(1)
		StopTimer(2)
		if keyDown then
			shooting = true
			AddTimer(1, Slow)
			sound = sound or BurstSound()
			if not pending then
				Shooting()
			end
		elseif imediate then
			pending = false
			Stop()
		elseif not keyDown then
			SetSpeed(player, playerSpeed)
			AddTimer(2, Stop)
		end
	end
	return player.DoShooting
end

local function AlterShoot(shoot)
	if not shoot.altered then
		eapi.SetVel(shoot.body, shoot.velocity)
		eapi.SetAcc(shoot.body, vector.null)
	end
	shoot.altered = true
end

local function ChargeSound()
	return eapi.PlaySound(gameWorld, "sound/charge.ogg", 0, 1.0)
end

local function Flash(alpha)
	local tile = util.WhiteScreen(150)
	eapi.AddTimer(staticBody, 0.05, util.DestroyClosure(tile))
	eapi.SetColor(tile, { r = 1, g = 1, b = 1, a = alpha or 0.5 })
end

local function Discarge(player)
	if player.charge then
		local function InvincibilityOff()
			SetInvincible(player, false)
		end
		eapi.FadeSound(player.charge, 0.1)			
		util.Map(AlterShoot, player.shootTable)
		eapi.PlaySound(gameWorld, "sound/laser.ogg", 0, 1.0)
		eapi.AddTimer(player.body, 2.0, InvincibilityOff)
		eapi.CancelTimer(player.drainTimer)
		eapi.CancelTimer(player.emitTimer)
		eapi.Destroy(player.white)
		player.charge = nil
		powerup.Recharge()
		Flash()

		if player.discargeTimer then 
			eapi.CancelTimer(player.discargeTimer)
			player.discargeTimer = nil
		end
	end
end

local blinkColor = { r = 1, g = 1, b = 1, a = 0.8 }

local function AnimateBlinking(player)
	player.white = util.WhiteScreen(150)
	eapi.SetColor(player.white, util.invisible)
	eapi.Animate(player.white, eapi.ANIM_LOOP, 20, 0)
	eapi.AnimateColor(player.white, eapi.ANIM_CLAMP,
			  blinkColor, bombDuration, 0)
end

local function Bomb(player)
	local angle = 0
	local interval = 0.05
	local timeLeft = bombDuration
	local delay = (bombDuration / bombRent) - 0.0001
	local function DiscargePlayer()
		Discarge(player)
	end
	local function DiscargeTimer()
		return eapi.AddTimer(player.body, bombDuration, DiscargePlayer)
	end
	local function EmitSingleShoot(i)
		angle = angle + util.fibonacci
		local adjust = timeLeft / bombDuration
		local normalVelocity = vector.Rotate(bombVelocity, angle)
		local scale = math.min(20, 1 / math.max(timeLeft, 0.01))
		local variation = adjust * scale * (0.4 + 0.1 * util.Random())
		local velocity = vector.Scale(normalVelocity, variation)
		local shoot = EmitBombShoot(player, angle, velocity, 0)
		eapi.SetAcc(shoot.body, vector.Scale(shoot.velocity, -scale))
		eapi.NewShape(shoot.body, nil, actor.Square(8), "Bomb")
		player.shootTable[shoot] = shoot

		-- replace shoot.velocity for AlterShoot function
		shoot.velocity = normalVelocity
	end
	local function StarDrain(fromTimer)
		player.drainTimer = eapi.AddTimer(player.body, delay, StarDrain)
		if fromTimer and not powerup.DecCharge() then
			DiscargePlayer()
		end
	end
	local function Emitter()
		for i = 1, 10, 1 do EmitSingleShoot() end
		player.emitTimer = eapi.AddTimer(player.body, interval, Emitter)
		timeLeft = timeLeft - interval
	end
	return function(keyDown)
		if not keyDown then
			DiscargePlayer()
		elseif powerup.DecCharge(bombCost) then
			AnimateBlinking(player)
			player.discargeTimer = DiscargeTimer()
			player.charge = ChargeSound()
			SetInvincible(player, true)
			angle = 360 * util.Random()
			timeLeft = 1
			StarDrain()
			Emitter()
		else
			eapi.PlaySound(gameWorld, "sound/shoot.ogg")
		end
	end
end

local function EnableInput(player)
	player.ShootFn = player.ShootFn or Shoot(player)
	input.Bind("Left", true, Move(player, LEFT))
	input.Bind("Right", true, Move(player, RIGHT))
	input.Bind("Up", true, Move(player, UP))
	input.Bind("Down", true, Move(player, DOWN))
	input.Bind("Shoot", true, player.ShootFn)
	input.Bind("Bomb", false, Bomb(player))
end

local hitCount = 10
local hitSize = { x = 2, y = 2 }
local hitOffset = { x = -32, y = -32 }
local hitAnimFile = { "image/hit-sparks.png", filter = true }
local hitAnim = eapi.ChopImage(hitAnimFile, { 64, 64 })

local function HitTile(shoot, angle, zAdjust)
	local z = shoot.z - zAdjust
	local sprite = shoot.hitAnim or hitAnim
	local tile = eapi.NewTile(shoot.body, hitOffset, nil, sprite, z)
	eapi.Animate(tile, eapi.ANIM_CLAMP, 128, 0)
	util.RotateTile(tile, angle)
	return tile
end

local hitTailSize = { x = 128, y = 128 }
local hitTailOffset = { x = -96, y = -64 }
local hitTailColor = { r = 1.0, g = 0.5, b = 0.5, a = 0.1 }
local function ExpandTail(tile)
	util.SetTileAlpha(tile, 0.9)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, hitTailColor, 0.25, 0)
	eapi.AnimateSize(tile, eapi.ANIM_CLAMP, hitTailSize, 0.25, 0)
	eapi.AnimatePos(tile, eapi.ANIM_CLAMP, hitTailOffset, 0.25, 0)
end

local hitTailColor = { r = 1.0, g = 0.8, b = 0.8, a = 0.5 }
local function FadeSparks(tile)
	eapi.AnimateColor(tile, eapi.ANIM_CLAMP, hitTailColor, 0.25, 0)
end

local function ShootHit(shoot, mob)
	local angle = shoot.hitAngle or 360 * util.Random()
	FadeSparks(HitTile(shoot, angle, 0))
	ExpandTail(HitTile(shoot, angle, 0.01))
	eapi.SetVel(shoot.body, eapi.GetVel(mob.body))
	eapi.SetAcc(shoot.body, vector.null)
	shoot.player.shootTable[shoot] = nil
	actor.DelayedDelete(shoot, 0.25)
	util.DestroyTable(shoot.tail)
	eapi.Destroy(shoot.tile)

	if hitCount > 0 or mob.health < 10 then
		hitCount = hitCount - 1
	else
		hitCount = util.Random(5, 10)
		explode.Splinters(shoot.body, shoot.z, 150, 60)
	end
	util.PlaySound(gameWorld, "sound/hit.ogg", 0.1, 0, 0.05)
end

local function DamageMob(mob, amount)
	actor.Blink(mob)
	mob.health = mob.health - amount
	if mob.health <= 0 then
		mob.Shoot(mob)
	end
end

local function ShootMob(aShape, sShape)
	local mob = actor.store[aShape]
	local shoot = actor.store[sShape]
	ShootHit(shoot, mob)
	util.MaybeCall(mob.Hit, mob, shoot)
	DamageMob(mob, shoot.damage)
end

actor.SimpleCollide("Mob", "Shoot", ShootMob)

local sparkOffset = { x = -8, y = -4 }
local sparkAnim = eapi.ChopImage("image/street-sparks.png", { 64, 64 })
local function Sparks(bShape, aShape, resolve)
	local _, obj = actor.Blocker(bShape, aShape, resolve)

	if util.Random() > 0.4 then return end

	local pos = vector.Add(actor.GetPos(obj), sparkOffset)
	local body = eapi.NewBody(gameWorld, pos)
	local tile = eapi.NewTile(body, hitOffset, nil, sparkAnim, -0.5)

	eapi.SetVel(body, parallax.GetSparkVel())
	eapi.SetAcc(body, { x = 0, y = 500 * util.Random() })
	eapi.Animate(tile, eapi.ANIM_CLAMP, 64 - 16 * util.Random(), 0)
	util.DelayedDestroy(body, 0.5)
	util.RandomRotateTile(tile)
end

actor.SimpleCollide("Street", "Player", Sparks)

local function MobHitsStreet(sShape, aShape)
	local mob = actor.store[aShape]
	if mob.ignoreStreet then return end
	-- street has impressive stopping power
	eapi.SetAcc(mob.body, vector.null)
	eapi.SetVel(mob.body, { x = -700, y = 0 })
	if state.level == "level1" then
		explode.Decal(mob.body, mob.decalOffset)
	end
	mob.Shoot(mob)
end

actor.SimpleCollide("Street", "Mob", MobHitsStreet, nil, false)

local function DisablePause()
	input.Bind("Pause")
	input.Bind("UIPause")
end

local function EnablePause()
	input.Bind("Pause", false, util.KeyDown(Pause))
	input.Bind("UIPause", false, util.KeyDown(Pause))
end

local function DisableAllInput()
	util.Map(DisableInput, playerTable)
	DisablePause()
end

local function EnableAllInput()
	util.Map(EnableInput, playerTable)
	EnablePause()
end

local function LevelEnd()
	local function End(player)
		actor.DeleteShape(player)
		eapi.SetAcc(player.body, { x = 800, y = 100 })
		local function Stop()
			eapi.SetVel(player.body, vector.null)
			eapi.SetAcc(player.body, vector.null)
		end
		eapi.AddTimer(player.body, 1.5, Stop)		
	end	
	util.Map(End, playerTable)
	state.lastStars = state.stars
	state.lastLives = state.lives
	parallax.Ascend(parallax[state.level])
	state.boss = false
end

local function SaveGame()
        local f = io.open(saveGame, "w")
        if f then
		state.lastStars = state.stars
		state.lastLives = state.lives
		local num = string.sub(state.level, 6, 6)
		f:write("state.lastStars = " .. state.lastStars .. "\n")
		f:write("state.lastLives = " .. state.lastLives .. "\n")
		f:write("state.levelNum = " .. (state.levelNum or num) .. "\n")
		f:write("state.boss = " .. util.FormatBool(state.boss) .. "\n")
		io.close(f)
        end
end

local function Start(shouldSave, light, alpha)
	player.Create({ x = -0.5 * util.GetCameraSize().x, y = 0 }, 1)
	powerup.SetStats(light, alpha)
	if shouldSave then
		player.SaveGame()
	end
	player.EnablePause()
end

player = {
	Get = Get,
	Pos = Pos,
	Flash = Flash,
	Start = Start,
	Create = Create,
	Simple = Simple,
	shield = shield,
	flame = flameImg,
	startLives = lives,
	bombRent = bombRent,
	bombCost = bombCost,
	LevelEnd = LevelEnd,
	SaveGame = SaveGame,
	speed = playerSpeed,
	KillFlash = KillFlash,
	DamageMob = DamageMob,
	extraLife = extraLife,
	EnableInput = EnableInput,
	EnablePause = EnablePause,
	bombRecharge = bombRecharge,
	SetInvincible = SetInvincible,
	fullCharge = bombRent + bombCost,
	DisableAllInput = DisableAllInput,
	EnableAllInput = EnableAllInput,
	sparkOffset = hitOffset,
	sparkAnim = sparkAnim,
	burner = burnerImg,
	Move = Move,
	Bomb = Bomb,
	Shoot = Shoot,
}
return player
