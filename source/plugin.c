#include "game.h"
#include "imgui_abi.h"

#include <stddef.h>

#define EXPORT __attribute__((visibility("default")))
#define MAX_EFFECTS 64
#define ITEM_FILTER_START 32
#define SETTINGS_VERSION 4u
#define RGBA(r, g, b, a) ((uint32_t)(r) | (uint32_t)(g) << 8 | (uint32_t)(b) << 16 | (uint32_t)(a) << 24)
#define TEXT RGBA(255, 255, 255, 255)
#define TEXT_OUTLINE RGBA(0, 0, 0, 220)
#define HP_GREEN RGBA(45, 205, 75, 120)
#define HP_YELLOW RGBA(245, 200, 60, 220)
#define HP_RED RGBA(230, 50, 50, 220)

struct PluginInfo {
    uint16_t major, minor, patch;
    char name[64];
};

extern uint32_t forge_mem_getMainTextAddr(void);
extern bool forge_graphics_isMenuVisible(void);
extern void forge_log(int level, const char *message, ...);

enum Sort { SHORTEST, LONGEST, BUFF_FIRST, AILMENT_FIRST };

typedef struct {
    uint32_t version;
    int x, y, scale, sort;
    uint8_t enabled;
    uint8_t visible[MAX_EFFECTS];
    int row_height, background_transparency;
    int split_columns;
} Settings;

typedef struct {
    unsigned index;
    int item_index;
    float seconds;
    float fraction;
} Row;

typedef struct {
    uint32_t edge, shadow;
} FrameColors;

static const FrameColors buff_frame = {RGBA(70, 130, 230, 200), RGBA(12, 24, 49, 230)};
static const FrameColors ailment_frame = {RGBA(225, 88, 99, 225), RGBA(58, 20, 29, 235)};
// Recomp resolves this shipped file through the active mod's RomFS, including renamed folders.
static const char settings_path[] = "app:/nativeNX/plugins/mhgu_effect_timers.cfg";
static const char legacy_settings_path[] = "mhgu_effect_timers.cfg";
static Settings settings;
static bool pending_save, save_failed, compatible, item_tracking, chinese;
static bool preview = true, editing_settings;
static ImFont *checked_font;

__attribute__((used, section(".module_name")))
static const struct {
    uint32_t reserved, length;
    char name[21];
} module_name = {0, 20, "Hunter Effect Timers"};

static void update_language(void)
{
    ImFont *font = igGetFont();
    if (!font || font == checked_font) return;
    checked_font = font;
    chinese = ImFont_IsGlyphInFont(font, 0x5012) &&
              ImFont_IsGlyphInFont(font, 0x8ba1) &&
              ImFont_IsGlyphInFont(font, 0x836f) &&
              ImFont_IsGlyphInFont(font, 0x72e9);
}

static const char *label(const char *english, const char *translated)
{
    return chinese ? translated : english;
}

static float clamp(float value, float minimum, float maximum)
{
    if (value < minimum) return minimum;
    if (value > maximum) return maximum;
    return value;
}

static void defaults(void)
{
    settings.version = SETTINGS_VERSION;
    settings.x = 96;
    settings.y = 26;
    settings.scale = 100;
    settings.sort = SHORTEST;
    settings.row_height = 40;
    settings.background_transparency = 45;
    settings.split_columns = false;
    settings.enabled = true;
    for (unsigned i = 0; i < MAX_EFFECTS; ++i) settings.visible[i] = true;
}

static bool read_settings(const char *path, Settings *stored)
{
    ImFileHandle file = igImFileOpen(path, "rb");
    if (!file) return false;

    const size_t file_sizes[] = {0, offsetof(Settings, row_height), offsetof(Settings, row_height),
                                offsetof(Settings, split_columns), sizeof(Settings)};
    const size_t version_size = sizeof(stored->version);
    bool read = igImFileRead(&stored->version, version_size, 1, file) == 1 &&
                stored->version >= 1 && stored->version <= SETTINGS_VERSION &&
                igImFileRead((char *)stored + version_size,
                             file_sizes[stored->version] - version_size, 1, file) == 1;
    igImFileClose(file);
    return read;
}

