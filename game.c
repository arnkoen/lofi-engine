#include "lofi.h"


#define ASSET "assets/"
lo_Entity ent = {0};
lo_Model model = {0};
lo_Texture tex_body = {0};
lo_Texture tex_head = {0};
lo_Sound snd = {0};
float time = 0;

void lo_init() {
    ent = lo_create();
    model = lo_load_model(ASSET"game_base.iqm");
    lo_AnimSet anims = lo_load_anims(ASSET"game_base.iqm");
    tex_body = lo_load_texture(ASSET"skin_body.dds");
    tex_head = lo_load_texture(ASSET"skin_head.dds");
    lo_set_model(ent, model);
    lo_set_texture(ent, tex_body, 0);
    lo_set_texture(ent, tex_head, 1);
    lo_set_anims(ent, &(lo_AnimDesc) {
        .set = anims,
        .anim = 1,
        .flags = ANIM_LOOP | ANIM_PLAY,
    });
    snd = lo_load_sound(ASSET"loop.ogg");
    lo_set_sound(ent, &(lo_SoundDesc) {
        .sound = snd,
        .vol = 0.75f,
        .min_range = 0.1f,
        .max_range = 100.f,
        .flags = SOUND_PLAY | SOUND_LOOP | SOUND_SPATIAL,
    });
}

void lo_frame(float dt) {
    time += dt;
    float radius = 4.0f;
    float speed = 0.5f;
    float angle = time * speed;
    lo_set_campos((float[]){
        lo_sinf(angle) * radius,
        1.5f,
        lo_cosf(angle) * radius
    });
    lo_set_cam_target((float[]){ 0, 0.75f, 0 });

    lo_dtx_canvas(800.0f * 0.5f, 600.0f * 0.5f);
    lo_dtx_origin(1.0f, 1.0f);
    lo_dtx_color3b(255, 255, 255);
    lo_dtx_puts("Hello Lofi\n");
}

void lo_cleanup() {
    lo_destroy(ent);
    lo_release_model(model);
    lo_release_texture(tex_body);
    lo_release_texture(tex_head);
    lo_release_anims();
    lo_release_sound(snd);
}

void lo_key(int keycode, bool down, bool repeat) {
    lo_dtx_origin(1.0f, 2.0f);
    if (down) {
        lo_dtx_puts("down\n");
    }
    if (repeat) {
        lo_dtx_puts("repeat\n");
    }
}
