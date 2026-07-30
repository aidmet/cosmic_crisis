#ifndef CC_LINK_NET_HPP
#define CC_LINK_NET_HPP

#include "cc_constants.hpp"

#include "bn_optional.h"
#include "bn_fixed.h"

namespace cc
{

// Packet layout in 16-bit bn::link messages:
// bits 0-3: player id
// bits 4-7: msg type
// bits 8-15: payload
//
// IMPORTANT: bn::link only keeps the latest message per peer per frame.
// We therefore queue outbound packets and flush exactly ONE send per update().

enum class net_msg : int
{
    hello = 1,
    ready = 2,
    state_x = 3,
    state_y = 4,
    fire = 5,
    dead = 6,
    seed_lo = 7,
    seed_hi = 8,
    tick = 9,
    lives = 10,
    meteor_a = 11, // slot + size + frame
    meteor_b = 12, // y
    meteor_c = 13, // vx/vy quantized
    meteor_kill = 14,
    slow = 15
};

struct remote_player
{
    bool active = false;
    bool alive = true;
    bool ready = false;
    bn::fixed x = -70;
    bn::fixed y = 0;
    int lives = start_lives;
    int weapon = 0;
    int pending_fire = 0;
    int last_seen = 0;
};

struct meteor_spawn_event
{
    int slot = 0;
    int size = 0;
    int frame = 0;
    bn::fixed y = 0;
    bn::fixed vx = -1;
    bn::fixed vy = 0;
};

struct meteor_pose_event
{
    int slot = 0;
    bn::fixed x = 0;
    bn::fixed y = 0;
};

class link_net
{
public:
    void start(game_mode mode, int max_players_wanted = 4);
    void stop();
    void update();

    [[nodiscard]] bool is_connected() const;
    [[nodiscard]] bool seed_ready() const;
    [[nodiscard]] bool lobby_ready() const;
    [[nodiscard]] int local_id() const;
    [[nodiscard]] int player_count() const;
    [[nodiscard]] unsigned shared_seed() const;
    [[nodiscard]] int host_tick() const;

    void send_hello();
    void send_ready();
    void send_seed(unsigned seed);
    void send_state(bn::fixed x, bn::fixed y, int lives, bool braking);
    void send_fire(bn::fixed y, int weapon);
    void send_dead();
    void send_tick(int tick);
    void send_meteor_spawn(int slot, int size, bn::fixed y, bn::fixed vx, bn::fixed vy, int frame);
    void send_meteor_kill(int slot, bool explode = true);
    void send_meteor_pose(int slot, bn::fixed x, bn::fixed y);
    void send_slow(int frames);

    [[nodiscard]] const remote_player& remote(int id) const;
    remote_player& remote_mut(int id);
    [[nodiscard]] bool host() const;

    [[nodiscard]] bool using_wireless() const;
    [[nodiscard]] bool using_online() const;

    int consume_fire(int id);
    [[nodiscard]] bool poll_meteor_spawn(meteor_spawn_event& out);
    [[nodiscard]] bool poll_meteor_kill(int& slot, bool& explode);
    [[nodiscard]] bool poll_meteor_pose(meteor_pose_event& out);
    [[nodiscard]] bool poll_slow(int& frames);

private:
    void _pump_bn_link();
    void _handle(int raw);
    void _mark_seen(int player);
    void _finish_meteor_spawn();
    void _enqueue(int packet, bool urgent = false);
    void _flush_one();

    game_mode _mode = game_mode::multi_cable;
    bool _active = false;
    bool _connected = false;
    bool _host = true;
    bool _got_seed_lo = false;
    bool _got_seed_hi = false;
    int _local_id = 0;
    int _player_count = 1;
    int _max_players = 4;
    int _host_tick = 0;
    int _age = 0;
    unsigned _seed = 1;
    unsigned _seed_lo = 0;
    unsigned _seed_hi = 0;
    remote_player _remotes[max_players];
    int _timeout = 0;
    int _send_phase = 0;

    static constexpr int out_cap = 32;
    int _out_q[out_cap];
    int _out_n = 0;

    // Incoming meteor spawn assembly (across frames)
    int _m_a = -1;
    int _m_size = 0;
    int _m_frame = 0;
    int _m_y = 0;
    bool _m_have_a = false;
    bool _m_have_b = false;
    bool _m_have_c = false;
    int _m_vxq = 0;
    int _m_vyq = 0;
    int _m_age = 0;

    bool _spawn_ready = false;
    meteor_spawn_event _spawn_event;
    bool _kill_ready = false;
    int _kill_slot = 0;
    bool _kill_explode = true;
    bool _slow_ready = false;
    int _slow_frames = 0;

    int _pose_slot = 0;
    int _pose_x = 0;
    int _pose_y = 0;
    bool _pose_have_slot = false;
    bool _pose_have_x = false;
    bool _pose_have_y = false;
    bool _pose_ready = false;
    meteor_pose_event _pose_event;
};

link_net& net();

} // namespace cc

#endif
