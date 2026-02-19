const lo = @import("lofi.zig");

const CAM_OFF_Y: f32 = 7.0;
const CAM_OFF_Z: f32 = -7.0;

const IDLE: i32 = 0;
const RUNNING: i32 = 1;
const WALKING: i32 = 2;

var model: lo.Model = .{};
var cube_model: lo.Model = .{};
var floor_model: lo.Model = .{};
var anims: lo.AnimSet = .{};
var tex_body: lo.Texture = .{};
var tex_head: lo.Texture = .{};
var tex_checker: lo.Texture = .{};
var snd: lo.Sound = .{};

var player_ent: lo.Entity = .{};
var cube_ent: lo.Entity = .{};
var floor_ent: lo.Entity = .{};

var key_w: bool = false;
var key_a: bool = false;
var key_s: bool = false;
var key_d: bool = false;
var key_shift: bool = false;
var player_state: i32 = IDLE;

//---------------------------------------------------------------------------

fn lenv3(v: [3]f32) f32 {
    return @sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

fn normv3(v: [3]f32) [3]f32 {
    const len = lenv3(v);
    if (len > 0.0) return .{ v[0] / len, v[1] / len, v[2] / len };
    return v;
}

fn nlerpq(a: [4]f32, b: [4]f32, t: f32) [4]f32 {
    const s: f32 = if ((a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]) >= 0.0) 1.0 else -1.0;
    const x = a[0] + t * (s * b[0] - a[0]);
    const y = a[1] + t * (s * b[1] - a[1]);
    const z = a[2] + t * (s * b[2] - a[2]);
    const w = a[3] + t * (s * b[3] - a[3]);
    const len = @sqrt(x * x + y * y + z * z + w * w);
    return .{ x / len, y / len, z / len, w / len };
}

fn player_update(dt: f32) void {
    var pos: [3]f32 = undefined;
    lo.getPosition(player_ent, &pos);

    var move: [3]f32 = .{ 0.0, 0.0, 0.0 };
    if (key_w) move[2] += 1.0;
    if (key_s) move[2] -= 1.0;
    if (key_a) move[0] += 1.0;
    if (key_d) move[0] -= 1.0;

    const is_moving = move[0] != 0.0 or move[2] != 0.0;

    const new_state: i32 = if (is_moving) (if (key_shift) RUNNING else WALKING) else IDLE;
    if (new_state != player_state) {
        player_state = new_state;
        var anim_desc = lo.AnimDesc{
            .set = anims,
            .anim = player_state,
            .flags = lo.ANIM_LOOP | lo.ANIM_PLAY,
        };
        lo.setAnims(player_ent, &anim_desc);
    }

    if (is_moving) {
        move = normv3(move);
        const speed: f32 = if (player_state == RUNNING) 3.75 else 1.75;
        pos[0] += move[0] * speed * dt;
        pos[2] += move[2] * speed * dt;
        lo.setPosition(player_ent, &pos);

        var qy = move[0];
        var qw = 1.0 + move[2];
        const len = lenv3(.{ qy, qw, 0.0 });
        if (len > 0.001) {
            qy /= len;
            qw /= len;
        } else {
            qy = 1.0;
            qw = 0.0;
        }
        const target: [4]f32 = .{ 0.0, qy, 0.0, qw };
        var cur: [4]f32 = undefined;
        lo.getRotation(player_ent, &cur);
        const blend: f32 = if (dt * 10.0 > 1.0) 1.0 else dt * 10.0;
        var rot = nlerpq(cur, target, blend);
        lo.setRotation(player_ent, &rot);
    }

    var cam_target: [3]f32 = .{ pos[0], pos[1] + 1.5, pos[2] };
    var cam_pos: [3]f32 = .{ cam_target[0], cam_target[1] + CAM_OFF_Y, cam_target[2] + CAM_OFF_Z };
    lo.setCamPos(&cam_pos);
    lo.setCamTarget(&cam_target);
}

//----------------------------------------------------------------------

