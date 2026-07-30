#include "cc_credits_scene.hpp"

#include "bn_keypad.h"
#include "bn_music.h"
#include "bn_music_items.h"
#include "bn_bg_palettes.h"
#include "bn_regular_bg_items_starfield.h"
#include "common_variable_8x16_sprite_font.h"
#include "cc_save_data.hpp"

namespace cc
{

credits_scene::credits_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void credits_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);
    _timer = 0;
    _sprites.clear();
    _text.set_center_alignment();
    _text.generate(0, -40, "COSMIC CRISIS", _sprites);
    _text.generate(0, -16, "The belt is still.", _sprites);
    _text.generate(0, 0, "The colony breathes.", _sprites);
    _text.generate(0, 24, "Thanks for playing.", _sprites);
    _text.generate(0, 56, "A: title", _sprites);

    if(! bn::music::playing() && save().music_volume)
    {
        bn::music_items::intro_music.play(0.4);
    }
}

void credits_scene::leave()
{
    _stars.reset();
    _sprites.clear();
}

scene_id credits_scene::update()
{
    if(_stars) _stars->update(0.15);
    ++_timer;
    if(bn::keypad::a_pressed() && _timer > 30)
    {
        return scene_id::title;
    }
    return scene_id::credits;
}

} // namespace cc
