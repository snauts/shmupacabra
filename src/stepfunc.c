#include <assert.h>
#include "eapi_C.h"
#include "stepfunc.h"
#include "util_lua.h"
#include "assert_lua.h"

void
stepfunc_std(lua_State *L, void *body, intptr_t data)
{
        UNUSED(L);
        UNUSED(data);
        
        vect_f pos = GetPos(body);
        vect_f vel = GetVel(body);
        vect_f acc = GetAcc(body);
        
        /* Step delta time. */
        float dt = GetWorld(body)->step_sec;
        
        /* Update velocity. */
        vel.x += acc.x * dt;
        vel.y += acc.y * dt;
        SetVel(body, vel);
        
        /* Update position. */
        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        SetPos(body, pos);
}

/*
 * Set basic rotation step function like so:
 *     eapi.SetStepC(body, eapi.STEPFUNC_ROT, dvel, ivel=0, mult=1)
 *
 * where `ivel` is the initial angular velocity of the body, `dvel` is the
 * desired (final) angular velocity, and `mult` is a multiplier that determines
 * how fast this velocity is achieved.
 *
 * Rotational values (radius, angular velocities, and multiplier) are stored as
 * regular velocity and acceleration values:
 *
 *     vel.x        current angular velocity
 *     vel.y        desired angular velocity (read from user args)
 *     acc.x        distance to parent (calculated from initial position)
 *     acc.y        multiplier (how fast desired velocity is achieved)
 */
void
stepfunc_rot(lua_State *L, void *body, intptr_t data)
{
        vect_f pos = GetPos(body);
        vect_f vel = GetVel(body);
        vect_f acc = GetAcc(body);
                         
        /* Zero Y acceleration means rotation values haven't been stored yet. */
        if (acc.y == 0.0) {
                /* Distance to parent. */
                acc.x = sqrtf(pos.x * pos.x + pos.y * pos.y);
                if (acc.x == 0.0)
                        log_warn("stdfunc_rot: zero distance to parent.");
                        
                /* Get user args. */
                lua_getglobal(L, "eapi");                       /* + eapi */
                L_get_strfield(L, -1, "idToObjectMap");         /* + idToObj */
                lua_pushinteger(L, data);
                lua_rawget(L, -2);                              /* + args */
                int arg_index = lua_gettop(L);
                
                /* Target velocity. */
                lua_pushinteger(L, 1);
                lua_rawget(L, arg_index);                       /* + vel */
                vel.y = lua_tonumber(L, -1);
                
                /* Initial angular velocity. */
                lua_pushinteger(L, 2);
                lua_rawget(L, arg_index);
                vel.x = lua_isnil(L, -1) ? 0.0 : lua_tonumber(L, -1);
                
                /* Multiplier. */
                lua_pushinteger(L, 3);                          /* + mult */
                lua_rawget(L, arg_index);
                acc.y = lua_isnil(L, -1) ? 1.0 : lua_tonumber(L, -1);
                info_assert(L, acc.y > 0.0 && acc.y <= 1.0, "Acceleration "
                            "multiplier out of range.");
                
                /* Store values and pop garbage. */
                SetAcc(body, acc);
                lua_pop(L, 6);
        }
        
        /* Figure out current angle. */
        float angle = acosf(pos.x / acc.x);
        if (pos.y < 0.0)
                angle = 2.0 * M_PI - angle;

        /* Update angular velocity toward desired. */
        vel.x += (vel.y - vel.x) * acc.y;
        SetVel(body, vel);

        /* Update body position. */
        float dt = GetWorld(body)->step_sec;
        angle += vel.x * dt;
        SetPos(body, (vect_f){acc.x * cosf(angle), acc.x * sinf(angle)});
}
