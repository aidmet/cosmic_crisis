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

    _text.generate(0, -48, mode_name, _sprites);
    _text.generate(0, -28, "Syncing pilots...", _sprites);

    bn::string<40> status;
    status = "Players: ";
    status += bn::to_string<8>(net().player_count());
    _text.generate(0, -8, status, _sprites);

    if(net().seed_ready())
    {
        _text.generate(0, 10, "Seed locked", _sprites);
    }
    else
    {
        _text.generate(0, 10, "Waiting seed...", _sprites);
    }

    if((_pulse / 20) % 2 == 0)
    {
        _text.generate(0, 28, "B: cancel", _sprites);
    }

    if(current_game_mode() == game_mode::multi_online)
    {
        _text.generate(0, 46, "Scanning mobile adapter...", _sprites);
    }
    else if(current_game_mode() == game_mode::multi_wireless)
    {
        _text.generate(0, 46, "Broadcasting room COSMIC", _sprites);
    }

    if(bn::keypad::b_pressed())
    {
        net().stop();
        return scene_id::multiplayer_menu;
    }

    // Harder sync gate: connected + shared seed + stable lobby window.
    bool ready = net().lobby_ready() && _timer > 150;
    bool force = net().lobby_ready() && bn::keypad::start_pressed() && _timer > 60;
    if(ready || force)
    {
        if(net().host())
        {
            if(_lobby_seed == 0)
            {
                _lobby_seed = unsigned(_timer) * 2654435761u;
                if(_lobby_seed == 0) _lobby_seed = 0xC051Cu;
            }
            net().send_seed(_lobby_seed);
            seed_rng(_lobby_seed);
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
