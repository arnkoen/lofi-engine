#include "lofi.h"

#define ASSET "assets/"

//For this example we don't use c stdlib.
//If you need it, you can compile using wasi-sdk.
static float lenv3(float v[3]) {
    return __builtin_sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
}

static void normv3(float v[3]) {
    float len = lenv3(v);
    if (len > 0.0f) { v[0] /= len; v[1] /= len; v[2] /= len; }
}

static void nlerpq(float out[4], float a[4], float b[4], float t) {
    float s = (a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3]) < 0.0f ? -1.0f : 1.0f;
    float x = a[0] + t*(s*b[0] - a[0]);
    float y = a[1] + t*(s*b[1] - a[1]);
    float z = a[2] + t*(s*b[2] - a[2]);
    float w = a[3] + t*(s*b[3] - a[3]);
    float len = __builtin_sqrtf(x*x + y*y + z*z + w*w);
    out[0] = x/len; out[1] = y/len; out[2] = z/len; out[3] = w/len;
}

#define CAM_OFF_Y  7.0f
#define CAM_OFF_Z -7.0f

static lo_Model   model, cube_model, floor_model;
static lo_AnimSet anims;
static lo_Texture tex_body, tex_head, tex_checker;
static lo_Sound   snd;

static lo_Entity player_ent, cube_ent, floor_ent;

static bool key_w, key_a, key_s, key_d, key_shift;

enum { IDLE = 0, RUNNING = 1, WALKING = 2 };
static int player_state = IDLE;

//---------------------------------------------------------------------------

static void player_update(float dt) {
    float pos[3];
    lo_get_position(player_ent, pos);

    float move[3] = {0.0f, 0.0f, 0.0f};
    if (key_w) move[2] += 1.0f;
    if (key_s) move[2] -= 1.0f;
    if (key_a) move[0] += 1.0f;
    if (key_d) move[0] -= 1.0f;

    bool is_moving = (move[0] != 0.0f || move[2] != 0.0f);

    int new_state = is_moving ? (key_shift ? RUNNING : WALKING) : IDLE;
    if (new_state != player_state) {
        player_state = new_state;
        lo_set_anims(player_ent, &(lo_AnimDesc){
            .set   = anims,
            .anim  = player_state,
            .flags = ANIM_LOOP | ANIM_PLAY,
        });
    }

    if (is_moving) {
        normv3(move);
        float speed = (player_state == RUNNING) ? 3.75f : 1.75f;
        pos[0] += move[0] * speed * dt;
        pos[2] += move[2] * speed * dt;
        lo_set_position(player_ent, pos);

        //y-axis quaternion from +Z toward move direction
        float qy = move[0], qw = 1.0f + move[2];
        float tmp[3] = {qy, qw, 0.0f};
        float len = lenv3(tmp);
        if (len > 0.001f) { qy /= len; qw /= len; }
        else              { qy = 1.0f; qw = 0.0f; }  // 180 degrees
        float target[4] = {0.0f, qy, 0.0f, qw};

        float cur[4], rot[4];
        lo_get_rotation(player_ent, cur);
        float blend = dt * 10.0f;
        if (blend > 1.0f) blend = 1.0f;
        nlerpq(rot, cur, target, blend);
        lo_set_rotation(player_ent, rot);
    }

    float cam_target[3] = {pos[0], pos[1] + 1.5f, pos[2]};
    float cam_pos[3]    = {cam_target[0], cam_target[1] + CAM_OFF_Y, cam_target[2] + CAM_OFF_Z};
    lo_set_campos(cam_pos);
    lo_set_cam_target(cam_target);
}

//-------------------------------------------------------------

