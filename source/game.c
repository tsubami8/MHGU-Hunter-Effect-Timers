#include "game.h"

// MHGU 1.4.0, ExeFS main build ID FB08F1D20FD1204FE7070672777F869B.
// Addresses are relative to main's text, independent of the guest load address.
enum {
    PLAYER_MANAGER = 0x1885b58,
    GAME_CONTROL = 0x188ca48,
    AREA_MANAGER = 0x18977a0,
    GET_AREA_CLIMATE = 0x500818,
    LOCAL_SLOT = 0x30,
    AREA_TYPE = 0x4c,
    PLAYER_SLOTS = 0x1c,
    HUNTER_ZONE = 0x1054,
    HUNTER_HP = 0x13d6,
    HUNTER_EQUIPMENT = 0x13c4,
    ACTION_GROUP = 0x2506,
    ACTION_NUMBER = 0x250a,
    STUN_ROUND_TIMER = 0x2528,
    STUN_ROUNDS_LEFT = 7,
    WEAPON_TYPE = 0x4d4,
    DUAL_BLADES = 11,
    HAS_DUAL_BLADES_DEMON_MODE = 0x4c4,
    HAS_ACTIVE_DRAGONHEART = 0x224,
    APPLY_ITEM_EFFECT = 0x2fafc4,
    ACTIVE_ITEM_ID = 0x2520,
};

enum ItemEffect {
    ITEM_DASH, ITEM_ATTACK, ITEM_DEFENSE, ITEM_RECOVERY, ITEM_HOT, ITEM_COLD,
    ITEM_TRACKING, ITEM_ARTS, ITEM_ALCHEMY_RECOVERY = 18, ITEM_ALCHEMY_GAUGE,
};

const Effect effects[] = {
    [ITEM_DASH] = {"Stamina Boost", "强走效果", 0x2904, BUFF, DASH_JUICE},
    [ITEM_ATTACK] = {"Item Attack Up", "道具攻击力提升", 0x2808, BUFF, FRAME_FLOAT},
    [ITEM_DEFENSE] = {"Item Defense Up", "道具防御力提升", 0x280c, BUFF, FRAME_FLOAT},
    [ITEM_RECOVERY] = {"Recovery Up", "自然回复力提升", 0x13ec, BUFF, FRAME_FLOAT},
    [ITEM_HOT] = {"Cold Protection", "耐寒效果", 0x2880, BUFF, HOT_DRINK},
    [ITEM_COLD] = {"Heat Protection", "耐热效果", 0x287c, BUFF, COLD_DRINK},
    [ITEM_TRACKING] = {"Monster Tracking", "怪物追踪效果", 0x2908, BUFF, FRAME_FLOAT},
    [ITEM_ARTS] = {"Hunter Arts Boost", "狩技槽积累加快", 0x29d4, BUFF, FRAME_FLOAT},
    {"Poison", "中毒", 0x288c, AILMENT, POISON_TICKS},
    {"Defense Down", "防御力下降", 0x28cc, AILMENT, DEFENSE_DOWN},
    {"Fireblight", "火属性异常", 0x28ec, AILMENT, FRAME_FLOAT},
    {"Waterblight", "水属性异常", 0x28f4, AILMENT, FRAME_FLOAT},
    {"Thunderblight", "雷属性异常", 0x28f8, AILMENT, FRAME_FLOAT},
    {"Iceblight", "冰属性异常", 0x28fc, AILMENT, FRAME_FLOAT},
    {"Dragonblight", "龙属性异常", 0x2900, AILMENT, DRAGONBLIGHT},
    {"Frenzy Overcome", "克服狂龙病毒", 0x2990, BUFF, FRAME_FLOAT},
    {"Frenzy (Failed)", "狂龙病毒发作", 0x2988, AILMENT, FRAME_FLOAT},
    {"Stun (max)", "昏厥（最长）", STUN_ROUND_TIMER, AILMENT, STUN_ROUNDS},
    [ITEM_ALCHEMY_RECOVERY] = {"Alchemy Recovery Up", "炼金回复力提升", 0x27f4, BUFF, FRAME_FLOAT},
    [ITEM_ALCHEMY_GAUGE] = {"Alchemy Gauge Boost", "炼金槽积累加快", 0x27fc, BUFF, FRAME_FLOAT},
};

