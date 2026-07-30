#ifndef CC_GAME_SCENE_HPP
#define CC_GAME_SCENE_HPP

#include "cc_scene.hpp"
#include "cc_starfield.hpp"
#include "cc_constants.hpp"

#include "bn_vector.h"
#include "bn_sprite_ptr.h"
#include "bn_sprite_animate_actions.h"
#include "bn_sprite_text_generator.h"
#include "bn_optional.h"
#include "bn_fixed.h"
namespace cc
{

class game_scene : public scene
{
public:
    game_scene();
    void enter() override;
    void leave() override;
    scene_id update() override;

private:
    struct meteor
    {
        bool active = false;
        bn::fixed x;
        bn::fixed y;
        bn::fixed vx;
        bn::fixed vy;
        int hp = 1;
        int size = 0; // 0=16, 1=32
        int frame = 0;
        int anim = 0;
        bn::optional<bn::sprite_ptr> sprite;
    };

    struct bullet
    {
        bool active = false;
        bn::fixed x;
        bn::fixed y;
        bn::fixed vx;
        bn::fixed vy;
        int kind = 0;
        bn::optional<bn::sprite_ptr> sprite;
    };

    struct pickup
    {
        bool active = false;
        bn::fixed x;
        bn::fixed y;
        powerup_type type = powerup_type::shield;
        bn::optional<bn::sprite_ptr> sprite;
        bn::vector<bn::sprite_ptr, 8> label;
    };

    struct blast
    {
        bool active = false;
        int age = 0;
        bn::optional<bn::sprite_ptr> sprite;
        bn::optional<bn::sprite_animate_action<4>> anim;
    };

    void _spawn_meteor();
    void _spawn_meteor_slot(int slot, int size, bn::fixed y, bn::fixed vx, bn::fixed vy, int frame);
    void _kill_meteor_slot(int slot, bool explode, bool allow_drop, bool count_progress);
    void _apply_net_world();
    void _fire();
    void _use_powerup();
    void _hit_player();
    void _rebuild_hud();
    void _set_pickup_label(pickup& p);
    [[nodiscard]] static const char* _powerup_name(powerup_type type);
    void _update_player();
    void _update_meteors();
    void _update_bullets();
    void _update_pickups();
    void _update_blasts();
    void _update_remote();
    [[nodiscard]] bool _is_multi() const;
    [[nodiscard]] bool _is_campaign() const;
    [[nodiscard]] int _meteors_for_level() const;
    void _next_level();
    [[nodiscard]] bool _timer_alive_enough();

    bn::optional<starfield> _stars;
    bn::optional<bn::sprite_ptr> _ship;
    bn::optional<bn::sprite_ptr> _shield_fx;
    bn::optional<bn::sprite_ptr> _remote_ships[max_players];
    bn::sprite_text_generator _text;
    bn::vector<bn::sprite_ptr, 56> _hud;
    bn::vector<bn::sprite_ptr, 8> _hearts;

    meteor _meteors[max_meteors];
    bullet _bullets[max_bullets];
    pickup _pickups[max_pickups];
    blast _blasts[8];

    bn::fixed _x;
    bn::fixed _y;
    bn::fixed _vx;
    bn::fixed _vy;
    int _lives = start_lives;
    int _level = 1;
    int _score = 0;
    int _passed = 0;
    int _spawn_timer = 0;
    int _fire_cooldown = 0;
    int _i_frames = 0;
    int _slow_timer = 0;
    int _engine_frame = 0;
    bool _paused = false;
    bool _dead = false;
    bool _has_shield = false;
    powerup_type _held = powerup_type::none;
    weapon_type _weapon = weapon_type::single;
    int _toast_timer = 0;
    const char* _toast_text = nullptr;
    int _game_over_timer = 0;
    int _chapter_goal = 40;
    bool _won = false;
    int _meteor_seq = 0;
};

} // namespace cc

#endif
