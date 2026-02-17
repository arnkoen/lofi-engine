const lo = @import("lofi.zig");

var ent: lo.Entity = .{};
var time: f32 = 0;

export fn lo_init() void {
    ent = lo.create();
    const model = lo.loadModel("assets/game_base.iqm");
    const anims = lo.loadAnims("assets/game_base.iqm");
    const tex_body = lo.loadTexture("assets/skin_body.dds");
    const tex_head = lo.loadTexture("assets/skin_head.dds");
    lo.setModel(ent, model);
    lo.setTexture(ent, tex_body, 0);
    lo.setTexture(ent, tex_head, 1);
    lo.setAnims(ent, &lo.AnimDesc{
        .set = anims,
        .anim = 0,
        .flags = lo.ANIM_LOOP | lo.ANIM_PLAY,
    });
    const snd = lo.loadSound("assets/loop.ogg");
    lo.setSound(ent, &lo.SoundDesc{
        .sound = snd,
        .vol = 0.75,
        .min_range = 0.1,
        .max_range = 100.0,
        .flags = lo.SOUND_PLAY | lo.SOUND_LOOP,
    });
}

export fn lo_frame(dt: f32) void {
    time += dt;
    const radius: f32 = 4.0;
    const speed: f32 = 0.5;
    const angle = time * speed;
    lo.setCamPos(&[3]f32{ lo.sinf(angle) * radius, 1.5, lo.cosf(angle) * radius });
    lo.setCamTarget(&[3]f32{ 0, 0.75, 0 });

    lo.dtxCanvas(800.0, 600.0);
    lo.dtxOrigin(1.0, 1.0);
    lo.dtxColor3b(255, 255, 255);
    lo.dtxPuts("Hello Lofi");
}

export fn lo_cleanup() void {
    lo.destroy(ent);
}
