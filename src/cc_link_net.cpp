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
    int v = (x + 104).right_shift_integer();
    return bn::clamp(v, 0, 255);
}

constexpr int encode_y(bn::fixed y)
{
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

constexpr int encode_mx(bn::fixed x)
{
    int v = (x + 140).right_shift_integer();
    return bn::clamp(v, 0, 255);
}

constexpr bn::fixed decode_mx(int payload)
{
    return bn::fixed(payload) - 140;
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
    _out_n = 0;
    _m_have_a = _m_have_b = _m_have_c = false;
    _spawn_ready = false;
    _kill_ready = false;
    _slow_ready = false;
    _pose_ready = false;
    _pose_have_slot = _pose_have_x = _pose_have_y = false;
    for(int i = 0; i < max_players; ++i)
    {
        _remotes[i] = remote_player();
    }
    _enqueue(pack(0, net_msg::hello, _max_players), true);
}

void link_net::stop()
{
    if(_active)
    {
        bn::link::deactivate();
    }
    _active = false;
    _connected = false;
    _out_n = 0;
}

void link_net::_enqueue(int packet, bool urgent)
{
    if(_out_n >= out_cap)
    {
        if(! urgent)
        {
            return;
        }
        // Drop oldest to make room for urgent traffic.
        for(int i = 1; i < _out_n; ++i)
        {
            _out_q[i - 1] = _out_q[i];
        }
        --_out_n;
    }

    if(urgent)
    {
        for(int i = _out_n; i > 0; --i)
        {
            _out_q[i] = _out_q[i - 1];
        }
        _out_q[0] = packet;
        ++_out_n;
        return;
    }

    _out_q[_out_n++] = packet;
}

void link_net::_flush_one()
{
    if(_out_n <= 0)
    {
        return;
    }
    bn::link::send(_out_q[0]);
    for(int i = 1; i < _out_n; ++i)
    {
        _out_q[i - 1] = _out_q[i];
    }
    --_out_n;
}

void link_net::_mark_seen(int player)
{
    if(player >= 0 && player < max_players)
    {
        _remotes[player].active = true;
        _remotes[player].last_seen = _age;
    }
}

void link_net::_finish_meteor_spawn()
{
    _spawn_event.slot = _m_a;
    _spawn_event.size = _m_size;
    _spawn_event.frame = _m_frame;
    _spawn_event.y = decode_y(_m_y);
    const int speed_tenths = bn::clamp(_m_vxq, 8, 60);
    _spawn_event.vx = -bn::fixed(speed_tenths) / 10;
    _spawn_event.vy = bn::fixed(int(_m_vyq) - 1) * bn::fixed(2) / 10;
    _spawn_ready = true;
    _m_have_a = _m_have_b = _m_have_c = false;
}

void link_net::_handle(int raw)
{
    int player = raw & 0xF;
    auto type = net_msg((raw >> 4) & 0xF);
    int payload = (raw >> 8) & 0xFF;

    if(player == 0xF)
    {
        if(type == net_msg::lives)
        {
            _pose_slot = payload & 0x1F;
            _pose_have_slot = true;
            _pose_have_x = false;
            _pose_have_y = false;
        }
        else if(type == net_msg::state_x)
        {
            _pose_x = payload;
            _pose_have_x = true;
        }
        else if(type == net_msg::state_y)
        {
            _pose_y = payload;
            _pose_have_y = true;
        }

        if(_pose_have_slot && _pose_have_x && _pose_have_y)
        {
            _pose_event.slot = _pose_slot;
            _pose_event.x = decode_mx(_pose_x);
            _pose_event.y = decode_y(_pose_y);
            _pose_ready = true;
            _pose_have_slot = _pose_have_x = _pose_have_y = false;
        }
        return;
    }

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

    case net_msg::meteor_a:
        _m_a = payload & 0x1F;
        _m_size = (payload >> 5) & 1;
        _m_frame = (payload >> 6) & 3;
        _m_have_a = true;
        _m_have_b = false;
        _m_have_c = false;
        _m_age = _age;
        break;

    case net_msg::meteor_b:
        _m_y = payload;
        _m_have_b = true;
        _m_age = _age;
        if(_m_have_a && _m_have_b && _m_have_c)
        {
            _finish_meteor_spawn();
        }
        break;

    case net_msg::meteor_c:
        _m_vxq = payload & 0x3F;
        _m_vyq = (payload >> 6) & 0x3;
        _m_have_c = true;
        _m_age = _age;
        if(_m_have_a && _m_have_b && _m_have_c)
        {
            _finish_meteor_spawn();
        }
        break;

    case net_msg::meteor_kill:
        _kill_slot = payload & 0x1F;
        _kill_explode = ((payload >> 5) & 1) != 0;
        _kill_ready = true;
        break;

    case net_msg::slow:
        _slow_frames = payload;
        if(_slow_frames <= 0) _slow_frames = 180;
        _slow_ready = true;
        break;

    default:
        break;
    }
}

