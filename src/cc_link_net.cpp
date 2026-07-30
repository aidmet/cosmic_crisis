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

constexpr int encode_x(bn::fixed x)
{
    // playfield x roughly [-104, 40] → [0, 255]
    int v = (x + 104).right_shift_integer();
    return bn::clamp(v, 0, 255);
}

constexpr int encode_y(bn::fixed y)
{
    // playfield y roughly [-64, 64] → [0, 255]
    int v = (y + 80).right_shift_integer();
    return bn::clamp(v, 0, 255);
}

constexpr bn::fixed decode_x(int payload)
{
    return bn::fixed(payload) - 104;
}

constexpr bn::fixed decode_y(int payload)
{
    return bn::fixed(payload) - 80;
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
    _got_seed_lo = false;
    _got_seed_hi = false;
    _local_id = 0;
    _player_count = 1;
    _host_tick = 0;
    _age = 0;
    _seed = 0xC051Cu ^ (unsigned)mode;
    _seed_lo = 0;
    _seed_hi = 0;
    _timeout = 0;
    _send_phase = 0;
    for(int i = 0; i < max_players; ++i)
    {
        _remotes[i] = remote_player();
    }
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

void link_net::_mark_seen(int player)
{
    if(player >= 0 && player < max_players)
    {
        _remotes[player].active = true;
        _remotes[player].last_seen = _age;
    }
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

    _mark_seen(player);

    switch(type)
    {
    case net_msg::hello:
        _player_count = bn::max(_player_count, player + 1);
        if(player < _local_id)
        {
            _host = false;
        }
        _connected = _player_count >= 2;
        break;

    case net_msg::ready:
        _remotes[player].ready = true;
        _connected = true;
        break;

    case net_msg::seed_lo:
        _seed_lo = unsigned(payload);
        _got_seed_lo = true;
        if(_got_seed_lo && _got_seed_hi && ! _host)
        {
            _seed = _seed_lo | (_seed_hi << 8);
        }
        break;

    case net_msg::seed_hi:
        _seed_hi = unsigned(payload);
        _got_seed_hi = true;
        if(_got_seed_lo && _got_seed_hi && ! _host)
        {
            _seed = _seed_lo | (_seed_hi << 8);
        }
        break;

    case net_msg::state_x:
        _remotes[player].x = decode_x(payload);
        _remotes[player].alive = true;
        break;

    case net_msg::state_y:
        _remotes[player].y = decode_y(payload);
        _remotes[player].alive = true;
        break;

    case net_msg::lives:
        _remotes[player].lives = bn::clamp(payload & 0x7, 0, max_lives);
        if(_remotes[player].lives <= 0)
        {
            _remotes[player].alive = false;
        }
        break;

    case net_msg::fire:
        _remotes[player].weapon = payload & 0x3;
        ++_remotes[player].pending_fire;
        break;

    case net_msg::dead:
        _remotes[player].alive = false;
        _remotes[player].lives = 0;
        break;

    case net_msg::tick:
        if(! _host)
        {
            _host_tick = payload;
        }
        break;

    default:
        break;
    }
}

void link_net::_pump_bn_link()
{
    // Drain multiple receive slots per frame for tighter sync.
    for(int attempt = 0; attempt < 4; ++attempt)
    {
        auto state = bn::link::receive();
        if(! state)
        {
            if(attempt == 0)
            {
                ++_timeout;
            }
            break;
        }

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
                _remotes[id].last_seen = _age;
            }
        }
    }
}

void link_net::update()
{
    if(!_active)
    {
        return;
    }
    ++_age;
    _pump_bn_link();

    // Drop remotes that went silent for ~2 seconds.
    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(_remotes[i].active && (_age - _remotes[i].last_seen) > 120)
        {
            _remotes[i].active = false;
            _remotes[i].alive = false;
        }
    }
}

bool link_net::is_connected() const
{
    return _connected;
}

bool link_net::seed_ready() const
{
    if(_host)
    {
        return true;
    }
    return _got_seed_lo && _got_seed_hi;
}

bool link_net::lobby_ready() const
{
    if(! _connected || ! seed_ready())
    {
        return false;
    }

    // Need every other active slot to have said hello recently.
    int seen = 1; // local
    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(_remotes[i].active && (_age - _remotes[i].last_seen) < 60)
        {
            ++seen;
        }
    }
    return seen >= 2 && seen >= _player_count;
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

int link_net::host_tick() const
{
    return _host_tick;
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
    _seed_lo = seed & 0xFFu;
    _seed_hi = (seed >> 8) & 0xFFu;
    _got_seed_lo = true;
    _got_seed_hi = true;
    bn::link::send(pack(_local_id, net_msg::seed_lo, int(_seed_lo)));
    bn::link::send(pack(_local_id, net_msg::seed_hi, int(_seed_hi)));
}

void link_net::send_state(bn::fixed x, bn::fixed y, int lives, bool)
{
    // Alternate emphasis but always push both axes for harder position sync.
    if((_send_phase & 1) == 0)
    {
        bn::link::send(pack(_local_id, net_msg::state_x, encode_x(x)));
        bn::link::send(pack(_local_id, net_msg::state_y, encode_y(y)));
    }
    else
    {
        bn::link::send(pack(_local_id, net_msg::state_y, encode_y(y)));
        bn::link::send(pack(_local_id, net_msg::state_x, encode_x(x)));
        bn::link::send(pack(_local_id, net_msg::lives, bn::clamp(lives, 0, max_lives)));
    }
    ++_send_phase;
}

void link_net::send_fire(bn::fixed, int weapon)
{
    bn::link::send(pack(_local_id, net_msg::fire, weapon & 0x3));
}

void link_net::send_dead()
{
    bn::link::send(pack(_local_id, net_msg::dead, 0));
    bn::link::send(pack(_local_id, net_msg::lives, 0));
}

void link_net::send_tick(int tick)
{
    _host_tick = tick & 0xFF;
    bn::link::send(pack(_local_id, net_msg::tick, _host_tick));
}

const remote_player& link_net::remote(int id) const
{
    return _remotes[bn::clamp(id, 0, max_players - 1)];
}

remote_player& link_net::remote_mut(int id)
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

int link_net::consume_fire(int id)
{
    remote_player& r = remote_mut(id);
    int n = r.pending_fire;
    r.pending_fire = 0;
    return n;
}

} // namespace cc
