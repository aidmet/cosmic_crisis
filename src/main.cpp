#include "bn_core.h"
#include "bn_bg_palettes.h"

#include "cc_scene.hpp"
#include "cc_save_data.hpp"
#include "cc_title_scene.hpp"
#include "cc_options_scene.hpp"
#include "cc_multi_menu.hpp"
#include "cc_wait_scene.hpp"
#include "cc_story_scene.hpp"
#include "cc_game_scene.hpp"
#include "cc_credits_scene.hpp"
#include "cc_link_net.hpp"

int main()
{
    bn::core::init();
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    cc::load_save();

    cc::title_scene title;
    cc::options_scene options;
    cc::multi_menu_scene multi_menu;
    cc::wait_scene wait;
    cc::story_scene story;
    cc::game_scene game;
    cc::credits_scene credits;

    cc::scene* active = &title;
    cc::current_scene_id() = cc::scene_id::title;
    active->enter();

    while(true)
    {
        if(cc::emergency_chord_held())
        {
            cc::net().stop();
            active->leave();
            cc::current_scene_id() = cc::scene_id::title;
            active = &title;
            active->enter();
            // wait until release to avoid instant re-trigger
            while(cc::emergency_chord_held())
            {
                bn::core::update();
            }
        }

        cc::scene_id next = active->update();
        if(next != cc::current_scene_id())
        {
            active->leave();
            cc::current_scene_id() = next;
            switch(next)
            {
            case cc::scene_id::title: active = &title; break;
            case cc::scene_id::options: active = &options; break;
            case cc::scene_id::multiplayer_menu: active = &multi_menu; break;
            case cc::scene_id::link_wait: active = &wait; break;
            case cc::scene_id::story: active = &story; break;
            case cc::scene_id::game: active = &game; break;
            case cc::scene_id::credits: active = &credits; break;
            default: active = &title; cc::current_scene_id() = cc::scene_id::title; break;
            }
            active->enter();
        }

        bn::core::update();
    }
}
