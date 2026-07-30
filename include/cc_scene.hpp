#ifndef CC_SCENE_HPP
#define CC_SCENE_HPP

#include "cc_constants.hpp"

namespace cc
{

class scene
{
public:
    virtual ~scene() = default;
    virtual void enter() {}
    virtual void leave() {}
    virtual scene_id update() = 0;
};

scene_id& current_scene_id();
game_mode& current_game_mode();
int& campaign_chapter();
bool& request_emergency_restart();

bool emergency_chord_held();

} // namespace cc

#endif
