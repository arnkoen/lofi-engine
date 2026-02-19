pub const Entity = packed struct { id: u32 = 0 };
pub const RigidBody = packed struct { ptr: u64 = 0 };
pub const AnimBody = packed struct { ptr: u64 = 0 };
pub const Texture = packed struct { id: u32 = 0 };
pub const Model = packed struct { id: u32 = 0 };
pub const AnimSet = packed struct { id: u32 = 0 };
pub const Sound = packed struct { id: u32 = 0 };

pub const GEOM_BOX: i32 = 0;
pub const GEOM_SPHERE: i32 = 1;
pub const GEOM_CYLINDER: i32 = 2;

pub const GeomDesc = extern struct {
    typ: i32,
    pos: [3]f32,
    rot: [4]f32,
    size: [3]f32, // box: w,h,d; sphere: diameter; cylinder: diameter, height
};

pub const ANIM_PLAY: i32 = 1 << 0;
pub const ANIM_LOOP: i32 = 1 << 1;

pub const SOUND_PLAY: u32 = 1 << 1;
pub const SOUND_LOOP: u32 = 1 << 2;
pub const SOUND_SPATIAL: u32 = 1 << 3;

pub const AnimDesc = extern struct {
    set: AnimSet,
    flags: i32,
    anim: i32,
};

pub const SoundDesc = extern struct {
    sound: Sound,
    vol: f32,
    min_range: f32,
    max_range: f32,
    flags: u32,
};

const env = struct {
    extern "env" fn lo_create() Entity;
    extern "env" fn lo_valid(entity: Entity) bool;
    extern "env" fn lo_destroy(entity: Entity) void;

    extern "env" fn lo_load_texture(path: [*:0]const u8) Texture;
    extern "env" fn lo_release_texture(tex: Texture) void;
    extern "env" fn lo_load_model(path: [*:0]const u8) Model;
    extern "env" fn lo_release_model(model: Model) void;
    extern "env" fn lo_load_anims(path: [*:0]const u8) AnimSet;
    extern "env" fn lo_release_anims() void;
    extern "env" fn lo_load_sound(path: [*:0]const u8) Sound;
    extern "env" fn lo_release_sound(sound: Sound) void;

    extern "env" fn lo_set_position(e: Entity, pos: [*]const f32) void;
    extern "env" fn lo_get_position(e: Entity, out: [*]f32) void;
    extern "env" fn lo_set_rotation(e: Entity, rot: [*]const f32) void;
    extern "env" fn lo_get_rotation(e: Entity, out: [*]f32) void;
    extern "env" fn lo_set_scale(e: Entity, scale: [*]const f32) void;
    extern "env" fn lo_get_scale(e: Entity, out: [*]f32) void;

    extern "env" fn lo_set_parent(entity: Entity, parent: Entity) void;
    extern "env" fn lo_remove_parent(entity: Entity) void;
    extern "env" fn lo_add_child(entity: Entity, child: Entity) void;
    extern "env" fn lo_set_children(e: Entity, children: [*]const Entity) void;
    extern "env" fn lo_clear_children(e: Entity) void;

    extern "env" fn lo_set_model(entity: Entity, model: Model) void;
    extern "env" fn lo_clear_model(entity: Entity) void;
    extern "env" fn lo_set_texture(entity: Entity, handle: Texture, slot: i32) void;
    extern "env" fn lo_clear_textures(entity: Entity) void;

    extern "env" fn lo_set_anims(entity: Entity, desc: *const AnimDesc) void;
    extern "env" fn lo_clear_anims(entity: Entity) void;

    extern "env" fn lo_set_sound(e: Entity, desc: *const SoundDesc) void;
    extern "env" fn lo_play_sound(e: Entity) void;
    extern "env" fn lo_stop_sound(e: Entity) void;
    extern "env" fn lo_clear_sound(e: Entity) void;

    extern "env" fn lo_create_rigid_body() RigidBody;
    extern "env" fn lo_free_rigid_body(body: RigidBody) void;
    extern "env" fn lo_set_rigid_body(e: Entity, body: RigidBody) void;
    extern "env" fn lo_clear_rigid_body(e: Entity) void;
    extern "env" fn lo_create_anim_body() AnimBody;
    extern "env" fn lo_free_anim_body(body: AnimBody) void;
    extern "env" fn lo_set_anim_body(e: Entity, body: AnimBody) void;
    extern "env" fn lo_clear_anim_body(e: Entity) void;

    extern "env" fn lo_rb_set_pos(body: RigidBody, pos: [*]const f32) void;
    extern "env" fn lo_rb_set_rot(body: RigidBody, rot: [*]const f32) void;
    extern "env" fn lo_ab_set_pos(body: AnimBody, pos: [*]const f32) void;
    extern "env" fn lo_ab_set_rot(body: AnimBody, rot: [*]const f32) void;

    extern "env" fn lo_rb_set_mass(body: RigidBody, mass: f32) void;
    extern "env" fn lo_rb_add_geom(body: RigidBody, desc: *const GeomDesc) void;
    extern "env" fn lo_ab_add_geom(body: AnimBody, desc: *const GeomDesc) void;

    extern "env" fn lo_set_campos(pos: [*]const f32) void;
    extern "env" fn lo_set_cam_target(target: [*]const f32) void;
    extern "env" fn lo_lock_mouse(lock: bool) void;

    extern "env" fn lo_dtx_layer(layer_id: i32) void;
    extern "env" fn lo_dtx_font(font_index: i32) void;
    extern "env" fn lo_dtx_canvas(w: f32, h: f32) void;
    extern "env" fn lo_dtx_origin(x: f32, y: f32) void;
    extern "env" fn lo_dtx_home() void;
    extern "env" fn lo_dtx_pos(x: f32, y: f32) void;
    extern "env" fn lo_dtx_pos_x(x: f32) void;
    extern "env" fn lo_dtx_pos_y(y: f32) void;
    extern "env" fn lo_dtx_move(dx: f32, dy: f32) void;
    extern "env" fn lo_dtx_move_x(dx: f32) void;
    extern "env" fn lo_dtx_move_y(dy: f32) void;
    extern "env" fn lo_dtx_crlf() void;
    extern "env" fn lo_dtx_color3b(r: u8, g: u8, b: u8) void;
    extern "env" fn lo_dtx_color3f(r: f32, g: f32, b: f32) void;
    extern "env" fn lo_dtx_color4f(rgba: [*]const f32) void;
    extern "env" fn lo_dtx_color1i(rgba: u32) void;
    extern "env" fn lo_dtx_putc(c: u8) void;
    extern "env" fn lo_dtx_puts(str: [*:0]const u8) void;
    extern "env" fn lo_dtx_putr(str: [*]const u8, len: i32) void;
};

