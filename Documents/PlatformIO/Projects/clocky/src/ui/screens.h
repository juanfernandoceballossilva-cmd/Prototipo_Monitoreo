#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _groups_t {
    lv_group_t *Grafica_A;
} groups_t;

extern groups_t groups;

void ui_create_groups();

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *screen_a;
    lv_obj_t *screen_b;
    lv_obj_t *screen_c;
    lv_obj_t *screen_d;
    lv_obj_t *screen_e;
    lv_obj_t *screen_f;
    lv_obj_t *screen_g;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *grafica_a;
} objects_t;

extern objects_t objects;

enum ScreensEnum {
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SCREEN_A = 2,
    SCREEN_ID_SCREEN_B = 3,
    SCREEN_ID_SCREEN_C = 4,
    SCREEN_ID_SCREEN_D = 5,
    SCREEN_ID_SCREEN_E = 6,
    SCREEN_ID_SCREEN_F = 7,
    SCREEN_ID_SCREEN_G = 8,
};

void create_screen_main();
void tick_screen_main();

void create_screen_screen_a();
void tick_screen_screen_a();

void create_screen_screen_b();
void tick_screen_screen_b();

void create_screen_screen_c();
void tick_screen_screen_c();

void create_screen_screen_d();
void tick_screen_screen_d();

void create_screen_screen_e();
void tick_screen_screen_e();

void create_screen_screen_f();
void tick_screen_screen_f();

void create_screen_screen_g();
void tick_screen_screen_g();

void create_user_widget_line_chart(lv_obj_t *parent_obj, void *flowState, int startWidgetIndex);
void tick_user_widget_line_chart(void *flowState, int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();


#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/