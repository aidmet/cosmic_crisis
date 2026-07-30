#include "cc_link_net.hpp"

#include "bn_core.h"
#include "bn_algorithm.h"

#include "LinkUniversal.hpp"
#include "LinkMobile.hpp"

extern "C"
{
#include "ugba/interrupts.h"
}

// Required globals for gba-link-connection ISR macros.
LinkUniversal* linkUniversal = nullptr;
LinkMobile* linkMobile = nullptr;

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
    return bn::clamp((x + 104).right_shift_integer(), 0, 255);
}

constexpr int encode_y(bn::fixed y)
{
    return bn::clamp((y + 80).right_shift_integer(), 0, 127);
}

constexpr bn::fixed decode_x(int payload)
{
    return bn::fixed(payload) - 104;
}

constexpr bn::fixed decode_y(int payload)
{
    return bn::fixed(payload) - 80;
}

// REON / libmobile loopback-style IPv4 phone number (127.0.0.1).
constexpr const char* default_online_peer = "127000000001";

LinkMobile::DataTransfer g_mobile_tx;
LinkMobile::DataTransfer g_mobile_rx;
}

link_net& net()
{
    static link_net instance;
    return instance;
}

void link_net::_install_universal_irqs()
{
    bn::core::set_vblank_callback(LINK_UNIVERSAL_ISR_VBLANK);
    IRQ_SetHandler(UGBA_IRQ_SERIAL, LINK_UNIVERSAL_ISR_SERIAL);
    IRQ_Enable(UGBA_IRQ_SERIAL);
    IRQ_SetHandler(UGBA_IRQ_TIMER3, LINK_UNIVERSAL_ISR_TIMER);
    IRQ_Enable(UGBA_IRQ_TIMER3);
}

void link_net::_install_mobile_irqs()
{
    bn::core::set_vblank_callback(LINK_MOBILE_ISR_VBLANK);
    IRQ_SetHandler(UGBA_IRQ_SERIAL, LINK_MOBILE_ISR_SERIAL);
    IRQ_Enable(UGBA_IRQ_SERIAL);
    IRQ_SetHandler(UGBA_IRQ_TIMER3, LINK_MOBILE_ISR_TIMER);
    IRQ_Enable(UGBA_IRQ_TIMER3);
}

void link_net::_clear_extra_irqs()
{
    bn::core::set_vblank_callback(nullptr);
    IRQ_Disable(UGBA_IRQ_SERIAL);
    IRQ_Disable(UGBA_IRQ_TIMER3);
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
    _online_wait_incoming = true;
    _online_dial_started = false;
    _mobile_transfer_busy = false;
    _local_id = 0;
    _player_count = 1;
    _age = 0;
    _seed = 0xC051Cu ^ (unsigned)mode;
    _seed_lo = 0;
    _seed_hi = 0;
    _send_phase = 0;
    _out_n = 0;
    _slow_ready = false;
    _go_ready = false;
    for(int i = 0; i < max_players; ++i)
    {
        _remotes[i] = remote_player();
    }

    if(mode == game_mode::multi_online)
    {
        _backend = backend::mobile;
        _max_players = 2;
        _install_mobile_irqs();
        linkMobile = new LinkMobile();
        linkMobile->activate();
    }
    else
    {
        _backend = backend::universal;
        const bool wireless = (mode == game_mode::multi_wireless);
        auto protocol = wireless ? LinkUniversal::Protocol::WIRELESS_AUTO
                                 : LinkUniversal::Protocol::CABLE;
        _install_universal_irqs();
        linkUniversal = new LinkUniversal(
            protocol,
            "COSMIC",
            LinkUniversal::CableOptions{
                LinkCable::BaudRate::BAUD_RATE_1,
                LINK_CABLE_DEFAULT_TIMEOUT,
                LINK_CABLE_DEFAULT_INTERVAL,
                LINK_CABLE_DEFAULT_SEND_TIMER_ID},
            LinkUniversal::WirelessOptions{
                true,
                true,
                (Link::u32)_max_players,
                LINK_WIRELESS_DEFAULT_TIMEOUT,
                LINK_WIRELESS_DEFAULT_INTERVAL,
                LINK_WIRELESS_DEFAULT_SEND_TIMER_ID});
        linkUniversal->activate();
    }

    _enqueue(pack(0, net_msg::hello, _max_players), true);
}

