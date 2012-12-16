
local keybind = {}      -- Map key names to functions.
local keystate = {}     -- Keep track of key state.

--
-- Unbind all keys.
--
local function UnbindAll()
        keybind = {}
end

--
-- Execute function bound to key.
--
local function DoKey(keyname, pressed)
        local binding = keybind[keyname]

        -- Update key state.
        local wasPressed = keystate[keyname]
        keystate[keyname] = pressed
        
        local function xor(a, b)
                return ((a and not(b)) or (b and not(a)))
        end
        
        -- Be sure to ignore repeat `key-down` events and also
        -- `key-up` event if key was not previously pressed.        
        if binding and xor(pressed, wasPressed) then
                binding.func(pressed, unpack(binding.arg))
        end
end

--
-- Execute function bound to joystick button.
--
local function DoJoyButton(joy, button, pressed)
        DoKey("Joy"..joy.." B"..(button+1), pressed)
end

--
-- Execute function(s) bound to joystick axis.
--
local function DoJoyAxis(joy, axis, value)
        local neg = "Joy"..joy.." A"..axis.."-"
        local pos = "Joy"..joy.." A"..axis.."+"
        
        if math.abs(value) < 0.3 then
                DoKey(neg, false)
                DoKey(pos, false)
                return
        end
        
        if value < 0 then
                DoKey(pos, false)
                DoKey(neg, true)
        else
                DoKey(neg, false)
                DoKey(pos, true)
        end
end

--
-- Set/restore normal key handling.
--
local function RestoreNormal()
        eapi.BindKeyboard(function(key, pressed)
                DoKey(key.scancode, pressed)
        end)
        eapi.BindJoystickButton(DoJoyButton)
        eapi.BindJoystickAxis(DoJoyAxis)
end

--
-- Bind key name to function that's executed when corresponding key is
-- pressed or released.
--
-- keyname         Key name or a list of key names.
--                 For keyboard keys see SDL/src/events/SDL_keyboard.c file.
-- immediate       Execute function immediately if key already pressed.
-- fn              User function. Use `nil` to remove binding.
--
local function Bind(keynames, immediate, fn, ...)
        if type(keynames) ~= "table" then
                keynames = {keynames}
        end
        
        -- Set functions that handle all key presses (if not already set).
        if next(keybind) == nil then
                RestoreNormal()
        end
        
        -- Create/remove entries in `keybind` table.
	for i, v in ipairs(keynames) do
                -- Key name may be the key's scancode, its scancode name or a
                -- joystick button/axis name.
                local name = v
                if type(name) ~= "number" then
                        name = eapi.GetKeyFromName(v) or v
                end
                
		if fn then
                        if keybind[name] then
                                print("input.lua warning: "..name.." already "..
                                      "bound.")
                        end
                        keybind[name] = { func = fn, arg = arg }
                        
                        -- Simulate `key-down` event if key is already pressed.
                        if immediate and keystate[name] then
                                fn(true, unpack(arg))
                        end
		else
                        local binding = keybind[name]
                        if binding then
                                -- Simulate `key-up` event if key was pressed.
                                if keystate[name] then
                                        binding.func(false, unpack(binding.arg))
                                end
                                keybind[name] = nil
                        end
                end
        end
end

--
-- Wait for any keyboard key or joystick button press and execute argument
-- function once that happens. Normal key handling is restored afterwards.
--
local function WaitAnyKey(except, fn, ...)
        if type(except) ~= "table" then
                except = {except}
        end

        local function AnyKey(pressed)
                if pressed then
                        fn(unpack(arg))
                        RestoreNormal()
                end
        end

        eapi.BindKeyboard(function(key, pressed)
                if util.HasValue(except, eapi.GetKeyName(key.scancode)) then
                        RestoreNormal()
                        DoKey(key.scancode, pressed)
                else
                        AnyKey(pressed)
                end
        end)
        eapi.BindJoystickButton(function(joy, button, pressed)
                local name = "Joy"..joy.." B"..(button+1)
                if util.HasValue(except, name) then
                        RestoreNormal()
                        DoKey(name, pressed)
                else
                        AnyKey(pressed)
                end
        end)
