#ifndef CC_SAVE_DATA_HPP
#define CC_SAVE_DATA_HPP

#include "bn_sram.h"
#include "bn_string_view.h"

namespace cc
{

struct save_blob
{
    char magic[4] = {'C', 'C', '0', '1'};
    int music_volume = 1; // 0..1 mapped later
    int sfx_volume = 1;
    int campaign_progress = 0;
    int high_score = 0;
    int unlocked_weapon = 0;
};

inline save_blob& save()
{
    static save_blob data;
    return data;
}

inline void load_save()
{
    save_blob tmp;
    bn::sram::read(tmp);
    if(tmp.magic[0] == 'C' && tmp.magic[1] == 'C' && tmp.magic[2] == '0' && tmp.magic[3] == '1')
    {
        save() = tmp;
    }
}

inline void write_save()
{
    bn::sram::write(save());
}

} // namespace cc

#endif
