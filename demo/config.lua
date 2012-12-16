-- Program configuration.

local eapi = eapi or { }

Cfg = {
        name = "Demo Game",
        version = "0.1",

        -- Display.
        fullscreen      = false,
        useDesktop      = true,         -- Use desktop size as window size.
        windowWidth     = 640,          -- Window width (ignored if useDesktop is true).
        windowHeight    = 480,          -- Window height (ignore if useDesktop is true).
                        
        -- Sound.
        channels        = 16,           -- Number of mixing channels.
        frequency       = 22050,        -- Use 44100 for 44.1KHz (CD audio).
        chunksize       = 512,          -- Less is slower but more accurate.
        stereo          = true,         -- Mono or stereo output.
        
        -- Virtual screen size used by game logic (do not touch!).
        screenWidth     = 640,
        screenHeight    = 480,

        -- Editor.
        loadEditor              = true,
        defaultShapeColor       = {r=0.0, g=0.8, b=0.4},
        selectedShapeColor      = {r=0.9, g=0,   b=0.5},
        invalidShapeColor       = {r=0.8, g=0.4, b=0.1},

        -- Debug things.
        sanityChecks    = true,         -- Enable sanity checking (assert, checktype, etc). Disable for release mode.
        printExtensions = false,        -- Print the OpenGL extension string.
        FPSUpdateInterval = 500,        -- FPS update interval in milliseconds.
        gameSpeed = 0,                  -- Negative values slow the game down positive values speed it up.

        -- Memory pool sizes.
        poolsize = {
                world      = 12,
                body       = 4000,
                tile       = 4000,
                shape      = 4000,
                group      = 100,
                camera     = 10,
                texture    = 100,
                spritelist = 200,
                sound      = 100,
                music      = 10,
                timer      = 1000,
                gridcell   = 20000,
                property   = 5000,
                collision  = 1000
        },
        
        -- Distance around shape to check for simultaneous collisions.
        collision_dist = 1,
        
        -- Defines offscreen area where invisible bodies remain active.
        cam_vicinity_factor = 0.5
}

-- Make sure the configuration is sane.
assert(Cfg.screenWidth >= 200 and Cfg.screenHeight >= 120)

assert(Cfg.channels > 0 and Cfg.channels <= 16)
assert(Cfg.frequency == 22050 or Cfg.frequency == 44100)
assert(Cfg.stereo == true or Cfg.stereo == false)
assert(Cfg.chunksize >= 256 and Cfg.chunksize <= 8192)

assert(Cfg.FPSUpdateInterval > 0);
assert(Cfg.gameSpeed >= -10 and Cfg.gameSpeed <= 10)
assert(Cfg.collision_dist >= 0)
assert(Cfg.cam_vicinity_factor >= 0)

return Cfg
