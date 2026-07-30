#include "cc_game_scene.hpp"

#include "cc_link_net.hpp"
#include "cc_rng.hpp"
#include "cc_save_data.hpp"

#include "bn_keypad.h"
#include "bn_music.h"
#include "bn_string.h"
#include "bn_algorithm.h"
#include "bn_bg_palettes.h"
#include "bn_regular_bg_items_starfield.h"
#include "bn_sprite_items_ships.h"
#include "bn_sprite_items_meteor16.h"
#include "bn_sprite_items_meteor32.h"
#include "bn_sprite_items_bullets.h"
#include "bn_sprite_items_powerups.h"
#include "bn_sprite_items_heart.h"
#include "bn_sprite_items_explosion.h"
#include "bn_sprite_items_shield_fx.h"
#include "bn_sprite_items_weapon_icons.h"
#include "common_variable_8x16_sprite_font.h"

namespace cc
{

game_scene::game_scene() :
    _text(common::variable_8x16_sprite_font)
{
}

bool game_scene::_is_multi() const
{
    return current_game_mode() == game_mode::multi_cable ||
           current_game_mode() == game_mode::multi_wireless ||
           current_game_mode() == game_mode::multi_online;
}

bool game_scene::_is_campaign() const
{
    return current_game_mode() == game_mode::campaign;
}

int game_scene::_meteors_for_level() const
{
    return meteors_per_level_base + (_level - 1) * meteors_per_level_step;
}

void game_scene::enter()
{
    bn::bg_palettes::set_transparent_color(bn::color(0, 0, 8));
    if(bn::music::playing())
    {
        bn::music::stop();
    }

    _stars.emplace(bn::regular_bg_items::starfield);
    _x = -70;
    _y = 0;
    _vx = 0;
    _vy = 0;
    _lives = start_lives;
    _level = 1 + (_is_campaign() ? campaign_chapter() : 0);
    _score = 0;
    _passed = 0;
    _spawn_timer = 0;
    _fire_cooldown = 0;
    _i_frames = 0;
    _slow_timer = 0;
    _paused = false;
    _dead = false;
    _won = false;
    _has_shield = false;
    _held = powerup_type::none;
    _weapon = weapon_type(bn::min(save().unlocked_weapon, 2));
    _game_over_timer = 0;
    _toast_timer = 0;
    _toast_text = nullptr;
    _chapter_goal = 30 + campaign_chapter() * 18;

    int local = _is_multi() ? net().local_id() : 0;
    _ship = bn::sprite_items::ships.create_sprite(_x, _y, local);

    for(int i = 0; i < max_players; ++i)
    {
        _remote_ships[i].reset();
        if(_is_multi() && i != local)
        {
            _remote_ships[i] = bn::sprite_items::ships.create_sprite(-70 + i * 12, bn::fixed(i * 20 - 40), i);
            _remote_ships[i]->set_visible(false);
        }
    }

    for(auto& m : _meteors) { m = meteor(); }
    for(auto& b : _bullets) { b = bullet(); }
    for(auto& p : _pickups) { p = pickup(); }
    for(auto& e : _blasts) { e = blast(); }

    _shield_fx.reset();
    _rebuild_hud();
}

void game_scene::leave()
{
    _stars.reset();
    _hud.clear();
    _hearts.clear();
    _shield_fx.reset();
    for(auto& m : _meteors) m.sprite.reset();
    for(auto& b : _bullets) b.sprite.reset();
    for(auto& p : _pickups) { p.sprite.reset(); p.label.clear(); p.active = false; }
    for(auto& e : _blasts) { e.sprite.reset(); e.anim.reset(); }
    for(auto& r : _remote_ships) r.reset();
    _ship.reset();
}

void game_scene::_rebuild_hud()
{
    _hud.clear();
    _hearts.clear();
    for(int i = 0; i < _lives; ++i)
    {
        _hearts.push_back(bn::sprite_items::heart.create_sprite(-108 + i * 10, -72));
    }

    _text.set_left_alignment();
    bn::string<32> line = "LV";
    line += bn::to_string<8>(_level);
    line += "  ";
    line += bn::to_string<8>(_score);
    _text.generate(-40, -72, line, _hud);

    if(_held != powerup_type::none)
    {
        _hud.push_back(bn::sprite_items::powerups.create_sprite(96, -72, int(_held)));
        _text.set_center_alignment();
        _text.generate(96, -56, _powerup_name(_held), _hud);
    }
    _hud.push_back(bn::sprite_items::weapon_icons.create_sprite(112, -72, int(_weapon)));

    if(_toast_timer > 0 && _toast_text)
    {
        _text.set_center_alignment();
        _text.generate(0, 48, _toast_text, _hud);
    }

    if(_paused)
    {
        _text.set_center_alignment();
        _text.generate(0, 0, "PAUSED", _hud);
        _text.generate(0, 16, "START: resume", _hud);
    }
}

const char* game_scene::_powerup_name(powerup_type type)
{
    switch(type)
    {
    case powerup_type::shield: return "SHIELD";
    case powerup_type::slow: return "SLOW";
    case powerup_type::clear: return "CLEAR";
    case powerup_type::weapon: return "WEAPON";
    case powerup_type::life: return "LIFE";
    default: return "";
    }
}

void game_scene::_set_pickup_label(pickup& p)
{
    p.label.clear();
    _text.set_center_alignment();
    _text.generate(p.x, p.y + 12, _powerup_name(p.type), p.label);
}

void game_scene::_spawn_meteor()
{
    for(int i = 0; i < max_meteors; ++i)
    {
        if(_meteors[i].active)
        {
            continue;
        }

        const int size = (rng().get_unbiased_int(5) == 0) ? 1 : 0;
        const bn::fixed y = bn::fixed(rng().get_unbiased_int(120)) - 60;
        bn::fixed speed = bn::fixed(1.2) + bn::fixed(_level) * bn::fixed(0.18);
        if(_is_campaign())
        {
            speed += bn::fixed(campaign_chapter()) * bn::fixed(0.15);
        }
        const bn::fixed vx = -speed - bn::fixed(rng().get_unbiased_int(8)) / 10;
        const bn::fixed vy = bn::fixed(rng().get_unbiased_int(7) - 3) / 10;
        const int frame = rng().get_unbiased_int(4);

        _spawn_meteor_slot(i, size, y, vx, vy, frame);

        if(_is_multi() && net().host())
        {
            net().send_meteor_spawn(i, size, y, vx, vy, frame);
        }
        return;
    }
}

void game_scene::_spawn_meteor_slot(int slot, int size, bn::fixed y, bn::fixed vx, bn::fixed vy, int frame)
{
    if(slot < 0 || slot >= max_meteors) return;

    meteor& m = _meteors[slot];
    m.sprite.reset();
    m.active = true;
    m.size = size ? 1 : 0;
    m.hp = m.size ? 3 : 1;
    m.x = 140;
    m.y = y;
    m.vx = vx;
    m.vy = vy;
    m.frame = frame & 3;
    m.anim = 0;
    if(m.size)
    {
        m.sprite = bn::sprite_items::meteor32.create_sprite(m.x, m.y, m.frame);
    }
    else
    {
        m.sprite = bn::sprite_items::meteor16.create_sprite(m.x, m.y, m.frame);
    }
}

void game_scene::_kill_meteor_slot(int slot, bool from_net, bool explode, bool allow_drop, bool count_progress)
{
    if(slot < 0 || slot >= max_meteors) return;
    meteor& m = _meteors[slot];
    if(! m.active) return;

    const bn::fixed mx = m.x;
    const bn::fixed my = m.y;
    const int msize = m.size;

    m.active = false;
    m.sprite.reset();

    if(explode)
    {
        for(auto& e : _blasts)
        {
            if(e.active) continue;
            e.active = true;
            e.age = 0;
            e.sprite = bn::sprite_items::explosion.create_sprite(mx, my, 0);
            e.anim = bn::create_sprite_animate_action_once(
                *e.sprite, 3, bn::sprite_items::explosion.tiles_item(), 0, 1, 2, 3);
            break;
        }
    }

    if(allow_drop && (!_is_multi() || net().host()) && rng().get_unbiased_int(100) < 18)
    {
        for(auto& p : _pickups)
        {
            if(p.active) continue;
            p.active = true;
            p.x = mx;
            p.y = my;
            p.type = powerup_type(rng().get_unbiased_int(5));
            p.sprite = bn::sprite_items::powerups.create_sprite(p.x, p.y, int(p.type));
            _set_pickup_label(p);
            break;
        }
    }

    if(count_progress)
    {
        if(explode)
        {
            _score += 25 + msize * 25;
        }
        else
        {
            _score += 10;
        }
        ++_passed;
    }

    if(_is_multi() && ! from_net)
    {
        net().send_meteor_kill(slot, explode);
    }
}

void game_scene::_apply_net_world()
{
    if(! _is_multi()) return;

    meteor_spawn_event spawn;
    while(net().poll_meteor_spawn(spawn))
    {
        _spawn_meteor_slot(spawn.slot, spawn.size, spawn.y, spawn.vx, spawn.vy, spawn.frame);
    }

    int kill_slot = 0;
    bool kill_explode = false;
    while(net().poll_meteor_kill(kill_slot, kill_explode))
    {
        _kill_meteor_slot(kill_slot, true, kill_explode, false, kill_explode);
    }

    int slow_frames = 0;
    if(net().poll_slow(slow_frames))
    {
        _slow_timer = slow_frames;
    }
}

void game_scene::_fire()
{
    if(_fire_cooldown > 0)
    {
        return;
    }

    auto spawn_bullet = [&](bn::fixed y_off, bn::fixed vy, int kind) {
        for(auto& b : _bullets)
        {
            if(b.active) continue;
            b.active = true;
            b.x = _x + 16;
            b.y = _y + y_off;
            b.vx = (kind == 2) ? bn::fixed(4) : bn::fixed(3.2);
            b.vy = vy;
            b.kind = kind;
            b.sprite = bn::sprite_items::bullets.create_sprite(b.x, b.y, kind);
            break;
        }
    };

    int kind = int(_weapon);
    if(_weapon == weapon_type::spread)
    {
        spawn_bullet(-4, -0.8, kind);
        spawn_bullet(0, 0, kind);
        spawn_bullet(4, 0.8, kind);
        _fire_cooldown = 12;
    }
    else if(_weapon == weapon_type::heavy)
    {
        spawn_bullet(0, 0, kind);
        _fire_cooldown = 18;
    }
    else
    {
        spawn_bullet(0, 0, kind);
        _fire_cooldown = 10;
    }

    if(_is_multi())
    {
        net().send_fire(_y, kind);
    }
}

void game_scene::_use_powerup()
{
    if(_held == powerup_type::none)
    {
        return;
    }

    switch(_held)
    {
    case powerup_type::shield:
        _has_shield = true;
        _shield_fx = bn::sprite_items::shield_fx.create_sprite(_x, _y, 0);
        break;
    case powerup_type::slow:
        _slow_timer = 60 * 3;
        if(_is_multi())
        {
            net().send_slow(_slow_timer);
        }
        break;
    case powerup_type::clear:
        for(int i = 0; i < max_meteors; ++i)
        {
            meteor& m = _meteors[i];
            if(! m.active) continue;
            bn::fixed dx = m.x - _x;
            bn::fixed dy = m.y - _y;
            if(dx * dx + dy * dy < bn::fixed(55 * 55))
            {
                _kill_meteor_slot(i, false, true, false, true);
            }
        }
        break;
    case powerup_type::weapon:
        _weapon = weapon_type(bn::min(int(_weapon) + 1, 2));
        save().unlocked_weapon = bn::max(save().unlocked_weapon, int(_weapon));
        break;
    case powerup_type::life:
        _lives = bn::min(_lives + 1, max_lives);
        break;
    default:
        break;
    }

    _held = powerup_type::none;
    _rebuild_hud();
}

void game_scene::_hit_player()
{
    if(_i_frames > 0)
    {
        return;
    }

    if(_has_shield)
    {
        _has_shield = false;
        _shield_fx.reset();
        _i_frames = i_frame_duration / 2;
        return;
    }

    --_lives;
    _i_frames = i_frame_duration;
    _rebuild_hud();

    for(auto& e : _blasts)
    {
        if(e.active) continue;
        e.active = true;
        e.age = 0;
        e.sprite = bn::sprite_items::explosion.create_sprite(_x, _y, 0);
        e.anim = bn::create_sprite_animate_action_once(
            *e.sprite, 3, bn::sprite_items::explosion.tiles_item(), 0, 1, 2, 3);
        break;
    }

    if(_lives <= 0)
    {
        _dead = true;
        _game_over_timer = 0;
        if(_is_multi())
        {
            net().send_dead();
        }
    }
}

void game_scene::_next_level()
{
    ++_level;
    _passed = 0;
    _rebuild_hud();
}

void game_scene::_update_player()
{
    bn::fixed accel = 0.35;
    bn::fixed max_speed = 2.4 + bn::fixed(_level) * bn::fixed(0.05);
    bool braking = bn::keypad::b_held();

    if(bn::keypad::left_held()) _vx -= accel;
    if(bn::keypad::right_held()) _vx += accel;
    if(bn::keypad::up_held()) _vy -= accel;
    if(bn::keypad::down_held()) _vy += accel;

    _vx *= bn::fixed(0.86);
    _vy *= bn::fixed(0.86);

    if(braking)
    {
        _vx *= bn::fixed(0.55);
        _vy *= bn::fixed(0.55);
    }

    _vx = bn::clamp(_vx, -max_speed, max_speed);
    _vy = bn::clamp(_vy, -max_speed, max_speed);
    _x += _vx;
    _y += _vy;
    _x = bn::clamp(_x, bn::fixed(-104), bn::fixed(40));
    _y = bn::clamp(_y, bn::fixed(-64), bn::fixed(64));

    ++_engine_frame;
    int local = _is_multi() ? net().local_id() : 0;
    // ships sheet: players across (5), engine frames down (2)
    const int graphics = ((_engine_frame / 6) % 2) * 5 + local;
    if(_ship)
    {
        _ship->set_item(bn::sprite_items::ships, graphics);
        _ship->set_position(_x, _y);
        _ship->set_visible(_i_frames == 0 || (_i_frames / 2) % 2 == 0);
    }

    if(_shield_fx)
    {
        _shield_fx->set_position(_x, _y);
        _shield_fx->set_item(bn::sprite_items::shield_fx, (_engine_frame / 8) % 2);
    }

    if(bn::keypad::a_pressed() || bn::keypad::a_held())
    {
        // tap-ish fire via cooldown
        _fire();
    }

    if(bn::keypad::r_pressed())
    {
        _use_powerup();
    }

    if(_is_multi())
    {
        net().send_state(_x, _y, _lives, braking);
    }
}

void game_scene::_update_meteors()
{
    bn::fixed slow = _slow_timer > 0 ? bn::fixed(0.45) : bn::fixed(1);

    for(int i = 0; i < max_meteors; ++i)
    {
        meteor& m = _meteors[i];
        if(! m.active)
        {
            continue;
        }

        m.x += m.vx * slow;
        m.y += m.vy * slow;
        ++m.anim;
        if(m.anim % 6 == 0)
        {
            m.frame = (m.frame + 1) & 3;
            if(m.sprite)
            {
                m.sprite->set_item(m.size ? bn::sprite_items::meteor32 : bn::sprite_items::meteor16, m.frame);
            }
        }
        if(m.sprite)
        {
            m.sprite->set_position(m.x, m.y);
        }

        if(m.x < -140)
        {
            _kill_meteor_slot(i, false, false, false, true);
            continue;
        }

        // collide player
        bn::fixed rr = m.size ? bn::fixed(14) : bn::fixed(8);
        bn::fixed dx = m.x - _x;
        bn::fixed dy = m.y - _y;
        if(dx * dx + dy * dy < rr * rr)
        {
            _kill_meteor_slot(i, false, false, false, false);
            _hit_player();
        }
    }
}

void game_scene::_update_bullets()
{
    for(auto& b : _bullets)
    {
        if(! b.active) continue;
        b.x += b.vx;
        b.y += b.vy;
        if(b.sprite) b.sprite->set_position(b.x, b.y);

        if(b.x > 130 || b.y < -80 || b.y > 80)
        {
            b.active = false;
            b.sprite.reset();
            continue;
        }

        for(int mi = 0; mi < max_meteors; ++mi)
        {
            meteor& m = _meteors[mi];
            if(! m.active) continue;
            bn::fixed rr = m.size ? bn::fixed(14) : bn::fixed(8);
            bn::fixed dx = m.x - b.x;
            bn::fixed dy = m.y - b.y;
            if(dx * dx + dy * dy < rr * rr)
            {
                int dmg = (b.kind == 2) ? 3 : 1;
                m.hp -= dmg;
                b.active = false;
                b.sprite.reset();
                if(m.hp <= 0)
                {
                    _kill_meteor_slot(mi, false, true, true, true);
                }
                break;
            }
        }
    }
}

void game_scene::_update_pickups()
{
    for(auto& p : _pickups)
    {
        if(! p.active) continue;
        p.x -= bn::fixed(0.8);
        if(p.sprite) p.sprite->set_position(p.x, p.y);
        for(auto& s : p.label)
        {
            s.set_position(s.x() - bn::fixed(0.8), s.y());
        }
        if(p.x < -130)
        {
            p.active = false;
            p.sprite.reset();
            p.label.clear();
            continue;
        }

        bn::fixed dx = p.x - _x;
        bn::fixed dy = p.y - _y;
        if(dx * dx + dy * dy < bn::fixed(12 * 12))
        {
            _toast_text = _powerup_name(p.type);
            _toast_timer = 90;

            if(p.type == powerup_type::life)
            {
                _lives = bn::min(_lives + 1, max_lives);
            }
            else if(p.type == powerup_type::weapon)
            {
                _weapon = weapon_type(bn::min(int(_weapon) + 1, 2));
            }
            else
            {
                _held = p.type;
            }
            p.active = false;
            p.sprite.reset();
            p.label.clear();
            _rebuild_hud();
        }
    }
}

void game_scene::_update_blasts()
{
    for(auto& e : _blasts)
    {
        if(! e.active) continue;
        ++e.age;
        if(e.anim)
        {
            e.anim->update();
            if(e.anim->done())
            {
                e.active = false;
                e.anim.reset();
                e.sprite.reset();
            }
        }
        else if(e.age > 20)
        {
            e.active = false;
            e.sprite.reset();
        }
    }
}

void game_scene::_update_remote()
{
    if(! _is_multi()) return;
    net().update();
    int local = net().local_id();

    if(net().host())
    {
        net().send_tick(_engine_frame);
    }

    for(int i = 0; i < max_players; ++i)
    {
        if(i == local || ! _remote_ships[i]) continue;
        const remote_player& r = net().remote(i);
        if(r.active && r.alive)
        {
            _remote_ships[i]->set_visible(true);
            _remote_ships[i]->set_position(r.x, r.y);
            _remote_ships[i]->set_item(bn::sprite_items::ships, i);

            const int shots = net().consume_fire(i);
            for(int s = 0; s < shots; ++s)
            {
                const int kind = r.weapon;
                for(auto& b : _bullets)
                {
                    if(b.active) continue;
                    b.active = true;
                    b.x = r.x + 16;
                    b.y = r.y;
                    b.vx = (kind == 2) ? bn::fixed(4) : bn::fixed(3.2);
                    b.vy = 0;
                    b.kind = kind;
                    b.sprite = bn::sprite_items::bullets.create_sprite(b.x, b.y, kind);
                    break;
                }
            }
        }
        else
        {
            _remote_ships[i]->set_visible(false);
        }
    }
}

scene_id game_scene::update()
{
    if(_stars)
    {
        _stars->update(_paused ? bn::fixed(0.05) : bn::fixed(0.55));
    }

    if(_dead || _won)
    {
        ++_game_over_timer;
        _hud.clear();
        _text.set_center_alignment();
        if(_won)
        {
            _text.generate(0, -10, "SECTOR CLEAR", _hud);
            _text.generate(0, 10, "A: continue", _hud);
            if(bn::keypad::a_pressed())
            {
                if(_is_campaign())
                {
                    save().campaign_progress = bn::max(save().campaign_progress, campaign_chapter() + 1);
                    write_save();
                    if(campaign_chapter() < 4)
                    {
                        campaign_chapter() = campaign_chapter() + 1;
                        return scene_id::story;
                    }
                    return scene_id::credits;
                }
                return scene_id::title;
            }
        }
        else
        {
            _text.generate(0, -16, "GAME OVER", _hud);
            bn::string<24> sc = "Score ";
            sc += bn::to_string<8>(_score);
            _text.generate(0, 4, sc, _hud);
            _text.generate(0, 24, "A: title", _hud);
            if(_is_multi())
            {
                _text.generate(0, 40, "Last survivor wins", _hud);
            }
            if(bn::keypad::a_pressed() && _game_over_timer > 40)
            {
                if(_score > save().high_score)
                {
                    save().high_score = _score;
                    write_save();
                }
                if(_is_multi()) net().stop();
                return scene_id::title;
            }
        }
        return scene_id::game;
    }

    if(bn::keypad::start_pressed())
    {
        _paused = ! _paused;
        _rebuild_hud();
    }

    if(_paused)
    {
        return scene_id::game;
    }

    if(_fire_cooldown > 0) --_fire_cooldown;
    if(_i_frames > 0) --_i_frames;
    if(_slow_timer > 0) --_slow_timer;
    if(_toast_timer > 0)
    {
        --_toast_timer;
        if(_toast_timer == 0)
        {
            _toast_text = nullptr;
            _rebuild_hud();
        }
    }

    _update_player();
    _update_remote();
    _apply_net_world();

    int spawn_every = bn::max(18, 50 - _level * 3);
    if(++_spawn_timer >= spawn_every)
    {
        _spawn_timer = 0;
        // Host owns meteor spawns in multiplayer so both screens share the field.
        if(! _is_multi() || net().host())
        {
            _spawn_meteor();
        }
    }

    _update_meteors();
    _update_bullets();
    _update_pickups();
    _update_blasts();

    if(_passed >= _meteors_for_level())
    {
        _next_level();
    }

    if(_is_campaign() && _passed + _score / 10 >= _chapter_goal)
    {
        _won = true;
        _game_over_timer = 0;
    }

    // Multiplayer win: you survive while all remotes dead (and connected)
    if(_is_multi() && net().is_connected())
    {
        bool any_other_alive = false;
        int local = net().local_id();
        for(int i = 0; i < net().player_count(); ++i)
        {
            if(i == local) continue;
            if(net().remote(i).alive) any_other_alive = true;
        }
        if(! any_other_alive && net().player_count() >= 2 && _timer_alive_enough())
        {
            _won = true;
        }
    }

    if((_engine_frame & 15) == 0)
    {
        _rebuild_hud();
    }

    return scene_id::game;
}

bool game_scene::_timer_alive_enough()
{
    // reuse engine frame as soft timer
    return _engine_frame > 120;
}

} // namespace cc