static void load_settings(void)
{
    defaults();
    Settings stored;
    if (!read_settings(settings_path, &stored)) {
        if (!read_settings(legacy_settings_path, &stored)) return;
        pending_save = true;
    }

    settings.x = (int)clamp(stored.x, 0, 100);
    settings.y = (int)clamp(stored.y, 0, 100);
    settings.scale = (int)clamp(stored.scale, 65, 200);
    settings.sort = (int)clamp(stored.sort, SHORTEST, AILMENT_FIRST);
    if (stored.version >= 3) {
        settings.row_height = (int)clamp(stored.row_height, 20, 76);
        settings.background_transparency = (int)clamp(stored.background_transparency, 0, 100);
    }
    if (stored.version >= 4) settings.split_columns = stored.split_columns != 0;
    settings.enabled = stored.enabled != 0;
    for (unsigned i = 0; i < MAX_EFFECTS; ++i) settings.visible[i] = stored.visible[i] != 0;
    if (stored.version == 1 && ITEM_FILTER_START + item_count <= MAX_EFFECTS) {
        for (unsigned i = 0; i < item_count; ++i) {
            settings.visible[ITEM_FILTER_START + i] = settings.visible[items[i].primary_effect];
        }
    }
}

static void save_settings(void)
{
    ImFileHandle file = igImFileOpen(settings_path, "r+b");
    bool written = false;
    if (file) {
        written = igImFileWrite(&settings, sizeof(settings), 1, file) == 1;
        written = igImFileClose(file) && written;
    }
    pending_save = false;
    save_failed = !written;
    if (save_failed) forge_log(2, "[Hunter Effect Timers] Could not write mod configuration: %s", settings_path);
}

static bool precedes(Row a, Row b)
{
    enum EffectKind first = effects[a.index].kind;
    enum EffectKind second = effects[b.index].kind;
    if (first != second && settings.sort == BUFF_FIRST) return first == BUFF;
    if (first != second && settings.sort == AILMENT_FIRST) return first == AILMENT;
    return settings.sort == LONGEST ? a.seconds > b.seconds : a.seconds < b.seconds;
}

static unsigned collect_rows(Row *rows, uintptr_t hunter, bool show_preview)
{
    unsigned count = 0;
    const int climate = hunter ? game_climate(hunter) : 0;
    unsigned examples[2] = {0, 0};
    const float preview_ratios[] = {1.0f, 0.50f, 0.24f, 0.72f};
    for (unsigned i = 0; i < effect_count; ++i) {
        const Effect *effect = &effects[i];
        int item_index = hunter ? game_current_item(hunter, i) : -1;
        if (show_preview && item_tracking) {
            for (unsigned j = 0; j < item_count; ++j) {
                if (items[j].primary_effect == i && settings.visible[ITEM_FILTER_START + j]) {
                    item_index = (int)j;
                    break;
                }
            }
        }
        const unsigned filter = item_index >= 0 ? ITEM_FILTER_START + (unsigned)item_index : i;
        if (!settings.visible[filter]) continue;
        GameTimer timer = game_timer(hunter, i, climate);
        if (show_preview && examples[effect->kind] < 2) {
            timer.seconds = (effect->kind == BUFF ? 147 : 21) - examples[effect->kind]++ * 10;
            timer.fraction = preview_ratios[count];
        }
        if (!(timer.seconds > 0)) continue;

        const Row row = {i, item_index, timer.seconds, timer.fraction};
        unsigned pos = count++;
        while (pos && precedes(row, rows[pos - 1])) {
            rows[pos] = rows[pos - 1];
            --pos;
        }
        rows[pos] = row;
    }
    return count;
}

static void format_time(float remaining, char text[7])
{
    unsigned seconds = (unsigned)remaining;
    if ((float)seconds < remaining) ++seconds;
    unsigned minutes = seconds / 60;
    unsigned tenth_minutes = minutes / 10;
    unsigned tenth_seconds = (seconds % 60) / 10;
    text[0] = '0' + (char)(tenth_minutes / 10);
    text[1] = '0' + (char)(tenth_minutes % 10);
    text[2] = '0' + (char)(minutes % 10);
    text[3] = ':';
    text[4] = '0' + (char)tenth_seconds;
    text[5] = '0' + (char)(seconds % 10);
    text[6] = '\0';
}

static bool shared_item(const Row *rows, unsigned count, unsigned index)
{
    if (rows[index].item_index < 0) return false;
    for (unsigned i = 0; i < count; ++i) {
        if (i != index && rows[i].item_index == rows[index].item_index) return true;
    }
    return false;
}

static char *append_text(char *out, const char *text)
{
    while (*text) *out++ = *text++;
    *out = '\0';
    return out;
}

