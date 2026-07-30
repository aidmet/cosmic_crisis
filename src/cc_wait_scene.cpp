#include "cc_wait_scene.hpp"

#include "cc_link_net.hpp"
#include "cc_rng.hpp"

#include "bn_keypad.h"
#include "bn_string.h"
#include "bn_bg_palettes.h"
#include "bn_regular_bg_items_starfield.h"
#include "common_variable_8x16_sprite_font.h"

namespace cc
{

wait_scene::wait_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

void wait_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    _stars.emplace(bn::regular_bg_items::starfield);
    _timer = 0;
    _pulse = 0;

    int max_p = 4;
    if(current_game_mode() == game_mode::multi_wireless)
    {
        max_p = 5;
    }
    net().start(current_game_mode(), max_p);
    net().send_hello();
}

void wait_scene::leave()
{
    _stars.reset();
    _sprites.clear();
}

scene_id wait_scene::update()
{
    if(_stars) _stars->update(0.5);
    net().update();
    net().send_hello();

    ++_timer;
    ++_pulse;

    _sprites.clear();
    _text.set_center_alignment();

    const char* mode_name = "Link Cable";
    if(current_game_mode() == game_mode::multi_wireless) mode_name = "Wireless Adapter";
    if(current_game_mode() == game_mode::multi_online) mode_name = "Online Relay";

    _text.generate(0, -40, mode_name, _sprites);
    _text.generate(0, -20, "Waiting for pilots...", _sprites);

    bn::string<40> status;
    status = "Players: ";
    status += bn::to_string<8>(net().player_count());
    _text.generate(0, 0, status, _sprites);

    if((_pulse / 20) % 2 == 0)
    {
        _text.generate(0, 24, "B: cancel", _sprites);
    }

    if(current_game_mode() == game_mode::multi_online)
    {
        _text.generate(0, 44, "Scanning mobile adapter...", _sprites);
    }
    else if(current_game_mode() == game_mode::multi_wireless)
    {
        _text.generate(0, 44, "Broadcasting room COSMIC", _sprites);
    }

    if(bn::keypad::b_pressed())
    {
        net().stop();
        return scene_id::multiplayer_menu;
    }

    // Start when 2+ players connected for a short stability window,
    // or host presses START to launch with current lobby.
    bool ready = net().is_connected() && _timer > 90;
    if(ready || (net().is_connected() && bn::keypad::start_pressed()))
    {
        if(net().host())
        {
            unsigned seed = unsigned(_timer) * 2654435761u;
            net().send_seed(seed);
            seed_rng(seed);
        }
        else
        {
            seed_rng(net().shared_seed());
        }
        net().send_ready();
        return scene_id::game;
    }

    return scene_id::link_wait;
}

} // namespace cc
