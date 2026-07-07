#ifndef EEZ_LVGL_UI_STYLES_H
#define EEZ_LVGL_UI_STYLES_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Style: active_tab
lv_style_t *get_style_active_tab_MAIN_DEFAULT();
void add_style_active_tab(lv_obj_t *obj);
void remove_style_active_tab(lv_obj_t *obj);

// Style: inactive_tab
lv_style_t *get_style_inactive_tab_MAIN_DEFAULT();
void add_style_inactive_tab(lv_obj_t *obj);
void remove_style_inactive_tab(lv_obj_t *obj);

// Style: tab_panel
lv_style_t *get_style_tab_panel_MAIN_DEFAULT();
void add_style_tab_panel(lv_obj_t *obj);
void remove_style_tab_panel(lv_obj_t *obj);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_STYLES_H*/