void link_net::stop()
{
    if(! _active && _backend == backend::none)
    {
        return;
    }

    if(linkUniversal)
    {
        linkUniversal->deactivate();
        delete linkUniversal;
        linkUniversal = nullptr;
    }
    if(linkMobile)
    {
        if(linkMobile->canShutdown())
        {
            linkMobile->shutdown();
        }
        linkMobile->deactivate();
        delete linkMobile;
        linkMobile = nullptr;
    }

    _clear_extra_irqs();
    _backend = backend::none;
    _active = false;
    _connected = false;
    _out_n = 0;
    _mobile_transfer_busy = false;
}

void link_net::_enqueue(int packet, bool urgent)
{
    if(_out_n >= out_cap)
    {
        if(! urgent) return;
        for(int i = 1; i < _out_n; ++i) _out_q[i - 1] = _out_q[i];
        --_out_n;
    }

    if(urgent)
    {
        for(int i = _out_n; i > 0; --i) _out_q[i] = _out_q[i - 1];
        _out_q[0] = packet;
        ++_out_n;
        return;
    }

    _out_q[_out_n++] = packet;
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
        if(player < _local_id) _host = false;
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
        _remotes[player].facing = (payload & 0x80) ? -1 : 1;
        _remotes[player].y = decode_y(payload & 0x7F);
        _remotes[player].alive = true;
        break;

    case net_msg::lives:
        _remotes[player].lives = bn::clamp(payload & 0x7, 0, max_lives);
        if(_remotes[player].lives <= 0) _remotes[player].alive = false;
        break;

    case net_msg::fire:
        _remotes[player].weapon = payload & 0x3;
        _remotes[player].facing = (payload & 0x4) ? -1 : 1;
        ++_remotes[player].pending_fire;
        break;

    case net_msg::dead:
        _remotes[player].alive = false;
        _remotes[player].lives = 0;
        break;

    case net_msg::slow:
        _slow_frames = payload > 0 ? payload : 180;
        _slow_ready = true;
        break;

    case net_msg::go:
        _go_ready = true;
        break;

    default:
        break;
    }
}

void link_net::_flush_outgoing()
{
    if(_backend == backend::universal)
    {
        if(! linkUniversal || ! linkUniversal->isConnected()) return;
        // Two packets per frame keeps ship x/y responsive.
        for(int n = 0; n < 2 && _out_n > 0; ++n)
        {
            if(! linkUniversal->canSend()) break;
            linkUniversal->send(Link::u16(_out_q[0]));
            for(int i = 1; i < _out_n; ++i) _out_q[i - 1] = _out_q[i];
            --_out_n;
        }
        return;
    }

    if(_backend == backend::mobile)
    {
        if(! linkMobile || ! linkMobile->isConnectedP2P()) return;
        if(_mobile_transfer_busy) return;
        if(_out_n <= 0) return;

        g_mobile_tx = LinkMobile::DataTransfer();
        int bytes = 0;
        while(_out_n > 0 && bytes + 2 <= LINK_MOBILE_MAX_USER_TRANSFER_LENGTH)
        {
            auto word = Link::u16(_out_q[0]);
            g_mobile_tx.data[bytes++] = Link::u8(word & 0xFF);
            g_mobile_tx.data[bytes++] = Link::u8((word >> 8) & 0xFF);
            for(int i = 1; i < _out_n; ++i) _out_q[i - 1] = _out_q[i];
            --_out_n;
        }
        g_mobile_tx.size = Link::u8(bytes);
        g_mobile_rx = LinkMobile::DataTransfer();
        if(linkMobile->transfer(g_mobile_tx, &g_mobile_rx))
        {
            _mobile_transfer_busy = true;
        }
    }
}

void link_net::_pump_universal()
{
    if(! linkUniversal) return;
    linkUniversal->sync();

    if(! linkUniversal->isConnected())
    {
        _connected = false;
        return;
    }

    _local_id = int(linkUniversal->currentPlayerId());
    _player_count = bn::max(int(linkUniversal->playerCount()), _player_count);
    _host = (_local_id == 0);
    _connected = _player_count >= 2;

    for(Link::u32 p = 0; p < linkUniversal->playerCount(); ++p)
    {
        if(int(p) == _local_id) continue;
        while(linkUniversal->canRead(p))
        {
            Link::u16 raw = linkUniversal->read(p);
            _handle(int(raw));
            _mark_seen(int(p));
        }
    }
}

