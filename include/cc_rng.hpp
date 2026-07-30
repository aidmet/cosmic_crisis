#ifndef CC_RNG_HPP
#define CC_RNG_HPP

#include "bn_random.h"

namespace cc
{

namespace
{

inline void _mix_seed_into(bn::random& r, unsigned seed)
{
    // Mix full seed bits so linked clients share the same stream.
    unsigned mix = seed ? seed : 1u;
    for(unsigned i = 0; i < 16; ++i)
    {
        if(mix & 1u)
        {
            (void)r.get();
        }
        mix = (mix >> 1) ^ ((mix & 1u) ? 0xA3C59AC3u : 0u);
    }
    for(unsigned i = 0; i < (seed % 97u) + 11u; ++i)
    {
        (void)r.get();
    }
}

}

inline bn::random& rng()
{
    static bn::random r;
    return r;
}

// Isolated stream for lockstep meteor spawns in multiplayer.
inline bn::random& world_rng()
{
    static bn::random r;
    return r;
}

inline void seed_rng(unsigned seed)
{
    rng() = bn::random();
    _mix_seed_into(rng(), seed);
}

inline void seed_world_rng(unsigned seed)
{
    world_rng() = bn::random();
    _mix_seed_into(world_rng(), seed ^ 0x4D375A86u);
}

} // namespace cc

#endif