const unsigned effect_count = sizeof(effects) / sizeof(effects[0]);

static const unsigned item_effects[] = {
    ITEM_DASH, ITEM_ATTACK, ITEM_DEFENSE, ITEM_RECOVERY, ITEM_HOT, ITEM_COLD,
    ITEM_TRACKING, ITEM_ARTS, ITEM_ALCHEMY_RECOVERY, ITEM_ALCHEMY_GAUGE,
};
enum { ITEM_EFFECT_COUNT = sizeof(item_effects) / sizeof(item_effects[0]) };

// Item IDs are the IDs in the original item-use dispatch table.
const Item items[] = {
    {15, ITEM_DASH, "Dash Juice", "狂奔药"},
    {16, ITEM_DASH, "Mega Dash Juice", "狂奔药G"},
    {19, ITEM_ATTACK, "Might Pill", "怪力药丸"},
    {303, ITEM_ATTACK, "Might Seed", "怪力种子"},
    {22, ITEM_DEFENSE, "Adamant Pill", "忍耐药丸"},
    {304, ITEM_DEFENSE, "Adamant Seed", "忍耐种子"},
    {14, ITEM_RECOVERY, "Immunizer", "活力剂"},
    {248, ITEM_ALCHEMY_RECOVERY, "Alchemy Immunizer", "炼金活力剂"},
    {24, ITEM_HOT, "Hot Drink", "热饮"},
    {53, ITEM_HOT, "Hot Meat", "热烤肉"},
    {282, ITEM_HOT, "Spicy Mushroom", "特产菇泡菜"},
    {23, ITEM_COLD, "Cool Drink", "冷饮"},
    {52, ITEM_COLD, "Chilled Meat", "冷烤肉"},
    {283, ITEM_COLD, "Frozen Berry", "冰结晶草莓"},
    {26, ITEM_TRACKING, "Psychoserum", "千里眼之药"},
    {33, ITEM_ARTS, "Hunter's Drink", "狩技饮料"},
    {1729, ITEM_ARTS, "EZ Hunter's Drink", "狩技饮料补给品"},
    {38, ITEM_RECOVERY, "Yukumo Egg", "结云温泉蛋"},
    {39, ITEM_RECOVERY, "Tanzia Chips", "丹治亚土豆片"},
    {361, ITEM_RECOVERY, "Gourmet Fish", "烤熟的鱼"},
    {363, ITEM_RECOVERY, "Sushifish", "刺身鱼"},
    {54, ITEM_RECOVERY, "Mosswine Jerky", "蘑菇猪肉干"},
    {296, ITEM_DASH, "Mopeshroom", "疲劳菇"},
    {49, ITEM_DASH, "Rare Steak", "半生肉"},
    {50, ITEM_DASH, "Well-done Steak", "烤熟的肉"},
    {256, ITEM_ALCHEMY_GAUGE, "Alchemy Booster", "炼金摇摇药"},
};

const unsigned item_count = sizeof(items) / sizeof(items[0]);
static uintptr_t main_base;
static uintptr_t tracked_hunter;

typedef struct {
    void *target, *detour, *original, *context, **context_cell;
    bool has_context;
} ForgeHook;

extern ForgeHook forge_hook_create(void *target, void *detour, void **original);
typedef void (*ApplyItemEffect)(uintptr_t);
static ApplyItemEffect original_apply_item_effect;

static struct {
    float frames;
    int item_plus_one;
} item_sources[ITEM_EFFECT_COUNT];

typedef struct {
    float remaining;
    float total;
    uint16_t slot;
} TimerState;

static TimerState timers[sizeof(effects) / sizeof(effects[0])];
static uintptr_t timer_hunter;

static void select_timer_hunter(uintptr_t hunter)
{
    if (hunter == timer_hunter) return;
    timer_hunter = hunter;
    for (unsigned i = 0; i < effect_count; ++i) timers[i].total = 0;
}