void link_net::_pump_mobile()
{
    if(! linkMobile) return;

    if(linkMobile->getState() == LinkMobile::State::NEEDS_RESET)
    {
        _connected = false;
        return;
    }

    if(linkMobile->isSessionActive() && ! _online_wait_incoming && ! _online_dial_started)
    {
        // Client requested dial earlier but session wasn't ready yet.
    }

    if(linkMobile->isConnectedP2P())
    {
        _connected = true;
        _player_count = 2;
        // Receiver hosts the match seed; caller is player 1.
        if(linkMobile->getRole() == LinkMobile::Role::RECEIVER)
        {
            _local_id = 0;
            _host = true;
        }
        else
        {
            _local_id = 1;
            _host = false;
        }
        _remotes[1 - _local_id].active = true;
        _remotes[1 - _local_id].last_seen = _age;
        _remotes[1 - _local_id].alive = true;
    }
    else
    {
        _connected = false;
    }

        if(_mobile_transfer_busy && g_mobile_rx.completed)
    {
        _mobile_transfer_busy = false;
        if(g_mobile_rx.success)
        {
            for(Link::u8 i = 0; i + 1 < g_mobile_rx.size; i += 2)
            {
                int raw = int(g_mobile_rx.data[i]) | (int(g_mobile_rx.data[i + 1]) << 8);
                _handle(raw);
            }
        }
    }
}

void link_net::update()
{
    if(!_active) return;
    ++_age;

    if(_backend == backend::universal)
    {
        _pump_universal();
    }
    else if(_backend == backend::mobile)
    {
        _pump_mobile();
    }

    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(_remotes[i].active && (_age - _remotes[i].last_seen) > 180)
        {
            _remotes[i].active = false;
            _remotes[i].alive = false;
        }
    }

    _flush_outgoing();
}

bool link_net::is_connected() const { return _connected; }

bool link_net::seed_ready() const
{
    return _host || (_got_seed_lo && _got_seed_hi);
}

bool link_net::lobby_ready() const
{
    if(! _connected || ! seed_ready()) return false;
    if(_backend == backend::mobile) return true;
    int seen = 1;
    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(_remotes[i].active && (_age - _remotes[i].last_seen) < 60) ++seen;
    }
    return seen >= 2 && seen >= _player_count;
}

bool link_net::peers_ready() const
{
    if(! _connected || ! seed_ready()) return false;
    if(_backend == backend::mobile)
    {
        int other = 1 - _local_id;
        return _remotes[other].ready;
    }
    int need = 0;
    int got = 0;
    for(int i = 0; i < max_players; ++i)
    {
        if(i == _local_id) continue;
        if(! _remotes[i].active) continue;
        if((_age - _remotes[i].last_seen) >= 90) continue;
        ++need;
        if(_remotes[i].ready) ++got;
    }
    return need >= 1 && got >= need;
}

int link_net::local_id() const { return _local_id; }
int link_net::player_count() const { return _player_count; }
unsigned link_net::shared_seed() const { return _seed; }