export fn lo_init() void {
    model = lo.loadModel("assets/game_base.iqm");
    anims = lo.loadAnims("assets/game_base.iqm");
    tex_body = lo.loadTexture("assets/skin_body.webp");
    tex_head = lo.loadTexture("assets/skin_head.webp");
    tex_checker = lo.loadTexture("assets/floor.webp");
    floor_model = lo.loadModel("assets/plane.iqm");
    cube_model = lo.loadModel("assets/cube.iqm");
    snd = lo.loadSound("assets/loop.ogg");

    //player
    player_ent = lo.create();
    lo.setModel(player_ent, model);
    lo.setTexture(player_ent, tex_body, 0);
    lo.setTexture(player_ent, tex_head, 1);
    var player_anim = lo.AnimDesc{ .set = anims, .anim = IDLE, .flags = lo.ANIM_LOOP | lo.ANIM_PLAY };
    lo.setAnims(player_ent, &player_anim);
    const player_body = lo.createAnimBody();
    var player_geom = lo.GeomDesc{
        .typ = lo.GEOM_CYLINDER,
        .pos = .{ 0, 0.75, 0 },
        .rot = .{ 0, 0, 0, 1 },
        .size = .{ 0.75, 1.0, 0 },
    };
    lo.abAddGeom(player_body, &player_geom);
    lo.setAnimBody(player_ent, player_body);

    //floor
    floor_ent = lo.create();
    lo.setModel(floor_ent, floor_model);
    lo.setTexture(floor_ent, tex_checker, 0);
    var floor_scale: [3]f32 = .{ 10, 10, 10 };
    lo.setScale(floor_ent, &floor_scale);
    const floor_body = lo.createAnimBody();
    var floor_geom = lo.GeomDesc{
        .typ = lo.GEOM_BOX,
        .pos = .{ 0, 0, 0 },
        .rot = .{ 0, 0, 0, 1 },
        .size = .{ 40, 0.1, 40 },
    };
    lo.abAddGeom(floor_body, &floor_geom);
    lo.setAnimBody(floor_ent, floor_body);

    //cube
    cube_ent = lo.create();
    lo.setModel(cube_ent, cube_model);
    lo.setTexture(cube_ent, tex_checker, 0);
    var cube_scale: [3]f32 = .{ 0.5, 0.5, 0.5 };
    lo.setScale(cube_ent, &cube_scale);
    const rb = lo.createRigidBody();
    var rb_pos: [3]f32 = .{ 0, 5, 0 };
    lo.rbSetPos(rb, &rb_pos);
    lo.rbSetMass(rb, 3.25);
    var rb_geom = lo.GeomDesc{
        .typ = lo.GEOM_BOX,
        .pos = .{ 0, 0, 0 },
        .rot = .{ 0, 0, 0, 1 },
        .size = .{ 1.1, 1.1, 1.1 },
    };
    lo.rbAddGeom(rb, &rb_geom);
    lo.setRigidBody(cube_ent, rb);
    var sound_desc = lo.SoundDesc{
        .sound = snd,
        .vol = 0.75,
        .min_range = 0.1,
        .max_range = 100.0,
        .flags = lo.SOUND_SPATIAL,
    };
    lo.setSound(cube_ent, &sound_desc);
}

export fn lo_frame(dt: f32) void {
    player_update(dt);
    lo.dtxCanvas(800.0 * 0.5, 600.0 * 0.5);
    lo.dtxOrigin(1.0, 1.0);
    lo.dtxColor3b(255, 255, 255);
    lo.dtxPuts("WASD: move\nShift: run\nSpace: play sound");
}

export fn lo_cleanup() void {
    lo.releaseModel(model);
    lo.releaseModel(cube_model);
    lo.releaseModel(floor_model);
    lo.releaseTexture(tex_body);
    lo.releaseTexture(tex_head);
    lo.releaseTexture(tex_checker);
    lo.releaseAnims();
    lo.releaseSound(snd);
}

export fn lo_mouse_pos(_: f32, _: f32) void {}

export fn lo_mouse_button(_: i32, _: bool) void {}

export fn lo_key(keycode: i32, down: bool, repeat: bool) void {
    if (repeat) return;
    switch (keycode) {
        87 => key_w = down,
        65 => key_a = down,
        83 => key_s = down,
        68 => key_d = down,
        340 => key_shift = down,
        344 => key_shift = down,
        32 => if (down) lo.playSound(cube_ent),
        else => {},
    }
}
