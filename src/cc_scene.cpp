#include "cc_scene.hpp"

#include "bn_keypad.h"

namespace cc
{

scene_id& current_scene_id()
{
    static scene_id id = scene_id::title;
    return id;
}

game_mode& current_game_mode()
{
    static game_mode mode = game_mode::endless;
    return mode;
}

int& campaign_chapter()
{
    static int chapter = 0;
    return chapter;
}

bool& request_emergency_restart()
{
    static bool flag = false;
    return flag;
}

bool emergency_chord_held()
{
    return bn::keypad::a_held() && bn::keypad::b_held() &&
           bn::keypad::start_held() && bn::keypad::select_held();
}

} // namespace cc
