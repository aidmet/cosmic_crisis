#ifndef CC_STARFIELD_HPP
#define CC_STARFIELD_HPP

#include "bn_fixed.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_item.h"

namespace cc
{

class starfield
{
public:
    explicit starfield(const bn::regular_bg_item& item) :
        _bg(item.create_bg(0, 0))
    {
        _bg.set_priority(3);
    }

    void update(bn::fixed scroll_speed = 0.35)
    {
        _x += scroll_speed;
        _bg.set_x(_x);
        _bg.set_y(_y);
        _y += scroll_speed * 0.15;
    }

    bn::regular_bg_ptr& bg()
    {
        return _bg;
    }

private:
    bn::regular_bg_ptr _bg;
    bn::fixed _x = 0;
    bn::fixed _y = 0;
};

} // namespace cc

#endif
