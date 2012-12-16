--
-- Part of game engine API that doesn't require any low level access and is
-- easier to do in Lua than in C anyway. The actual C API can be found in
-- src/eapi_Lua.c. Everything here is in the `eapi` namespace table, just
-- like the real API. Names of private functions and variables (those not
-- meant to be called/accessed by user scripts) begin with two underscores.
--

require("input")

eapi.idToObjectMap = {}
local lastID = 0

--
-- Animation types (added to eapi from the engine during startup):
--      eapi.ANIM_LOOP
--      eapi.ANIM_CLAMP
--      eapi.ANIM_CLAMP_EASEIN
--      eapi.ANIM_CLAMP_EASEOUT
--      eapi.ANIM_CLAMP_EASEINOUT
--      eapi.ANIM_REVERSE_CLAMP
--      eapi.ANIM_REVERSE_LOOP
--

--
-- Predefined C step functions:
--      eapi.STEPFUNC_STD
--

--
-- Generate a new object ID,
-- store object in `eapi.idToObjectMap`, and return the ID.
--
local function GenID(obj)        
        lastID = lastID + 1
        local ID = lastID
        eapi.idToObjectMap[ID] = obj
        return ID
end

--
-- Generate a new object ID for user arguments.
--
local function GenArgID(args)
        local argID = 0
        if #args ~= 0 then
                argID = GenID(args)
        end
        return argID
end

--
-- Reset all engine-side and script-side state.
--
function eapi.Clear()
        -- Clear engine-side state.
        eapi.__Clear()
        
        -- Clear client-side (script) state.
        eapi.idToObjectMap = {}
        lastID = 0
end

--
-- Set an object's step function. If `nil`, then current step function is unset.
--
function eapi.SetStep(obj, stepFunc, ...)
        -- Remove previous step function from eapi.idToObjectMap.
        local stepFuncID, argID = eapi.__GetStep(obj)
        if stepFuncID then
                eapi.idToObjectMap[stepFuncID] = nil
                if argID ~= 0 then
                        eapi.idToObjectMap[argID] = nil
                end
        end
        
        stepFuncID = nil
        if stepFunc then
                stepFuncID = GenID(stepFunc)
        end
        eapi.__SetStep(obj, stepFuncID, GenArgID(arg))
end

--
-- Set an object's step function. Unlike SetStep(), `stepFunc` must not refer to
-- a Lua function but should be a C function pointer instead.
--
function eapi.SetStepC(obj, stepFunc, ...)
        -- Remove previous user args from eapi.idToObjectMap.
        local stepFuncID, argID = eapi.__GetStep(obj)
        if argID ~= 0 and eapi.idToObjectMap[argID] then
                eapi.idToObjectMap[argID] = nil
        end
        eapi.__SetStepC(obj, stepFunc, GenArgID(arg))
end

--
-- Register a timer function to be called at a specified time.
--
-- obj          World or Body object. Body timers are called only when the
--              body is visible or almost visible on screen. World timers
--              behave like normal timer functions and will be called when their
--              time comes.
-- when         How many seconds until timer is called.
-- func         Timer function.
--
function eapi.AddTimer(obj, when, func, ...)
        return eapi.__AddTimer(obj, when, GenID(func), GenArgID(arg))
end

--
-- Set function that handles keyboard input. The function will be executed
-- whenever a key is either pressed or released. The prototype for it is
-- like this:
--
--      KeyFunc({scancode=?, keycode=?}, pressed)
-- 
-- where `scancode` is a physical key location-dependent identifier
-- (SDL_Scancode), and keycode is a character layout-dependent identifier
-- (SDL_Keycode). `pressed` is a boolean value indicating whether the key
-- has been pressed of released.
--
-- To get key name from its scancode or vice versa, use these functions:
--
--      eapi.GetKeyName(scancode)
--      eapi.GetKeyFromName(scancodeName)
--
function eapi.BindKeyboard(func, ...)
        eapi.__BindKeyboard(GenID(func), GenArgID(arg))
end

--
-- Set function that handles joystick buttons. Prototype:
--
--      JoyButton(joystickID, buttonID, pressed)
--
function eapi.BindJoystickButton(func, ...)
        eapi.__BindJoystickButton(GenID(func), GenArgID(arg))
end

--
-- Set function that handles joystick axis movement. Prototype:
--
--      JoyAxis(joystickID, axisID, value)
--
function eapi.BindJoystickAxis(func, ...)
        eapi.__BindJoystickAxis(GenID(func), GenArgID(arg))
end

--
-- Register collision handler.
--
--      world           World object.
--      groupA/B	Each shape belongs to a group. The name of this group
--                      is supplied as argument to eapi.NewShape().
--                      eapi.Collide() accepts as arguments the names of two
--                      groups, and registers a collision handler to call when
--                      two shapes belonging to these groups collide.
--                      groupNameA can be the same as groupNameB. In that case,
--                      shapes within the same group will collide.
--      func            Callback function that will handle collisions between
--                      pairs of shapes from the two groups.
--                      Pass in `nil` to remove collision handler for a
--                      particular pair. In this case the rest of the arguments
--                      must not be present (or nil as well).
--      upd		If true, collision events are received continuously
--                      (each step). If false, collision function is called when
--                      collision between two shapes starts (shapes touch after
--                      being separate), and then when it ends ("separation"
--                      callback).
--      priority        Priority determines the order in which collision
--                      handlers are called for a particular pair of shapes.
--                      So lets say we have registered handlers for these two
--                      pairs:
--                              "Player" vs "Ground"
--                              "Player" vs "Exit".
--                      Now a shape belonging to group "Player" can be
--                      simultaneously colliding with both a "Ground" shape and
--                      an "Exit" shape. If priorities are the same for both
--                      handlers, then the order in which they will be executed
--                      is undetermined. We can, however, set the priority of
--                      "Player" vs "Ground" handler to be higher, so that it
--                      would always be executed first.
--
function eapi.Collide(world, groupA, groupB, func, upd, priority, ...)
        if not func then
                eapi.__Collide(world, groupA, groupB, nil)
                return
        end
        eapi.__Collide(world, groupA, groupB, GenID(func), upd, priority,
                       GenArgID(arg))
end

--
-- Whenever it's necessary to invoke a function if only its ID in the
-- `eapi.idToObjectMap` table is known, the engine calls eapi.__CallFunc() with
-- the function ID as argument.
--
-- funcID       ID that maps to a Lua function.
-- argID        ID of extra arguments that must be
--		looked up in `eapi.idToObjectMap`.
--              Zero means that there are no saved user arguments.
-- remove       Remove function from the eapi.idToObjectMap table once the
--              function has finished.
--
function eapi.__CallFunc(funcID, argID, remove, ...)
        local idToObjectMap = eapi.idToObjectMap
        local func = idToObjectMap[funcID]
        if argID == 0 then
                if remove then
                        idToObjectMap[funcID] = nil
                end
                return func(unpack(arg))
        end
        
        local savedArgs = eapi.idToObjectMap[argID]
        if remove then
                idToObjectMap[funcID] = nil
                idToObjectMap[argID] = nil
        end
        
        -- Append `savedArgs` to `arg` because we cannot unpack() both in one
        -- function call. (Doing `func(unpack(arg), unpack(savedArgs))` does not
        -- work!).
        local sz = #arg
        for i, v in ipairs(savedArgs) do
                arg[sz + i] = v
        end
        return func(unpack(arg))
end