static void store_timer(unsigned index, uint16_t slot, float remaining, bool refreshed)
{
    TimerState *state = &timers[index];
    if (refreshed || state->slot != slot || remaining <= 0 ||
        state->total <= 0 || remaining > state->remaining) {
        state->total = remaining;
    }
    state->slot = slot;
    state->remaining = remaining;
}

static uint32_t read_word(uintptr_t address)
{
    return *(const volatile uint32_t *)address;
}

static float read_frame_timer(uintptr_t address)
{
    return *(const volatile float *)address;
}

static int read_short(uintptr_t address)
{
    return *(const volatile int16_t *)address;
}

static unsigned read_byte(uintptr_t address)
{
    return *(const volatile uint8_t *)address;
}

bool game_init(uintptr_t main_text)
{
    if (!main_text ||
        read_word(main_text + 0x29cec4) != 0xe92d4ff0 ||
        read_word(main_text + 0x29f814) != 0xe7846000 ||
        read_word(main_text + 0x500818) != 0xe92d4830 ||
        read_word(main_text + 0x53aa84) != 0xed911a00) return false;
    main_base = main_text;
    return true;
}

uintptr_t game_local_hunter(void)
{
    if (!main_base) return 0;
    const uintptr_t control = read_word(main_base + GAME_CONTROL);
    const uintptr_t manager = read_word(main_base + PLAYER_MANAGER);
    if (!control || !manager || read_byte(control + AREA_TYPE) != 0xff) return 0;

    const unsigned index = read_word(control + LOCAL_SLOT);
    if (index >= 4) return 0;
    const uintptr_t hunter = read_word(manager + PLAYER_SLOTS + sizeof(uint32_t) * index);
    return hunter && read_short(hunter + HUNTER_HP) > 0 ? hunter : 0;
}

static void refresh_item_sources(uintptr_t hunter)
{
    const bool same_hunter = hunter && hunter == tracked_hunter;
    for (unsigned i = 0; i < ITEM_EFFECT_COUNT; ++i) {
        if (!item_sources[i].item_plus_one) continue;
        const float remaining = same_hunter ? read_frame_timer(hunter + effects[item_effects[i]].offset) : 0;
        if (!(remaining > 0 && remaining <= item_sources[i].frames)) {
            item_sources[i].item_plus_one = 0;
        }
        item_sources[i].frames = remaining;
    }
    tracked_hunter = hunter;
}

int game_current_item(uintptr_t hunter, unsigned effect_index)
{
    if (hunter != tracked_hunter) return -1;
    for (unsigned i = 0; i < ITEM_EFFECT_COUNT; ++i) {
        if (item_effects[i] == effect_index) return item_sources[i].item_plus_one - 1;
    }
    return -1;
}

static void watch_applied_item(uintptr_t hunter)
{
    if (hunter == 0 || hunter != game_local_hunter()) {
        original_apply_item_effect(hunter);
        return;
    }

    const unsigned id = (unsigned)read_short(hunter + ACTIVE_ITEM_ID);
    unsigned item_index = 0;
    while (item_index < item_count && items[item_index].id != id) ++item_index;
    refresh_item_sources(hunter);
    float before[ITEM_EFFECT_COUNT];
    for (unsigned i = 0; i < ITEM_EFFECT_COUNT; ++i) {
        before[i] = read_frame_timer(hunter + effects[item_effects[i]].offset);
    }

    original_apply_item_effect(hunter);

    select_timer_hunter(hunter);
    for (unsigned i = 0; i < ITEM_EFFECT_COUNT; ++i) {
        const unsigned index = item_effects[i];
        const uint16_t slot = effects[index].offset;
        const float remaining = read_frame_timer(hunter + slot);
        if (remaining != before[i]) {
            item_sources[i].item_plus_one = remaining > 0 && item_index < item_count ? (int)item_index + 1 : 0;
            item_sources[i].frames = remaining;
            store_timer(index, slot, remaining > 0 && remaining <= 216000 ? remaining : 0, true);
        }
    }
}

