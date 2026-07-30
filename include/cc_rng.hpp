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
    for(unsigned i = 0; i < (seed % 97u) + 7u; ++i)
    {
        (void)rng().get();
    }
}

} // namespace cc

#endif