static void fill_rect(ImDrawList *draw, float x0, float y0, float x1, float y1, uint32_t color)
{
    ImDrawList_AddRectFilled(draw, (ImVec2){x0, y0}, (ImVec2){x1, y1}, color, 0, 0);
}

static void draw_rule(ImDrawList *draw, float x, float right, float y, float stroke, FrameColors frame)
{
    fill_rect(draw, x, y, right, y + stroke, frame.shadow);
    const float pitch = 8 * stroke;
    for (float segment = x; segment < right; segment += pitch) {
        const float end = clamp(segment + pitch - stroke, segment, right);
        fill_rect(draw, segment, y, end, y + stroke, frame.edge);
    }
}

static void draw_end(ImDrawList *draw, float outer, float direction, float cap_width,
                     float y, float height, float stroke, FrameColors frame)
{
    const float x = direction > 0 ? outer : outer - cap_width;
    fill_rect(draw, x, y, x + cap_width, y + height, frame.shadow);
    const float outer_rail = outer + direction * stroke;
    const float inner_rail = outer + direction * (cap_width - stroke);
    const float x0 = direction > 0 ? outer_rail : inner_rail;
    const float x1 = direction > 0 ? inner_rail : outer_rail;
    fill_rect(draw, x0, y + stroke, x0 + stroke, y + height - stroke, frame.edge);
    fill_rect(draw, x1 - stroke, y + stroke, x1, y + height - stroke, frame.edge);
    for (int mark = 1; mark <= 2; ++mark) {
        const float notch_y = y + height * mark / 3.0f - stroke * 0.5f;
        fill_rect(draw, x0, notch_y, x1, notch_y + stroke, frame.edge);
    }
}

static void draw_label(ImDrawList *draw, ImFont *font, float size, float x, float y, const char *text)
{
    const float outline = clamp(size / 24.0f, 1.0f, 2.0f);
    const ImVec2 offsets[] = {{-outline, 0}, {outline, 0}, {0, -outline}, {0, outline}};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        ImDrawList_AddText_FontPtr(draw, font, size, (ImVec2){x + offsets[i].x, y + offsets[i].y},
                                  TEXT_OUTLINE, text, 0, 0, 0);
    }
    ImDrawList_AddText_FontPtr(draw, font, size, (ImVec2){x, y}, TEXT, text, 0, 0, 0);
}

static void format_progress(float seconds, unsigned percent, char text[16])
{
    char time[7];
    format_time(seconds, time);
    char *end = append_text(text, time + (time[0] == '0' ? 1 : 0));
    end = append_text(end, "  (");
    if (percent == 100) *end++ = '1';
    if (percent >= 10) *end++ = '0' + (char)(percent / 10 % 10);
    *end++ = '0' + (char)(percent % 10);
    append_text(end, "%)");
}

static void draw_column(ImDrawList *draw, ImFont *font, const Row *rows, unsigned visible,
                        float x, float y, float width, float line, float scale)
{
    if (!visible) return;
    const float base_font = igGetFontSize();
    if (!(base_font > 0)) return;
    const float stroke = clamp(line / 18.0f, 1, 4);
    const float cap_limit = width * 0.18f;
    const float cap = clamp(line * 0.46f, clamp(stroke * 4, 0, cap_limit), cap_limit);
    const float font_size = clamp(24.0f * scale, 8, line - stroke * 2);
    const float right = x + width;
    const float fill_left = x + cap;
    const float fill_right = right - cap;
    const unsigned alpha = (unsigned)(100 - settings.background_transparency) * 255 / 100;
    fill_rect(draw, fill_left, y, fill_right, y + visible * line, RGBA(0, 0, 0, alpha));

    bool previous_ailment = false;
    for (unsigned i = 0; i < visible; ++i) {
        const float top = y + i * line;
        const Effect *effect = &effects[rows[i].index];
        const bool ailment = effect->kind == AILMENT;
        const FrameColors frame = ailment ? ailment_frame : buff_frame;
        const float ratio = clamp(rows[i].fraction, 0, 1);
        const unsigned percent = (unsigned)(ratio * 100.0f + 0.5f);
        const uint32_t fill = percent >= 51 ? HP_GREEN : (percent >= 25 ? HP_YELLOW : HP_RED);
        fill_rect(draw, fill_left, top, fill_left + (fill_right - fill_left) * ratio, top + line, fill);
        draw_end(draw, x, 1, cap, top, line, stroke, frame);
        draw_end(draw, right, -1, cap, top, line, stroke, frame);
        // A shared rule between two effects also serves as the ailment's frame.
        draw_rule(draw, x, right, top, stroke, ailment || previous_ailment ? ailment_frame : buff_frame);
        previous_ailment = ailment;

        const int item_index = rows[i].item_index;
        const char *effect_name = label(effect->english, effect->chinese);
        const char *name = item_index < 0 ? effect_name : label(items[item_index].english, items[item_index].chinese);
        char full_name[96];
        if (shared_item(rows, visible, i)) {
            char *end = append_text(full_name, name);
            end = append_text(end, " / ");
            append_text(end, effect_name);
            name = full_name;
        }

        char progress[16];
        format_progress(rows[i].seconds, percent, progress);
        const float padding = stroke * 4;
        const float content_width = fill_right - fill_left - padding * 3;
        const float name_em = igCalcTextSize(name, 0, false, -1).x / base_font;
        const float value_em = igCalcTextSize(progress, 0, false, -1).x / base_font;
        const float size = clamp(content_width / (name_em + value_em), 4, font_size);
        const float text_y = top + (line - size) * 0.5f;
        draw_label(draw, font, size, fill_left + padding, text_y, name);
        draw_label(draw, font, size, fill_right - padding - value_em * size, text_y, progress);
    }
    draw_rule(draw, x, right, y + visible * line - stroke, stroke, previous_ailment ? ailment_frame : buff_frame);
}