pub const create = env.lo_create;
pub const valid = env.lo_valid;
pub const destroy = env.lo_destroy;

pub const loadTexture = env.lo_load_texture;
pub const releaseTexture = env.lo_release_texture;
pub const loadModel = env.lo_load_model;
pub const releaseModel = env.lo_release_model;
pub const loadAnims = env.lo_load_anims;
pub const releaseAnims = env.lo_release_anims;
pub const loadSound = env.lo_load_sound;
pub const releaseSound = env.lo_release_sound;

pub const setPosition = env.lo_set_position;
pub const getPosition = env.lo_get_position;
pub const setRotation = env.lo_set_rotation;
pub const getRotation = env.lo_get_rotation;
pub const setScale = env.lo_set_scale;
pub const getScale = env.lo_get_scale;

pub const setParent = env.lo_set_parent;
pub const removeParent = env.lo_remove_parent;
pub const addChild = env.lo_add_child;
pub const setChildren = env.lo_set_children;
pub const clearChildren = env.lo_clear_children;

pub const setModel = env.lo_set_model;
pub const clearModel = env.lo_clear_model;
pub const setTexture = env.lo_set_texture;
pub const clearTextures = env.lo_clear_textures;

pub const setAnims = env.lo_set_anims;
pub const clearAnims = env.lo_clear_anims;

pub const setSound = env.lo_set_sound;
pub const playSound = env.lo_play_sound;
pub const stopSound = env.lo_stop_sound;
pub const clearSound = env.lo_clear_sound;

pub const createRigidBody = env.lo_create_rigid_body;
pub const freeRigidBody = env.lo_free_rigid_body;
pub const setRigidBody = env.lo_set_rigid_body;
pub const clearRigidBody = env.lo_clear_rigid_body;
pub const createAnimBody = env.lo_create_anim_body;
pub const freeAnimBody = env.lo_free_anim_body;
pub const setAnimBody = env.lo_set_anim_body;
pub const clearAnimBody = env.lo_clear_anim_body;

pub const rbSetPos = env.lo_rb_set_pos;
pub const rbSetRot = env.lo_rb_set_rot;
pub const abSetPos = env.lo_ab_set_pos;
pub const abSetRot = env.lo_ab_set_rot;

pub const rbSetMass = env.lo_rb_set_mass;
pub const rbAddGeom = env.lo_rb_add_geom;
pub const abAddGeom = env.lo_ab_add_geom;

pub const setCamPos = env.lo_set_campos;
pub const setCamTarget = env.lo_set_cam_target;
pub const lockMouse = env.lo_lock_mouse;

pub const dtxLayer = env.lo_dtx_layer;
pub const dtxFont = env.lo_dtx_font;
pub const dtxCanvas = env.lo_dtx_canvas;
pub const dtxOrigin = env.lo_dtx_origin;
pub const dtxHome = env.lo_dtx_home;
pub const dtxPos = env.lo_dtx_pos;
pub const dtxPosX = env.lo_dtx_pos_x;
pub const dtxPosY = env.lo_dtx_pos_y;
pub const dtxMove = env.lo_dtx_move;
pub const dtxMoveX = env.lo_dtx_move_x;
pub const dtxMoveY = env.lo_dtx_move_y;
pub const dtxCrlf = env.lo_dtx_crlf;
pub const dtxColor3b = env.lo_dtx_color3b;
pub const dtxColor3f = env.lo_dtx_color3f;
pub const dtxColor4f = env.lo_dtx_color4f;
pub const dtxColor1i = env.lo_dtx_color1i;
pub const dtxPutc = env.lo_dtx_putc;
pub const dtxPuts = env.lo_dtx_puts;
pub const dtxPutr = env.lo_dtx_putr;