void link_net::_pump_bn_link()
{
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

    // Abandon incomplete spawn assembly after ~1s.
    if((_m_have_a || _m_have_b || _m_have_c) && (_age - _m_age) > 60)
    {
        _m_have_a = _m_have_b = _m_have_c = false;
    }

    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(_remotes[i].active && (_age - _remotes[i].last_seen) > 120)
        {
            _remotes[i].active = false;
            _remotes[i].alive = false;
        }
    }

    // One packet per frame — required for reliable multi-packet messages.
    _flush_one();
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

    int seen = 1;
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
    _enqueue(pack(_local_id, net_msg::hello, _max_players));
}

void link_net::send_ready()
{
    _enqueue(pack(_local_id, net_msg::ready, 1), true);
}

void link_net::send_seed(unsigned seed)
{
    _seed = seed;
    _seed_lo = seed & 0xFFu;
    _seed_hi = (seed >> 8) & 0xFFu;
    _got_seed_lo = true;
    _got_seed_hi = true;
    _enqueue(pack(_local_id, net_msg::seed_lo, int(_seed_lo)), true);
    _enqueue(pack(_local_id, net_msg::seed_hi, int(_seed_hi)), true);
}

void link_net::send_state(bn::fixed x, bn::fixed y, int lives, bool)
{
    // Exactly one state packet per call so ships/meteors can share the wire.
    if((_send_phase % 3) == 0)
    {
        _enqueue(pack(_local_id, net_msg::state_x, encode_x(x)));
    }
    else if((_send_phase % 3) == 1)
    {
        _enqueue(pack(_local_id, net_msg::state_y, encode_y(y)));
    }
    else
    {
        _enqueue(pack(_local_id, net_msg::lives, bn::clamp(lives, 0, max_lives)));
    }
    ++_send_phase;
}

void link_net::send_fire(bn::fixed, int weapon)
{
    _enqueue(pack(_local_id, net_msg::fire, weapon & 0x3), true);
}

void link_net::send_dead()
{
    _enqueue(pack(_local_id, net_msg::dead, 0), true);
}

void link_net::send_tick(int tick)
{
    _host_tick = tick & 0xFF;
    _enqueue(pack(_local_id, net_msg::tick, _host_tick));
}

void link_net::send_meteor_spawn(int slot, int size, bn::fixed y, bn::fixed vx, bn::fixed vy, int frame)
{
    const int a = (slot & 0x1F) | ((size & 1) << 5) | ((frame & 3) << 6);
    const int b = encode_y(y);
    int speed = (-vx * 10).right_shift_integer();
    speed = bn::clamp(speed, 8, 60);
    int vy_code = 1;
    if(vy < bn::fixed(-0.1)) vy_code = 0;
    else if(vy > bn::fixed(0.3)) vy_code = 3;
    else if(vy > bn::fixed(0.1)) vy_code = 2;
    const int c = (speed & 0x3F) | ((vy_code & 3) << 6);

    // Urgent so spawn parts aren't stuck behind ship spam; still one/frame.
    _enqueue(pack(_local_id, net_msg::meteor_a, a), true);
    _enqueue(pack(_local_id, net_msg::meteor_b, b), true);
    _enqueue(pack(_local_id, net_msg::meteor_c, c), true);
}

void link_net::send_meteor_kill(int slot, bool explode)
{
    int payload = slot & 0x1F;
    if(explode) payload |= 0x20;
    _enqueue(pack(_local_id, net_msg::meteor_kill, payload), true);
}

void link_net::send_meteor_pose(int slot, bn::fixed x, bn::fixed y)
{
    _enqueue(pack(0xF, net_msg::lives, slot & 0x1F));
    _enqueue(pack(0xF, net_msg::state_x, encode_mx(x)));
    _enqueue(pack(0xF, net_msg::state_y, encode_y(y)));
}

void link_net::send_slow(int frames)
{
    _enqueue(pack(_local_id, net_msg::slow, bn::clamp(frames, 1, 255)), true);
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

bool link_net::poll_meteor_spawn(meteor_spawn_event& out)
{
    if(! _spawn_ready) return false;
    out = _spawn_event;
    _spawn_ready = false;
    return true;
}

bool link_net::poll_meteor_kill(int& slot, bool& explode)
{
    if(! _kill_ready) return false;
    slot = _kill_slot;
    explode = _kill_explode;
    _kill_ready = false;
    return true;
}

bool link_net::poll_meteor_pose(meteor_pose_event& out)
{
    if(! _pose_ready) return false;
    out = _pose_event;
    _pose_ready = false;
    return true;
}

bool link_net::poll_slow(int& frames)
{
    if(! _slow_ready) return false;
    frames = _slow_frames;
    _slow_ready = false;
    return true;
}

} // namespace cc
