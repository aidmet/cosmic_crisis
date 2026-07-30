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
    lives = 10
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

    [[nodiscard]] const remote_player& remote(int id) const;
    remote_player& remote_mut(int id);
    [[nodiscard]] bool host() const;

    [[nodiscard]] bool using_wireless() const;
    [[nodiscard]] bool using_online() const;

    int consume_fire(int id);

private:
    void _pump_bn_link();
    void _handle(int raw);
    void _mark_seen(int player);

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
};

link_net& net();

} // namespace cc

#endif
