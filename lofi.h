#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __wasm__
#define EXPORT(name) __attribute__((export_name(#name)))
#define IMPORT(name) __attribute__((import_module("env"), import_name(#name)))
#else
#define EXPORT(name)
#define IMPORT(name)
#endif

#define ANIM_PLAY (1 << 0)
#define ANIM_LOOP (1 << 1)

#define SOUND_PLAY    (1U << 1)
#define SOUND_LOOP    (1U << 2)
#define SOUND_SPATIAL (1U << 3)

typedef struct lo_Texture { uint32_t id; } lo_Texture;
IMPORT(lo_load_texture) lo_Texture lo_load_texture(const char* path);
IMPORT(lo_release_texture) void lo_release_texture(lo_Texture tex);

typedef struct lo_Model { uint32_t id; } lo_Model;
IMPORT(lo_load_model) lo_Model lo_load_model(const char* path);
IMPORT(lo_release_model) void lo_release_model(lo_Model model);

typedef struct lo_AnimSet { uint32_t id; } lo_AnimSet;
IMPORT(lo_load_anims) lo_AnimSet lo_load_anims(const char* path);
IMPORT(lo_release_anims) void lo_release_anims(void);

typedef struct lo_Sound { uint32_t id; } lo_Sound;
IMPORT(lo_load_sound) lo_Sound lo_load_sound(const char* path);
IMPORT(lo_release_sound) void lo_release_sound(lo_Sound sound);

typedef struct lo_Entity { uint32_t id; } lo_Entity;

IMPORT(lo_create) lo_Entity lo_create();
IMPORT(lo_valid) bool lo_valid(lo_Entity entity);
IMPORT(lo_destroy) void lo_destroy(lo_Entity entity);

IMPORT(lo_set_position) void lo_set_position(lo_Entity e, float pos[3]);
IMPORT(lo_get_position) void lo_get_position(lo_Entity e, float out[3]);
IMPORT(lo_set_rotation) void lo_set_rotation(lo_Entity e, float rot[4]);
IMPORT(lo_get_rotation) void lo_get_rotation(lo_Entity e, float out[4]);
IMPORT(lo_set_scale) void lo_set_scale(lo_Entity e, float scale[3]);
IMPORT(lo_get_scale) void lo_get_scale(lo_Entity e, float out[3]);

IMPORT(lo_set_parent) void lo_set_parent(lo_Entity entity, lo_Entity parent);
IMPORT(lo_remove_parent) void lo_remove_parent(lo_Entity entity);
IMPORT(lo_add_child) void lo_add_child(lo_Entity entity, lo_Entity child);
IMPORT(lo_set_children) void lo_set_children(lo_Entity e, lo_Entity* children);
IMPORT(lo_clear_children) void lo_clear_children(lo_Entity e);

IMPORT(lo_set_model) void lo_set_model(lo_Entity entity, lo_Model model);
IMPORT(lo_clear_model) void lo_clear_model(lo_Entity entity);
IMPORT(lo_set_texture) void lo_set_texture(lo_Entity entity, lo_Texture handle, int slot);
IMPORT(lo_clear_textures) void lo_clear_textures(lo_Entity entity);

typedef struct lo_AnimDesc { lo_AnimSet set; int32_t flags; int32_t anim; } lo_AnimDesc;
IMPORT(lo_set_anims) void lo_set_anims(lo_Entity entity, lo_AnimDesc* desc);
IMPORT(lo_clear_anims) void lo_clear_anims(lo_Entity entity);

typedef struct lo_SoundDesc { lo_Sound sound; float vol; float min_range; float max_range; uint32_t flags; } lo_SoundDesc;
IMPORT(lo_set_sound) void lo_set_sound(lo_Entity e, lo_SoundDesc* desc);
IMPORT(lo_play_sound) void lo_play_sound(lo_Entity e);
IMPORT(lo_stop_sound) void lo_stop_sound(lo_Entity e);
IMPORT(lo_clear_sound) void lo_clear_sound(lo_Entity e);

IMPORT(lo_sinf) float lo_sinf(float x);
IMPORT(lo_cosf) float lo_cosf(float x);

IMPORT(lo_set_campos) void lo_set_campos(float pos[3]);
IMPORT(lo_set_cam_target) void lo_set_cam_target(float target[3]);

IMPORT(lo_dtx_layer) void lo_dtx_layer(int layer_id);
IMPORT(lo_dtx_font) void lo_dtx_font(int font_index);
IMPORT(lo_dtx_canvas) void lo_dtx_canvas(float w, float h);
IMPORT(lo_dtx_origin) void lo_dtx_origin(float x, float y);
IMPORT(lo_dtx_home) void lo_dtx_home(void);
IMPORT(lo_dtx_pos) void lo_dtx_pos(float x, float y);
IMPORT(lo_dtx_pos_x) void lo_dtx_pos_x(float x);
IMPORT(lo_dtx_pos_y) void lo_dtx_pos_y(float y);
IMPORT(lo_dtx_move) void lo_dtx_move(float dx, float dy);
IMPORT(lo_dtx_move_x) void lo_dtx_move_x(float dx);
IMPORT(lo_dtx_move_y) void lo_dtx_move_y(float dy);
IMPORT(lo_dtx_crlf) void lo_dtx_crlf(void);
IMPORT(lo_dtx_color3b) void lo_dtx_color3b(uint8_t r, uint8_t g, uint8_t b);
IMPORT(lo_dtx_color3f) void lo_dtx_color3f(float r, float g, float b);
IMPORT(lo_dtx_color4f) void lo_dtx_color4f(float rgba[4]);
IMPORT(lo_dtx_color1i) void lo_dtx_color1i(uint32_t rgba);
IMPORT(lo_dtx_putc) void lo_dtx_putc(char c);
IMPORT(lo_dtx_puts) void lo_dtx_puts(const char* str);
IMPORT(lo_dtx_putr) void lo_dtx_putr(const char* str, int len);

EXPORT(lo_init) void lo_init();
EXPORT(lo_frame) void lo_frame(float dt);
EXPORT(lo_cleanup) void lo_cleanup();

EXPORT(lo_mouse_pos) void lo_mouse_pos(float x, float y);
EXPORT(lo_mouse_button) void lo_mouse_button(int button, bool down);
EXPORT(lo_key) void lo_key(int keycode, bool down, bool repeat);
