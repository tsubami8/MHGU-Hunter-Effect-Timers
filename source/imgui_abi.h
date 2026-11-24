#ifndef EFFECT_TIMERS_IMGUI_ABI_H
#define EFFECT_TIMERS_IMGUI_ABI_H

#include <stdbool.h>
#include <stdint.h>

// Subset of the cimgui 1.92.8 interface exported by Forge 2.
typedef struct { float x, y; } ImVec2;
typedef struct ImDrawList ImDrawList;
typedef struct ImFont ImFont;
typedef struct {
    uint32_t id, flags;
    ImVec2 pos, size;
} ImGuiViewportPrefix;

const ImGuiViewportPrefix *igGetMainViewport(void);
ImDrawList *igGetBackgroundDrawList_ViewportPtr(const ImGuiViewportPrefix *viewport);
ImDrawList *igGetForegroundDrawList_Nil(void);
ImFont *igGetFont(void);
float igGetFontSize(void);
ImVec2 igCalcTextSize(const char *text, const char *end, bool hide_after_hash, float wrap_width);
bool ImFont_IsGlyphInFont(ImFont *font, uint16_t codepoint);
void ImDrawList_AddRectFilled(ImDrawList *list, ImVec2 min, ImVec2 max,
                             uint32_t color, float rounding, int flags);
void ImDrawList_AddText_FontPtr(ImDrawList *list, ImFont *font, float size,
                               ImVec2 position, uint32_t color, const char *text,
                               const char *end, float wrap_width, const void *clip);

bool igCheckbox(const char *label, bool *value);
bool igSliderInt(const char *label, int *value, int min, int max,
                 const char *format, int flags);
bool igCombo_Str(const char *label, int *value, const char *items, int popup_height);
bool igCollapsingHeader_TreeNodeFlags(const char *label, int flags);
bool igButton(const char *label, ImVec2 size);
void igSameLine(float offset, float spacing);
void igTextUnformatted(const char *text, const char *end);

typedef void *ImFileHandle;
ImFileHandle igImFileOpen(const char *path, const char *mode);
bool igImFileClose(ImFileHandle file);
uint64_t igImFileRead(void *data, uint64_t size, uint64_t count, ImFileHandle file);
uint64_t igImFileWrite(const void *data, uint64_t size, uint64_t count, ImFileHandle file);

#endif
