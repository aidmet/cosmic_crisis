#include "cc_link_net.hpp"

#include "bn_link.h"
#include "bn_link_state.h"
#include "bn_link_player.h"
#include "bn_algorithm.h"

namespace cc
{

namespace
{
constexpr int pack(int player, net_msg type, int payload)
{
    return (player & 0xF) | ((int(type) & 0xF) << 4) | ((payload & 0xFF) << 8);
}
}

link_net& net()
{
    static link_net instance;
    return instance;
}

void link_net::start(game_mode mode, int max_players_wanted)
{
    stop();
    _mode = mode;
    _max_players = bn::min(max_players_wanted, max_players);
    _active = true;
    _connected = false;
    _host = true;
    _local_id = 0;
    _player_count = 1;
    _seed = 0xC051Cu ^ (unsigned)mode;
    _timeout = 0;
    for(int i = 0; i < max_players; ++i)
    {
        _remotes[i] = remote_player();
    }
    // Kick the hardware by sending a hello.
    bn::link::send(pack(0, net_msg::hello, _max_players));
}

void link_net::stop()
{
    if(_active)
    {
        bn::link::deactivate();
    }
    _active = false;
    _connected = false;
}

void link_net::_handle(int raw)
{
    int player = raw & 0xF;
    auto type = net_msg((raw >> 4) & 0xF);
    int payload = (raw >> 8) & 0xFF;

    if(player < 0 || player >= max_players)
    {
        return;
    }

    switch(type)
    {
    case net_msg::hello:
        _remotes[player].active = true;
        _player_count = bn::max(_player_count, player + 1);
        if(player < _local_id)
        {
            _host = false;
        }
        _connected = _player_count >= 2;
        break;

    case net_msg::ready:
        _remotes[player].active = true;
        _connected = true;
        break;

    case net_msg::seed:
        if(!_host)
        {
            // payload only 8 bits — accept host-provided mix for lobby sync
            _seed = 0xA500u | (unsigned(payload) << 8);
        }
        break;

    case net_msg::state:
        {
            // payload encodes y in 0..159 approx
            _remotes[player].active = true;
            _remotes[player].alive = true;
            _remotes[player].y = bn::fixed(payload) - 80;
            _remotes[player].x = -70 + player * 12;
        }
        break;

    case net_msg::fire:
        // remote fire signal stored as weapon nibble in payload high
        _remotes[player].weapon = payload & 0x3;
        break;

    case net_msg::dead:
        _remotes[player].alive = false;
        _remotes[player].lives = 0;
        break;

    default:
        break;
    }
}

void link_net::_pump_bn_link()
{
    if(auto state = bn::link::receive())
    {
        _timeout = 0;
        _player_count = bn::max(_player_count, state->player_count());
        _local_id = state->current_player_id();
        _host = (_local_id == 0);
        _connected = state->player_count() >= 2;

        for(const bn::link_player& p : state->other_players())
        {
            _handle(int(p.data()));
            int id = p.id();
            if(id >= 0 && id < max_players)
            {
                _remotes[id].active = true;
            }
        }
    }
    else
    {
        ++_timeout;
    }
}

void link_net::update()
{
    if(!_active)
    {
        return;
    }
    _pump_bn_link();
}

bool link_net::is_connected() const
{
    return _connected;
}

int link_net::local_id() const
{
    return _local_id;
}

int link_net::player_count() const
{
    return _player_count;
}

unsigned link_net::shared_seed() const
{
    return _seed;
}

void link_net::send_hello()
{
    bn::link::send(pack(_local_id, net_msg::hello, _max_players));
}

void link_net::send_ready()
{
    bn::link::send(pack(_local_id, net_msg::ready, 1));
}

void link_net::send_seed(unsigned seed)
{
    _seed = seed;
    bn::link::send(pack(_local_id, net_msg::seed, int(seed & 0xFF)));
}

void link_net::send_state(bn::fixed, bn::fixed y, int, bool)
{
    int py = (y + 80).right_shift_integer();
    py = bn::clamp(py, 0, 255);
    bn::link::send(pack(_local_id, net_msg::state, py));
}

void link_net::send_fire(bn::fixed, int weapon)
{
    bn::link::send(pack(_local_id, net_msg::fire, weapon & 0xFF));
}

void link_net::send_dead()
{
    bn::link::send(pack(_local_id, net_msg::dead, 0));
}

const remote_player& link_net::remote(int id) const
{
    return _remotes[bn::clamp(id, 0, max_players - 1)];
}

bool link_net::host() const
{
    return _host;
}

bool link_net::using_wireless() const
{
    return _mode == game_mode::multi_wireless;
}

bool link_net::using_online() const
{
    return _mode == game_mode::multi_online;
}

} // namespace cc
