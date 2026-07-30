#ifndef CC_CONSTANTS_HPP
#define CC_CONSTANTS_HPP

namespace cc
{

constexpr int screen_w = 240;
constexpr int screen_h = 160;
constexpr int max_lives = 5;
constexpr int start_lives = 3;
constexpr int max_meteors = 24;
constexpr int max_bullets = 16;
constexpr int max_pickups = 6;
constexpr int max_players = 5;
constexpr int i_frame_duration = 60;
constexpr int brake_divisor = 5; // velocity *= 2/5 when braking roughly via fixed
constexpr int meteors_per_level_base = 12;
constexpr int meteors_per_level_step = 4;

enum class scene_id
{
    title,
    story_menu,
    story,
    options,
    multiplayer_menu,
    link_wait,
    game,
    pause,
    game_over,
    credits
};

enum class game_mode
{
    endless,
    campaign,
    multi_cable,
    multi_wireless,
    multi_online
};

enum class powerup_type : int
{
    none = -1,
    shield = 0,
    slow = 1,
    clear = 2,
    weapon = 3,
    life = 4
};

enum class weapon_type : int
{
    single = 0,
    spread = 1,
    heavy = 2
};

} // namespace cc

#endif