bool game_track_item_use(void)
{
    if (!main_base) return false;
    uintptr_t target = main_base + APPLY_ITEM_EFFECT;
    if (read_word(target) != 0xe92d41f0 || read_word(target + 4) != 0xed2d8b02) return false;
    void *original = 0;
    forge_hook_create((void *)target, (void *)watch_applied_item, &original);
    original_apply_item_effect = (ApplyItemEffect)original;
    return original != 0;
}

int game_climate(uintptr_t hunter)
{
    if (!(read_frame_timer(hunter + 0x287c) > 0 ||
          read_frame_timer(hunter + 0x2880) > 0)) return 0;

    const uintptr_t area = read_word(main_base + AREA_MANAGER);
    const unsigned zone = read_byte(hunter + HUNTER_ZONE);
    if (!area || zone == 0xff || !read_word(area + 0x3bc)) return 0;

    typedef int (*GetAreaClimate)(uintptr_t, unsigned);
    const GetAreaClimate get_climate = (GetAreaClimate)(main_base + GET_AREA_CLIMATE);
    return get_climate(area, zone);
}

static bool hunter_flag(uintptr_t hunter, unsigned method_slot)
{
    const uintptr_t vtable = read_word(hunter);
    if (!vtable) return false;
    typedef int (*HunterFlag)(uintptr_t);
    const HunterFlag flag = (HunterFlag)(uintptr_t)read_word(vtable + method_slot);
    return flag && flag(hunter) == 1;
}

static bool dash_drains_twice_as_fast(uintptr_t hunter)
{
    const uintptr_t equipment = read_word(hunter + HUNTER_EQUIPMENT);
    return equipment && read_byte(equipment + WEAPON_TYPE) == DUAL_BLADES &&
           hunter_flag(hunter, HAS_DUAL_BLADES_DEMON_MODE);
}

static float read_timer(uintptr_t hunter, const Effect *effect, uint16_t *slot)
{
    float frames;
    *slot = effect->offset;
    if (effect->format == POISON_TICKS) {
        const int ticks = read_short(hunter + effect->offset);
        if (ticks <= 0) return 0;
        frames = (ticks - 1) * 60.0f + read_frame_timer(hunter + 0x2890);
    } else if (effect->format == STUN_ROUNDS) {
        if (read_short(hunter + ACTION_GROUP) != 0x40 || read_short(hunter + ACTION_NUMBER) != 26) return 0;
        const unsigned rounds = read_byte(hunter + STUN_ROUNDS_LEFT);
        if (rounds == 0 || rounds > 4) return 0;
        frames = (rounds - 1) * 750.0f + read_frame_timer(hunter + effect->offset);
    } else {
        frames = read_frame_timer(hunter + effect->offset);
        if (effect->format == DEFENSE_DOWN && !(frames > 0)) {
            *slot = 0x28c8;
            frames = read_frame_timer(hunter + *slot);
        }
    }
    if (!(frames > 0 && frames <= 216000)) return 0;

    // Dragonheart maintains a small internal timer while its HP condition holds.
    if (effect->format == DRAGONBLIGHT && hunter_flag(hunter, HAS_ACTIVE_DRAGONHEART)) return 0;
    return frames;
}

void game_update_timers(void)
{
    const uintptr_t hunter = game_local_hunter();
    select_timer_hunter(hunter);
    if (original_apply_item_effect) refresh_item_sources(hunter);
    if (!hunter) return;

    for (unsigned i = 0; i < effect_count; ++i) {
        uint16_t slot;
        const float remaining = read_timer(hunter, &effects[i], &slot);
        store_timer(i, slot, remaining, false);
    }
}

GameTimer game_timer(uintptr_t hunter, unsigned effect_index, int climate)
{
    const TimerState *state = &timers[effect_index];
    if (!hunter || hunter != timer_hunter || state->total <= 0) return (GameTimer){0, 0};
    const enum TimerFormat format = effects[effect_index].format;
    float rate = 60.0f;
    if (format == DASH_JUICE && dash_drains_twice_as_fast(hunter)) rate = 120.0f;

    // The engine spends 1.5 timer frames per simulation frame in extreme climates.
    if ((format == COLD_DRINK && climate == 2) ||
        (format == HOT_DRINK && climate == 4)) rate = 90.0f;
    return (GameTimer){state->remaining / rate, state->remaining / state->total};
}
