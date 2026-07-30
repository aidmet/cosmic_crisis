#include "cc_multi_menu.hpp"

#include "bn_keypad.h"
#include "bn_string.h"
#include "bn_bg_palettes.h"
#include "bn_regular_bg_items_starfield.h"
#include "common_variable_8x16_sprite_font.h"

namespace cc
{

namespace
{
constexpr int menu_count = 4;
constexpr const char* menu_labels[menu_count] = {
    "LINK CABLE",
    "WIRELESS",
    "ONLINE",
    "BACK",
};
}

multi_menu_scene::multi_menu_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void multi_menu_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);
    _index = 0;
    _rebuild_menu();
}

void multi_menu_scene::leave()
{
    _stars.reset();
    _text_sprites.clear();
}

void multi_menu_scene::_rebuild_menu()
{
    _text_sprites.clear();
    _text.set_center_alignment();
    _text.generate(0, -50, "MULTIPLAYER", _text_sprites);
    _text.generate(0, -34, "Cable / Wireless / Mobile", _text_sprites);

    for(int i = 0; i < menu_count; ++i)
    {
        bn::string<24> line;
        if(i == _index)
        {
            line = "> ";
            line += menu_labels[i];
            line += " <";
        }
        else
        {
            line = menu_labels[i];
        }

        _text.generate(0, -4 + i * 18, line, _text_sprites);
    }

    _text.generate(0, 72, "A: select  B: back", _text_sprites);
}

scene_id multi_menu_scene::update()
{
    if(_stars) _stars->update(0.35);

    if(bn::keypad::up_pressed())
    {
        _index = (_index + menu_count - 1) % menu_count;
        _rebuild_menu();
    }
    else if(bn::keypad::down_pressed())
    {
        _index = (_index + 1) % menu_count;
        _rebuild_menu();
    }

    if(bn::keypad::b_pressed() || (_index == 3 && bn::keypad::a_pressed()))
    {
        return scene_id::title;
    }

    if(bn::keypad::a_pressed())
    {
        if(_index == 0) current_game_mode() = game_mode::multi_cable;
        if(_index == 1) current_game_mode() = game_mode::multi_wireless;
        if(_index == 2) current_game_mode() = game_mode::multi_online;
        if(_index <= 2) return scene_id::link_wait;
    }

    return scene_id::multiplayer_menu;
}

} // namespace cc