const char* link_net::transport_status() const
{
    if(_backend == backend::universal && linkUniversal)
    {
        if(linkUniversal->isConnected())
        {
            return using_wireless() ? "Wireless linked" : "Cable linked";
        }
        if(using_wireless())
        {
            switch(linkUniversal->getWirelessState())
            {
            case LinkWireless::State::SEARCHING: return "Scanning COSMIC rooms";
            case LinkWireless::State::SERVING: return "Hosting COSMIC room";
            case LinkWireless::State::CONNECTING: return "Joining COSMIC room";
            case LinkWireless::State::CONNECTED: return "Wireless handshake";
            default: return "Starting wireless";
            }
        }
        return "Waiting for cable peers";
    }

    if(_backend == backend::mobile && linkMobile)
    {
        switch(linkMobile->getState())
        {
        case LinkMobile::State::NEEDS_RESET: return "No mobile adapter";
        case LinkMobile::State::PINGING:
        case LinkMobile::State::WAITING_TO_START:
        case LinkMobile::State::STARTING_SESSION:
        case LinkMobile::State::ACTIVATING_SIO32:
        case LinkMobile::State::WAITING_32BIT_SWITCH:
        case LinkMobile::State::READING_CONFIGURATION:
            return "Detecting mobile adapter";
        case LinkMobile::State::SESSION_ACTIVE:
            return _online_wait_incoming ? "Wait for call (START)" : "Ready to dial (A)";
        case LinkMobile::State::CALL_REQUESTED:
        case LinkMobile::State::CALLING:
            return "Calling peer...";
        case LinkMobile::State::CALL_ESTABLISHED:
            return "Online P2P linked";
        case LinkMobile::State::ISP_CALL_REQUESTED:
        case LinkMobile::State::ISP_CALLING:
        case LinkMobile::State::PPP_LOGIN:
            return "ISP login...";
        case LinkMobile::State::PPP_ACTIVE:
            return "PPP online";
        case LinkMobile::State::SHUTDOWN_REQUESTED:
        case LinkMobile::State::ENDING_SESSION:
        case LinkMobile::State::WAITING_8BIT_SWITCH:
        case LinkMobile::State::SHUTDOWN:
            return "Adapter shutting down";
        default:
            return "Mobile adapter busy";
        }
    }

    return "Link idle";
}

void link_net::online_wait_incoming()
{
    _online_wait_incoming = true;
    _online_dial_started = false;
}

void link_net::online_dial_default()
{
    if(! linkMobile) return;
    if(! linkMobile->isSessionActive()) return;
    if(_online_dial_started) return;
    _online_wait_incoming = false;
    _online_dial_started = true;
    linkMobile->call(default_online_peer);
}

bool link_net::online_can_dial() const
{
    return _backend == backend::mobile && linkMobile && linkMobile->isSessionActive() &&
           ! linkMobile->isConnectedP2P();
}

bool link_net::online_waiting_incoming() const
{
    return _online_wait_incoming;
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

void link_net::send_state(bn::fixed x, bn::fixed y, int lives, int facing)
{
    _enqueue(pack(_local_id, net_msg::state_x, encode_x(x)));
    int y_payload = encode_y(y) | ((facing < 0) ? 0x80 : 0);
    _enqueue(pack(_local_id, net_msg::state_y, y_payload));
    if((_send_phase & 7) == 7)
    {
        _enqueue(pack(_local_id, net_msg::lives, bn::clamp(lives, 0, max_lives)));
    }
    ++_send_phase;
}

void link_net::send_fire(bn::fixed, int weapon, int facing)
{
    int payload = (weapon & 0x3) | ((facing < 0) ? 0x4 : 0);
    _enqueue(pack(_local_id, net_msg::fire, payload), true);
}

void link_net::send_dead()
{
    _enqueue(pack(_local_id, net_msg::dead, 0), true);
}

void link_net::send_slow(int frames)
{
    _enqueue(pack(_local_id, net_msg::slow, bn::clamp(frames, 1, 255)), true);
}

void link_net::send_go()
{
    _go_ready = true;
    _enqueue(pack(_local_id, net_msg::go, 1), true);
}

const remote_player& link_net::remote(int id) const
{
    return _remotes[bn::clamp(id, 0, max_players - 1)];
}

remote_player& link_net::remote_mut(int id)
{
    return _remotes[bn::clamp(id, 0, max_players - 1)];
}

bool link_net::host() const { return _host; }
bool link_net::using_wireless() const { return _mode == game_mode::multi_wireless; }
bool link_net::using_online() const { return _mode == game_mode::multi_online; }

int link_net::consume_fire(int id)
{
    remote_player& r = remote_mut(id);
    int n = r.pending_fire;
    r.pending_fire = 0;
    return n;
}

bool link_net::poll_slow(int& frames)
{
    if(! _slow_ready) return false;
    frames = _slow_frames;
    _slow_ready = false;
    return true;
}

bool link_net::poll_go()
{
    if(! _go_ready) return false;
    _go_ready = false;
    return true;
}

} // namespace cc
