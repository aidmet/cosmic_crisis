#include "cc_story_scene.hpp"

#include "cc_save_data.hpp"

#include "bn_keypad.h"
#include "bn_algorithm.h"
#include "bn_bg_palettes.h"
#include "bn_sprite_items_portrait_commander.h"
#include "bn_regular_bg_items_starfield.h"
#include "common_variable_8x16_sprite_font.h"

namespace cc
{

namespace
{
struct page
{
    const char* line1;
    const char* line2;
    const char* line3;
};

constexpr page chapters[][3] = {
    {
        {"COMMANDER:", "Pilot, the outer belt", "is tearing itself apart."},
        {"Meteors are flooding", "shipping lanes near", "Kepler Relay."},
        {"You are clear for", "sortie's first wave.", "Stay sharp."},
    },
    {
        {"COMMANDER:", "Wave two is faster.", "Something is guiding them."},
        {"Intel suggests a", "gravitic anomaly ahead.", "Weapons free."},
        {"Power cells detected", "in the debris field.", "Use them wisely."},
    },
    {
        {"COMMANDER:", "This is the storm's eye.", "No room for error."},
        {"If you fall, the", "colony loses the sky.", "We are with you."},
        {"Break through...", "or become another", "silent comet."},
    },
    {
        {"COMMANDER:", "Final approach.", "The anomaly is awake."},
        {"Every meteor now carries", "a piece of the dark.", "Unload everything."},
        {"For Cosmic Crisis.", "For home.", "Launch."},
    },
    {
        {"COMMANDER:", "You did the impossible.", "The belt is calming."},
        {"The colony sends", "its light to you.", "Stand down, hero."},
        {"End of transmission.", "See you on the deck.", "..."},
    },
};

constexpr int chapter_count = 5;
}

story_scene::story_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void story_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);
    _portrait = bn::sprite_items::portrait_commander.create_sprite(-80, -20);
    _chapter = bn::min(campaign_chapter(), chapter_count - 1);
    _page = 0;
    _show_page();
}

void story_scene::leave()
{
    _stars.reset();
    _sprites.clear();
    _portrait.reset();
}

void story_scene::_show_page()
{
    _sprites.clear();
    const page& p = chapters[_chapter][_page];
    _text.set_left_alignment();
    _text.generate(-40, -30, p.line1, _sprites);
    _text.generate(-40, -10, p.line2, _sprites);
    _text.generate(-40, 10, p.line3, _sprites);
    _text.set_center_alignment();
    _text.generate(0, 55, "A: next   B: skip to sortie", _sprites);
}

scene_id story_scene::update()
{
    if(_stars) _stars->update(0.2);

    if(bn::keypad::b_pressed())
    {
        return scene_id::game;
    }

    if(bn::keypad::a_pressed())
    {
        if(_page < 2)
        {
            ++_page;
            _show_page();
        }
        else
        {
            // Last chapter epilogue returns to title after victory flow from game.
            if(_chapter >= chapter_count - 1 && save().campaign_progress >= chapter_count - 1)
            {
                // Still allow replaying finale briefing then game.
            }
            return scene_id::game;
        }
    }

    return scene_id::story;
}

} // namespace cc