static void draw_hud(const Row *rows, unsigned count, bool example)
{
    const ImGuiViewportPrefix *viewport = igGetMainViewport();
    if (!viewport || !(viewport->size.x >= 40 && viewport->size.y >= 12)) return;

    ImDrawList *draw = example ? igGetForegroundDrawList_Nil() :
                                igGetBackgroundDrawList_ViewportPtr(viewport);
    ImFont *font = igGetFont();
    if (!draw || !font) return;

    const float scale = viewport->size.y / 1080.0f * settings.scale / 100.0f;
    const float gap = settings.split_columns ? clamp(12.0f * scale, 0, viewport->size.x * 0.04f) : 0;
    const float width_limit = settings.split_columns ? (viewport->size.x - gap) * 0.5f : viewport->size.x;
    const float width = clamp(408.0f * scale, clamp(40, 0, width_limit), width_limit);
    const float total_width = settings.split_columns ? width * 2 + gap : width;
    const float line = clamp(settings.row_height * scale, 12, viewport->size.y);
    const float x = viewport->pos.x + (viewport->size.x - total_width) * settings.x / 100.0f;
    // Reserve one row when mapping the slider, so both columns keep the same top edge.
    const float y = viewport->pos.y + (viewport->size.y - line) * settings.y / 100.0f;
    const float space = viewport->pos.y + viewport->size.y - y;
    const unsigned max_visible = (unsigned)clamp((space + 0.01f) / line, 1, count);
    if (!settings.split_columns) {
        draw_column(draw, font, rows, max_visible, x, y, width, line, scale);
        return;
    }

    Row buffs[MAX_EFFECTS], ailments[MAX_EFFECTS];
    unsigned buff_count = 0, ailment_count = 0;
    for (unsigned i = 0; i < count; ++i) {
        if (effects[rows[i].index].kind == BUFF) buffs[buff_count++] = rows[i];
        else ailments[ailment_count++] = rows[i];
    }
    draw_column(draw, font, buffs, buff_count < max_visible ? buff_count : max_visible,
                x, y, width, line, scale);
    draw_column(draw, font, ailments, ailment_count < max_visible ? ailment_count : max_visible,
                x + width + gap, y, width, line, scale);
}

static void filter_checkbox(unsigned index, const char *name)
{
    bool visible = settings.visible[index];
    if (igCheckbox(name, &visible)) {
        settings.visible[index] = visible;
        pending_save = true;
    }
}

static void filter_menu(enum EffectKind kind, const char *heading)
{
    if (!igCollapsingHeader_TreeNodeFlags(heading, 0)) return;
    for (unsigned i = 0; i < effect_count; ++i) {
        if (effects[i].kind != kind) continue;
        filter_checkbox(i, label(effects[i].english, effects[i].chinese));
    }
}

static void item_filter_menu(void)
{
    if (!igCollapsingHeader_TreeNodeFlags(label("Items by name", "按道具名称筛选"), 0)) return;
    for (unsigned i = 0; i < item_count; ++i) {
        filter_checkbox(ITEM_FILTER_START + i, label(items[i].english, items[i].chinese));
    }
}

