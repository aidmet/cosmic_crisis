#ifndef CC_RNG_HPP
#define CC_RNG_HPP

#include "bn_random.h"

namespace cc
{

inline bn::random& rng()
{
    static bn::random r;
    return r;
}

inline void seed_rng(unsigned seed)
{
    rng() = bn::random();
    // Mix full seed bits so linked clients share the same stream.
    unsigned mix = seed ? seed : 1u;
    for(unsigned i = 0; i < 16; ++i)
    {
        if(mix & 1u)
        {
            (void)rng().get();
        }
        mix = (mix >> 1) ^ ((mix & 1u) ? 0xA3C59AC3u : 0u);
    }
    for(unsigned i = 0; i < (seed % 97u) + 11u; ++i)
    {
        (void)rng().get();
    }
}

} // namespace cc

#endif