void lo_init(void) {
    model       = lo_load_model(ASSET"game_base.iqm");
    anims       = lo_load_anims(ASSET"game_base.iqm");
    tex_body    = lo_load_texture(ASSET"skin_body.dds");
    tex_head    = lo_load_texture(ASSET"skin_head.dds");
    tex_checker = lo_load_texture(ASSET"floor.dds");
    floor_model = lo_load_model(ASSET"plane.iqm");
    cube_model  = lo_load_model(ASSET"cube.iqm");
    snd         = lo_load_sound(ASSET"loop.ogg");

    //player
    player_ent = lo_create();
    lo_set_model(player_ent, model);
    lo_set_texture(player_ent, tex_body, 0);
    lo_set_texture(player_ent, tex_head, 1);
    lo_set_anims(player_ent, &(lo_AnimDesc){
        .set   = anims,
        .anim  = IDLE,
        .flags = ANIM_LOOP | ANIM_PLAY,
    });
    lo_AnimBody player_body = lo_create_anim_body();
    lo_ab_add_geom(player_body, &(lo_GeomDesc){
        .type = LO_GEOM_CYLINDER,
        .pos  = {0, 0.75f, 0},
        .rot  = {0, 0, 0, 1},
        .size = {0.75f, 1.0f},
    });
    lo_set_anim_body(player_ent, player_body);

    //floor
    floor_ent = lo_create();
    lo_set_model(floor_ent, floor_model);
    lo_set_texture(floor_ent, tex_checker, 0);
    lo_set_scale(floor_ent, (float[]){10, 10, 10});
    lo_AnimBody floor_body = lo_create_anim_body();
    lo_ab_add_geom(floor_body, &(lo_GeomDesc){
        .type = LO_GEOM_BOX,
        .rot  = {0, 0, 0, 1},
        .size = {40, 0.1f, 40},
    });
    lo_set_anim_body(floor_ent, floor_body);

    //cube
    cube_ent = lo_create();
    lo_set_model(cube_ent, cube_model);
    lo_set_texture(cube_ent, tex_checker, 0);
    lo_set_scale(cube_ent, (float[]){0.5f, 0.5f, 0.5f});
    lo_RigidBody rb = lo_create_rigid_body();
    lo_rb_set_pos(rb, (float[]){0, 5, 0});
    lo_rb_set_mass(rb, 3.25f);
    lo_rb_add_geom(rb, &(lo_GeomDesc){
        .type = LO_GEOM_BOX,
        .rot  = {0, 0, 0, 1},
        .size = {1.1f, 1.1f, 1.1f},
    });
    lo_set_rigid_body(cube_ent, rb);
    lo_set_sound(cube_ent, &(lo_SoundDesc){
        .sound     = snd,
        .vol       = 0.75f,
        .min_range = 0.1f,
        .max_range = 100.0f,
        .flags     = SOUND_SPATIAL,
    });
}

void lo_frame(float dt) {
    player_update(dt);

    lo_dtx_canvas(800.0f * 0.5f, 600.0f * 0.5f);
    lo_dtx_origin(1.0f, 1.0f);
    lo_dtx_color3b(255, 255, 255);
    lo_dtx_puts("WASD: move\nShift: run\nSpace: play sound");
}

void lo_cleanup(void) {
    lo_release_model(model);
    lo_release_model(cube_model);
    lo_release_model(floor_model);
    lo_release_texture(tex_body);
    lo_release_texture(tex_head);
    lo_release_texture(tex_checker);
    lo_release_anims();
    lo_release_sound(snd);
}

void lo_mouse_pos(float dx, float dy) {
    (void)dx; (void)dy;
}

void lo_mouse_button(int button, bool down) {
    (void)button; (void)down;
}

void lo_key(int keycode, bool down, bool repeat) {
    if (repeat) return;
    switch (keycode) {
        case 87:  key_w     = down; break;  // W
        case 65:  key_a     = down; break;  // A
        case 83:  key_s     = down; break;  // S
        case 68:  key_d     = down; break;  // D
        case 340: key_shift = down; break;  // Left Shift
        case 344: key_shift = down; break;  // Right Shift
        case 32:  if (down) lo_play_sound(cube_ent); break; // Space
    }
}
