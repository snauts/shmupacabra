-- Program configuration.

local eapi = eapi or { }

Cfg = {
        name = "Shmupacabra",
	version = "devel",

	-- Display.
	scanlines	= true,
	fullscreen	= true,
	windowWidth	= 800,
	windowHeight	= 480,
	screenBPP	= 0,
	
	-- Sound.
	channels	= 16,
	frequency	= 22050,
	chunksize	= 512,
	stereo		= true,

	-- Debug things.
	debug		= false,
	useDesktop	= true,
	screenWidth	= 800,
	screenHeight	= 480,
	printExtensions = false,
	FPSUpdateInterval = 500,
	gameSpeed = 0,
        defaultShapeColor = {r=0,g=1,b=0},

	-- Default control scheme: actions mapped to keys.
	controls = {
		Left  = { "Left",  "Joy0 A0-" },
		Right = { "Right", "Joy0 A0+" },
		Up    = { "Up",    "Joy0 A1-" },
		Down  = { "Down",  "Joy0 A1+" },
		Shoot = { "X",     "Joy0 B1" },
		Bomb  = { "Z",     "Joy0 B2" },
		Pause = { "P",     "Joy0 B3" },

		UIUp    = { "Up",   "Joy0 A1-" },
		UIDown  = { "Down", "Joy0 A1+" },
		UIPause = "Escape",
		
		Select = "Return",
		Skip = "Space",
	},

        -- Engine config.
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
                timer      = 4000,
                gridcell   = 50000,
                property   = 20000,
                collision  = 1000
        },
        collision_dist = 1,
        cam_vicinity_factor = 0.5
}

return Cfg
