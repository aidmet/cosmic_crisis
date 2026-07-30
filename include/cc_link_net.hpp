#ifndef CC_LINK_NET_HPP
#define CC_LINK_NET_HPP

#include "cc_constants.hpp"

#include "bn_optional.h"
#include "bn_fixed.h"

namespace cc
{

// Packet layout in 16-bit bn::link / LinkUniversal messages:
// bits 0-3: player id
// bits 4-7: msg type
// bits 8-15: payload

enum class net_msg : int
{
    hello = 1,
    ready = 2,
    state = 3,
    fire = 4,
    dead = 5,
    seed = 6,
    ping = 7
};

struct remote_player
{
    bool active = false;
    bool alive = true;
    bn::fixed x = 0;
    bn::fixed y = 0;
    int lives = start_lives;
    int weapon = 0;
};

class link_net
{
public:
    void start(game_mode mode, int max_players_wanted = 4);
    void stop();
    void update();

    [[nodiscard]] bool is_connected() const;
    [[nodiscard]] int local_id() const;
    [[nodiscard]] int player_count() const;
    [[nodiscard]] unsigned shared_seed() const;

    void send_hello();
    void send_ready();
    void send_seed(unsigned seed);
    void send_state(bn::fixed x, bn::fixed y, int lives, bool braking);
    void send_fire(bn::fixed y, int weapon);
    void send_dead();

    [[nodiscard]] const remote_player& remote(int id) const;
    [[nodiscard]] bool host() const;

    [[nodiscard]] bool using_wireless() const;
    [[nodiscard]] bool using_online() const;

private:
    void _pump_bn_link();
    void _handle(int raw);

    game_mode _mode = game_mode::multi_cable;
    bool _active = false;
    bool _connected = false;
    bool _host = true;
    int _local_id = 0;
    int _player_count = 1;
    int _max_players = 4;
    unsigned _seed = 1;
    remote_player _remotes[max_players];
    int _timeout = 0;
};

link_net& net();

} // namespace cc

#endif
