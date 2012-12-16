#ifndef GAME2D_CONFIG_H
#define GAME2D_CONFIG_H

#include "common.h"

/* Cached configuration (mostly read from config.lua). */
struct {
        int             debug;
        int             fullscreen;
        int             flip;       /* True if device is flipped bottom up. */
        int             retina;     /* True if retina (high-density) display. */
        char            device[32]; /* Internal device name. */
        
        /*
         * Pixel scale refers to texture pixels not mapping directly onto world
         * coordinate space. If its value is 2, then a sprite of dimensions
         * 50x30 will map to world size 100x60.
         *
         * This has been introduced due to iPhone retina display devices. If we
         * want to stay compatible with older models (screen size 480x320),
         * while not sacrificing any presicion on the retina iPhones, then we
         * assume our world to have as many units as it would on the newer
         * phones. For the non-retina iPhones however, sprites are automatically
         * upscaled by a factor of 2.
         */
        unsigned        pixel_scale;
        
        /*
         * If true, application will dynamically update its data from online
         * server.
         */
        int             download_update;
        
        unsigned        FPSUpdateInterval;
        int             gameSpeed;
        uint32_t        defaultShapeColor;
        char            version[10];      /* Engine version. */
        char            location[128];    /* User application location path. */
        char            name[16];         /* User application name. */
        
        /*
         * grid_expand  Auto expand grid. Use when fine-tuning space
         *              partitioning.
         * grid_info    Print information about the grid when world is
         *              destroyed.
         * grid_many    How many objects in a cell are considered "many".
         */
        int             grid_expand;
        int             grid_info;
        unsigned        grid_many;
        
        /* Virtual screen size used by scripts. */
        unsigned        screen_width;
        unsigned        screen_height;
        
        int             use_desktop;      /* Use desktop size as window size. */
        unsigned        window_width;
        unsigned        window_height;
        float           w_t, w_b, w_l, w_r; /* Framebuffer quad coords. */
        
        /*
         * collision_dist       Expand shape bounding box by `collision_dist`
         *                      units to check for simultaneous collisions.
         * cam_vicinity_factor  Determines how far outside camera visibility
         *                      bodies remain active.
         */
        int             collision_dist;
        float           cam_vicinity_factor;
        
        /* Memory pool sizes. */
        struct poolsize_t {
                int world;
                int body;
                int tile;
                int shape;
                int group;
                int camera;
                int texture;
                int spritelist;
                int timer;
                int gridcell;
                int property;
                int collision;
                int touch;
                int hackevent;
                int bodytrace;
                int tiletrace;
                int shapetrace;
                int sound;
                int music;
        } poolsize;
} config;

int     cfg_read(const char *source);
void    cfg_close(void);

/* Retrieve configuration. */
float           cfg_get_float(const char *key);
void            cfg_get_cstr(const char *key, char *buf, unsigned bufsize);
uint32_t        cfg_get_color(const char *key);
int             cfg_get_int(const char *key);
int             cfg_get_bool(const char *key);
int             cfg_has_key(const char *key);

#define GET_CFG(key, method, default_value)                     \
            (cfg_has_key(key) ? method(key) : (default_value))

/* Modify configuration. */
void    cfg_set_str(const char *key, const char *value);

#endif  /* GAME2D_CONFIG_H */
