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
    _lobby_seed = 0;
    _armed = false;
    _countdown = 0;

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

namespace
{

scene_id launch_match(unsigned seed)
{
    if(seed == 0) seed = 0xC051Cu;
    seed_rng(seed);
    seed_world_rng(seed);
    return scene_id::game;
}

}

scene_id wait_scene::update()
{
    if(_stars) _stars->update(0.5);
    net().update();
    net().send_hello();

    ++_timer;
    ++_pulse;

    // Host locks one seed and rebroadcasts it so clients converge.
    if(net().host() && net().is_connected())
    {
        if(_lobby_seed == 0)
        {
            _lobby_seed = unsigned(_timer) * 2654435761u;
            if(_lobby_seed == 0) _lobby_seed = 0xC051Cu;
        }
        if((_timer % 8) == 0)
        {
            net().send_seed(_lobby_seed);
        }
    }

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

    if(bn::keypad::b_pressed())
    {
        net().stop();
        return scene_id::multiplayer_menu;
    }

    // Client: host decides the exact launch frame via go packet.
    if(! net().host() && net().poll_go())
    {
        return launch_match(net().shared_seed());
    }

    // Arm when lobby is stable (or Start). Both sides must arm before launch.
    bool can_arm = net().lobby_ready() && _timer > 90;
    if(! _armed && can_arm && (_timer > 150 || bn::keypad::start_pressed()))
    {
        _armed = true;
    }

    if(_armed)
    {
        net().send_ready();
        if(net().host() && _lobby_seed != 0)
        {
            net().send_seed(_lobby_seed);
        }

        if(net().peers_ready())
        {
            if(net().host())
            {
                if(_countdown <= 0)
                {
                    _countdown = 60;
                }
            }
            else
            {
                _text.generate(0, 24, "Ready - wait for host", _sprites);
            }
        }
        else
        {
            _countdown = 0;
            _text.generate(0, 24, "Ready - waiting for peer", _sprites);
        }
    }
    else if((_pulse / 20) % 2 == 0)
    {
        _text.generate(0, 24, "B: cancel", _sprites);
    }

    if(net().host() && _countdown > 0)
    {
        --_countdown;
        bn::string<24> go = "Launch in ";
        go += bn::to_string<8>((_countdown / 20) + 1);
        _text.generate(0, 24, go, _sprites);

        if(_countdown == 0)
        {
            unsigned seed = _lobby_seed ? _lobby_seed : 0xC051Cu;
            net().send_seed(seed);
            net().send_go();
            // Flush go immediately — do not wait a frame for the queue.
            net().update();
            return launch_match(seed);
        }
    }

    if(current_game_mode() == game_mode::multi_online)
    {
        _text.generate(0, 44, "Scanning mobile adapter...", _sprites);
    }
    else if(current_game_mode() == game_mode::multi_wireless)
    {
        _text.generate(0, 44, "Broadcasting room COSMIC", _sprites);
    }

    return scene_id::link_wait;
}

} // namespace cc
