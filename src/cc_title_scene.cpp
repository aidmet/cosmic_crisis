#include "cc_title_scene.hpp"

#include "cc_save_data.hpp"

#include "bn_keypad.h"
#include "bn_music.h"
#include "bn_music_items.h"
#include "bn_string.h"
#include "bn_algorithm.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_items_title_logo.h"
#include "bn_regular_bg_items_starfield.h"

#include "common_variable_8x16_sprite_font.h"

namespace cc
{

namespace
{
constexpr int menu_count = 4;
constexpr const char* menu_labels[menu_count] = {
    "PLAY",
    "STORY",
    "MULTIPLAYER",
    "OPTIONS",
};
}

title_scene::title_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void title_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);

    // One title panel, clear of the menu
    _title = bn::sprite_items::title_logo.create_sprite(0, -48);

    if(! bn::music::playing())
    {
        bn::music_items::intro_music.play(save().music_volume ? bn::fixed(0.5) : bn::fixed(0));
    }

    _index = 0;
    _rebuild_menu();
}

void title_scene::leave()
{
    _stars.reset();
    _title.reset();
    _text_sprites.clear();
}

void title_scene::_rebuild_menu()
{
    _text_sprites.clear();
    _text.set_center_alignment();

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

    _text.generate(0, 72, "A: select", _text_sprites);
}

scene_id title_scene::update()
{
    if(_stars)
    {
        _stars->update(0.35);
    }

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

    if(bn::keypad::a_pressed())
    {
        switch(_index)
        {
        case 0:
            current_game_mode() = game_mode::endless;
            return scene_id::game;
        case 1:
            current_game_mode() = game_mode::campaign;
            campaign_chapter() = bn::min(save().campaign_progress, 4);
            return scene_id::story;
        case 2:
            return scene_id::multiplayer_menu;
        case 3:
            return scene_id::options;
        default:
            break;
        }
    }

    return scene_id::title;
}

} // namespace cc
