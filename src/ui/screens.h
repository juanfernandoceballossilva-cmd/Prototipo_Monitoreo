#ifndef EEZ_LVGL_UI_SCREENS_H
#define EEZ_LVGL_UI_SCREENS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

// Screens

enum ScreensEnum {
    _SCREEN_ID_FIRST = 1,
    SCREEN_ID_MAIN = 1,
    SCREEN_ID_SCREEN_A = 2,
    SCREEN_ID_SCREEN_B = 3,
    SCREEN_ID_SCREEN_C = 4,
    _SCREEN_ID_LAST = 4
};

typedef struct _objects_t {
    lv_obj_t *main;
    lv_obj_t *screen_a;
    lv_obj_t *screen_b;
    lv_obj_t *screen_c;
    lv_obj_t *obj0;
    lv_obj_t *obj1;
    lv_obj_t *obj2;
    lv_obj_t *obj3;
    lv_obj_t *obj4;
    lv_obj_t *obj5;
    lv_obj_t *system_info;
    lv_obj_t *lora_envios_value;
    lv_obj_t *indicador_1;
    lv_obj_t *indicador_2;
    lv_obj_t *obj6;
    lv_obj_t *obj7;
    lv_obj_t *obj8;
    lv_obj_t *obj9;
    lv_obj_t *obj10;
    lv_obj_t *obj11;
    lv_obj_t *ph_value;
    lv_obj_t *fosforo_value;
    lv_obj_t *humedad_value;
    lv_obj_t *nitrogeno_value;
    lv_obj_t *conductividad_value;
    lv_obj_t *potasio_value;
    lv_obj_t *salinidad_value;
    lv_obj_t *tds_value;
    lv_obj_t *temperatura_value;
    lv_obj_t *obj12;
    lv_obj_t *obj13;
    lv_obj_t *obj14;
    lv_obj_t *obj15;
    lv_obj_t *obj16;
    lv_obj_t *obj17;
    lv_obj_t *grafica_a;
    lv_obj_t *btn_ph;
    lv_obj_t *ph_value_2;
    lv_obj_t *obj18;
    lv_obj_t *btn_fosforo;
    lv_obj_t *fosforo_value_2;
    lv_obj_t *obj19;
    lv_obj_t *btn_humedad;
    lv_obj_t *humedad_value_2;
    lv_obj_t *obj20;
    lv_obj_t *btn_nitrogeno;
    lv_obj_t *nitrogeno_value_2;
    lv_obj_t *obj21;
    lv_obj_t *btn_conductividad;
    lv_obj_t *obj22;
    lv_obj_t *conductividad_value_2;
    lv_obj_t *btn_potasio;
    lv_obj_t *obj23;
    lv_obj_t *potasio_value_2;
    lv_obj_t *btn_salinidad;
    lv_obj_t *obj24;
    lv_obj_t *salinidad_value_2;
    lv_obj_t *btn_tds;
    lv_obj_t *obj25;
    lv_obj_t *tds_value_2;
    lv_obj_t *btn_temperatura;
    lv_obj_t *temperatura_value_2;
    lv_obj_t *obj26;
    lv_obj_t *titulo_grafica;
    lv_obj_t *escala;
    lv_obj_t *obj27;
    lv_obj_t *obj28;
    lv_obj_t *obj29;
    lv_obj_t *obj30;
    lv_obj_t *obj31;
    lv_obj_t *obj32;
    lv_obj_t *switch_value;
    lv_obj_t *indicador_5_2;
    lv_obj_t *obj33;
} objects_t;

extern objects_t objects;

void create_screen_main();
void tick_screen_main();

void create_screen_screen_a();
void tick_screen_screen_a();

void create_screen_screen_b();
void tick_screen_screen_b();

void create_screen_screen_c();
void tick_screen_screen_c();

void create_user_widget_line_chart(lv_obj_t *parent_obj, void *flowState, int startWidgetIndex);
void tick_user_widget_line_chart(void *flowState, int startWidgetIndex);

void tick_screen_by_id(enum ScreensEnum screenId);
void tick_screen(int screen_index);

void create_screens();

// Groups

typedef struct _groups_t {
    lv_group_t *Grafica_A;
} groups_t;

extern groups_t groups;

void ui_create_groups();

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_SCREENS_H*/