#ifndef CC_MULTI_MENU_HPP
#define CC_MULTI_MENU_HPP

#include "cc_scene.hpp"
#include "cc_starfield.hpp"

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_optional.h"

namespace cc
{

class multi_menu_scene : public scene
{
public:
    multi_menu_scene();
    void enter() override;
    void leave() override;
    scene_id update() override;

private:
    void _rebuild_menu();

    bn::optional<starfield> _stars;
    bn::sprite_text_generator _text;
    bn::vector<bn::sprite_ptr, 48> _text_sprites;
    int _index = 0;
};

} // namespace cc

#endif