end

--
-- actions = { "Shoot", "Bomb", {"Left", "Right"}, {"Up", "Down"} }
--
-- PreFn(actionNumber, actionName)
-- PostFn(actionNumber, actionName, keyName)
-- FinishedFn(controls, cancelled)
--
local function SetupControls(actions, PreFn, PostFn, cancelKey, FinishedFn)
        checktype(actions, "table", PreFn, "function", PostFn, "function",
                  FinishedFn, "function")
        assert(#actions > 0)
        
        local controls = {}     -- Resulting controls: action -> key
        local usedKeys = {}     -- Keys already used: keyname -> true
        
        -- Inform user about first action.
        actions = util.DeepCopy(actions)
        local actionNumber = 1
        local firstAction = actions[1]
        if type(firstAction) == "table" then
                assert(#firstAction == 2)
                PreFn(actionNumber, firstAction[1])
        else
                PreFn(actionNumber, firstAction)
        end
        
        local function DoBinding(keyname, opposingKeyname)
                -- Check if key has been already used previously.
                if usedKeys[keyname] then
                        return
                end
                usedKeys[keyname] = true
        
                local action = table.remove(actions, 1)
                local opposingAction = nil
                if type(action) == "table" then
                        assert(#action == 2)
                        opposingAction = action[2]
                        action = action[1]
                        
                        -- If no oppsite direction key exists, put the opposite
                        -- action as next (regular) action.
                        if not opposingKeyname then
                                table.insert(actions, 1, opposingAction)
                                opposingAction = nil
                        end
                end
                
                -- Do control assignment.
                controls[action] = keyname
                
                -- Inform user that action was successfully assigned to key.
                PostFn(actionNumber, action, tostring(keyname))
                actionNumber = actionNumber + 1
                
                -- Handle opposing action.
                if opposingAction then
                        PreFn(actionNumber, opposingAction)
                        controls[opposingAction] = opposingKeyname
                        PostFn(actionNumber, opposingAction, tostring(opposingKeyname))
                        actionNumber = actionNumber + 1
                end
                
                if #actions == 0 then
                        RestoreNormal()
                        FinishedFn(controls, true)
                        return
                end
                
                -- Prepare user for next action.
                local nextAction = actions[1]
                if type(nextAction) == "table" then
                        assert(#nextAction == 2)
                        PreFn(actionNumber, nextAction[1])
                else
                        PreFn(actionNumber, nextAction)
                end
        end
        
        -- Inspect keyboard keys.
        eapi.BindKeyboard(function(key, pressed)
                if not pressed then
                        return
                end
                local keyname = eapi.GetKeyName(key.scancode)
                if cancelKey and keyname == cancelKey then
                        -- User interrupeted.. restore normal key handling.
                        RestoreNormal()
                        FinishedFn(controls, false)
                        return
                end
                if not keyname then
                        keyname = key.scancode
                end
                local opposite = {
                        Left = "Right",
                        Right = "Left",
                        Up = "Down",
                        Down = "Up"
                }
                DoBinding(keyname, opposite[keyname])
        end)
        
        -- Inspect joystick buttons.
        eapi.BindJoystickButton(function(joy, button, pressed)
                if not pressed then
                        return
                end
                DoBinding("Joy"..joy.." B"..(button+1))
        end)
        
        -- Inspect joystick axes.
        eapi.BindJoystickAxis(function(joy, axis, value)
                -- Compose complete axis name.
                local axis = "Joy"..joy.." A"..axis
                
                -- Make sure user cleanly pressed in the axis direction.
                if math.abs(value) < 0.8 then
                        keystate[axis] = false
                        return
                end
                
                if not keystate[axis] then
                        local dir, opdir = "+", "-"
                        if value < 0 then
                                dir, opdir = opdir, dir
                        end
                        DoBinding(axis..dir, axis..opdir)
                        
                        -- State: pressed.
                        keystate[axis] = true
                end
        end)
end

input = {
        Bind            = Bind,
        WaitAnyKey      = WaitAnyKey,
        SetupControls   = SetupControls,
        UnbindAll       = UnbindAll,
}
