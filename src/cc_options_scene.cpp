#include "cc_options_scene.hpp"

#include "cc_save_data.hpp"

#include "bn_keypad.h"
#include "bn_music.h"
#include "bn_string.h"
#include "bn_bg_palettes.h"
#include "bn_regular_bg_items_starfield.h"
#include "common_variable_8x16_sprite_font.h"

namespace cc
{

options_scene::options_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void options_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);
    _index = 0;
    _rebuild_text();
}

void options_scene::leave()
{
    write_save();
    _stars.reset();
    _sprites.clear();
}

void options_scene::_rebuild_text()
{
    _sprites.clear();
    _text.set_center_alignment();
    _text.generate(0, -50, "OPTIONS", _sprites);
    _text.set_left_alignment();

    bn::string<32> music_line = _index == 0 ? "> Music: " : "  Music: ";
    music_line += save().music_volume ? "ON" : "OFF";
    _text.generate(-70, -10, music_line, _sprites);

    bn::string<32> sfx_line = _index == 1 ? "> SFX: " : "  SFX: ";
    sfx_line += save().sfx_volume ? "ON" : "OFF";
    _text.generate(-70, 10, sfx_line, _sprites);

    bn::string<40> prog = _index == 2 ? "> Reset story progress" : "  Reset story progress";
    _text.generate(-70, 30, prog, _sprites);

    _text.set_center_alignment();
    _text.generate(0, 60, "LEFT/RIGHT change  B back", _sprites);
}

scene_id options_scene::update()
{
    if(_stars)
    {
        _stars->update(0.25);
    }

    if(bn::keypad::up_pressed())
    {
        _index = (_index + 2) % 3;
        _rebuild_text();
    }
    else if(bn::keypad::down_pressed())
    {
        _index = (_index + 1) % 3;
        _rebuild_text();
    }
    else if(bn::keypad::left_pressed() || bn::keypad::right_pressed() || bn::keypad::a_pressed())
    {
        if(_index == 0)
        {
            save().music_volume = save().music_volume ? 0 : 1;
            if(save().music_volume)
            {
                if(bn::music::playing())
                {
                    bn::music::set_volume(0.5);
                }
            }
            else if(bn::music::playing())
            {
                bn::music::set_volume(0);
            }
        }
        else if(_index == 1)
        {
            save().sfx_volume = save().sfx_volume ? 0 : 1;
        }
        else if(_index == 2 && bn::keypad::a_pressed())
        {
            save().campaign_progress = 0;
        }
        _rebuild_text();
    }

    if(bn::keypad::b_pressed())
    {
        return scene_id::title;
    }

    return scene_id::options;
}

} // namespace cc
