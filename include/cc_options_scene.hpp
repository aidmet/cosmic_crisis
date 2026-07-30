#ifndef CC_OPTIONS_SCENE_HPP
#define CC_OPTIONS_SCENE_HPP

#include "cc_scene.hpp"
#include "cc_starfield.hpp"

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_optional.h"

namespace cc
{

class options_scene : public scene
{
public:
    options_scene();
    void enter() override;
    void leave() override;
    scene_id update() override;

private:
    void _rebuild_text();

    bn::optional<starfield> _stars;
    bn::sprite_text_generator _text;
    bn::vector<bn::sprite_ptr, 48> _sprites;
    int _index = 0;
};

} // namespace cc

#endif
