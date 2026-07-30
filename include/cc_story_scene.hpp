#ifndef CC_STORY_SCENE_HPP
#define CC_STORY_SCENE_HPP

#include "cc_scene.hpp"
#include "cc_starfield.hpp"

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_text_generator.h"
#include "bn_optional.h"

namespace cc
{

class story_scene : public scene
{
public:
    story_scene();
    void enter() override;
    void leave() override;
    scene_id update() override;

private:
    void _show_page();

    bn::optional<starfield> _stars;
    bn::optional<bn::sprite_ptr> _portrait;
    bn::sprite_text_generator _text;
    bn::vector<bn::sprite_ptr, 64> _sprites;
    int _page = 0;
    int _chapter = 0;
};

} // namespace cc

#endif