EXPORT void forge_onLoad(struct PluginInfo *info)
{
    const char name[] = "Hunter Effect Timers";
    info->major = 2;
    info->minor = 0;
    info->patch = 0;
    for (unsigned i = 0; i < sizeof(name); ++i) info->name[i] = name[i];
}

EXPORT void forge_onInit(void)
{
    compatible = effect_count <= ITEM_FILTER_START && ITEM_FILTER_START + item_count <= MAX_EFFECTS &&
                 game_init(forge_mem_getMainTextAddr());
    forge_log(compatible ? 1 : 2, "[Hunter Effect Timers] %s",
              compatible ? "v0.5, MHGU 1.4.0 profile active" : "Unsupported game build");
    if (!compatible) return;
    item_tracking = game_track_item_use();
    forge_log(item_tracking ? 1 : 2, "[Hunter Effect Timers] %s",
              item_tracking ? "Item name tracking active" : "Item apply hook unavailable; using effect names");
}

EXPORT void forge_onUpdate(float delta)
{
    (void)delta;
    game_update_timers();
}

EXPORT void forge_onImGuiInit(void *ctx, void *alloc, void *free)
{
    (void)ctx;
    (void)alloc;
    (void)free;
    load_settings();
}

EXPORT void forge_onImGuiRender(void)
{
    update_language();
    if (!igCollapsingHeader_TreeNodeFlags(
            label("Hunter Effect Timers###hunter_effect_timers", "猎人状态计时器###hunter_effect_timers"), 0)) return;
    editing_settings = true;

    if (!compatible) {
        igTextUnformatted(label("Unsupported game build", "当前游戏版本不受支持"), 0);
        return;
    }

    bool enabled = settings.enabled;
    if (igCheckbox(label("Show HUD", "显示倒计时"), &enabled)) {
        settings.enabled = enabled;
        pending_save = true;
    }
    igCheckbox(label("Preview while menu is open", "在菜单中预览"), &preview);
    bool split_columns = settings.split_columns;
    if (igCheckbox(label("Separate buffs and ailments", "增益与异常分栏"), &split_columns)) {
        settings.split_columns = split_columns;
        pending_save = true;
    }
    pending_save |= igSliderInt(label("Horizontal", "横向位置"), &settings.x, 0, 100, "%d%%", 0);
    pending_save |= igSliderInt(label("Top edge", "上沿位置"), &settings.y, 0, 100, "%d%%", 0);
    pending_save |= igSliderInt(label("Scale", "大小"), &settings.scale, 65, 200, "%d%%", 0);
    pending_save |= igSliderInt(label("Row height", "行高"), &settings.row_height, 20, 76, "%d px", 0);
    pending_save |= igSliderInt(label("Background transparency", "背景透明度"), &settings.background_transparency, 0, 100, "%d%%", 0);
    pending_save |= igCombo_Str(label("Order", "排序方式"), &settings.sort,
        label("Shortest first\0Longest first\0Buffs first\0Ailments first\0",
              "剩余时间从短到长\0剩余时间从长到短\0增益优先\0异常优先\0"), -1);
    if (item_tracking) {
        item_filter_menu();
    } else {
        igTextUnformatted(label("Item tracking unavailable", "道具名称识别不可用"), 0);
    }
    filter_menu(BUFF, label("General Buffs", "通用增益效果"));
    filter_menu(AILMENT, label("Ailments", "异常状态"));

    if (igButton(label("Reset defaults", "恢复默认"), (ImVec2){0, 0})) {
        defaults();
        pending_save = true;
    }
    igSameLine(0, -1);
    if (igButton(label("Save settings", "保存设置"), (ImVec2){0, 0})) save_settings();
    if (save_failed) igTextUnformatted(label("Could not save settings", "设置保存失败"), 0);
}

EXPORT void forge_onImGuiFreeRender(void)
{
    const bool menu_open = forge_graphics_isMenuVisible();
    const bool example = menu_open && editing_settings && preview;
    if ((!menu_open || !editing_settings) && pending_save) save_settings();
    editing_settings = false;
    if (!compatible || !settings.enabled) return;
    update_language();

    const uintptr_t hunter = example ? 0 : game_local_hunter();
    if (!hunter && !example) return;

    Row rows[MAX_EFFECTS];
    const unsigned count = collect_rows(rows, hunter, example);
    if (count) draw_hud(rows, count, example);
}
