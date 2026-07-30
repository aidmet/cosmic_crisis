#ifndef CC_LINK_NET_HPP
#define CC_LINK_NET_HPP

#include "cc_constants.hpp"

#include "bn_optional.h"
#include "bn_fixed.h"

namespace cc
{

// Packet layout in 16-bit link messages:
// bits 0-3: player id
// bits 4-7: msg type
// bits 8-15: payload
//
// Backends:
// - Link cable / Wireless Adapter: gba-link-connection LinkUniversal
// - Online (Mobile Adapter GB / REON): LinkMobile P2P transfers

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
    lives = 9,
    slow = 10,
    go = 11
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
    int facing = 1;
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
    [[nodiscard]] bool peers_ready() const;
    [[nodiscard]] int local_id() const;
    [[nodiscard]] int player_count() const;
    [[nodiscard]] unsigned shared_seed() const;

    // Short ASCII status for lobby UI (never uses non-font glyphs).
    [[nodiscard]] const char* transport_status() const;

    // Online (mobile adapter) controls once SESSION_ACTIVE.
    void online_wait_incoming();
    void online_dial_default();
    [[nodiscard]] bool online_can_dial() const;
    [[nodiscard]] bool online_waiting_incoming() const;

    void send_hello();
    void send_ready();
    void send_seed(unsigned seed);
    void send_state(bn::fixed x, bn::fixed y, int lives, int facing);
    void send_fire(bn::fixed y, int weapon, int facing = 1);
    void send_dead();
    void send_slow(int frames);
    void send_go();

    [[nodiscard]] const remote_player& remote(int id) const;
    remote_player& remote_mut(int id);
    [[nodiscard]] bool host() const;

    [[nodiscard]] bool using_wireless() const;
    [[nodiscard]] bool using_online() const;

    int consume_fire(int id);
    [[nodiscard]] bool poll_slow(int& frames);
    [[nodiscard]] bool poll_go();

private:
    enum class backend : int
    {
        none,
        universal,
        mobile
    };

    void _handle(int raw);
    void _mark_seen(int player);
    void _enqueue(int packet, bool urgent = false);
    void _flush_outgoing();
    void _pump_universal();
    void _pump_mobile();
    void _install_universal_irqs();
    void _install_mobile_irqs();
    void _clear_extra_irqs();

    game_mode _mode = game_mode::multi_cable;
    backend _backend = backend::none;
    bool _active = false;
    bool _connected = false;
    bool _host = true;
    bool _got_seed_lo = false;
    bool _got_seed_hi = false;
    bool _online_wait_incoming = true;
    bool _online_dial_started = false;
    bool _mobile_transfer_busy = false;
    int _local_id = 0;
    int _player_count = 1;
    int _max_players = 4;
    int _age = 0;
    unsigned _seed = 1;
    unsigned _seed_lo = 0;
    unsigned _seed_hi = 0;
    remote_player _remotes[max_players];
    int _send_phase = 0;

    static constexpr int out_cap = 16;
    int _out_q[out_cap];
    int _out_n = 0;

    bool _slow_ready = false;
    int _slow_frames = 0;
    bool _go_ready = false;
};

link_net& net();

} // namespace cc

#endif
