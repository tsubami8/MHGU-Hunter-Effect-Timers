#ifndef EFFECT_TIMERS_GAME_H
#define EFFECT_TIMERS_GAME_H

#include <stdbool.h>
#include <stdint.h>

enum EffectKind { BUFF, AILMENT };
enum TimerFormat { FRAME_FLOAT, DASH_JUICE, HOT_DRINK, COLD_DRINK, POISON_TICKS,
                   DEFENSE_DOWN, DRAGONBLIGHT, STUN_ROUNDS };

typedef struct {
    const char *english;
    const char *chinese;
    uint16_t offset;
    enum EffectKind kind;
    enum TimerFormat format;
} Effect;

typedef struct {
    uint16_t id;
    unsigned primary_effect;
    const char *english;
    const char *chinese;
} Item;

typedef struct {
    float seconds;
    float fraction;
} GameTimer;

extern const Effect effects[];
extern const unsigned effect_count;
extern const Item items[];
extern const unsigned item_count;

bool game_init(uintptr_t main_text);
bool game_track_item_use(void);
void game_update_timers(void);
int game_current_item(uintptr_t hunter, unsigned effect_index);
uintptr_t game_local_hunter(void);
int game_climate(uintptr_t hunter);
GameTimer game_timer(uintptr_t hunter, unsigned effect_index, int climate);

#endif
