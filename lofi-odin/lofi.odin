package lofi

Entity :: struct {
	id: u32,
}

Texture :: struct {
	id: u32,
}

Model :: struct {
	id: u32,
}

Anim_Set :: struct {
	id: u32,
}

Sound :: struct {
	id: u32,
}

// Flags
ANIM_PLAY :: 1 << 0
ANIM_LOOP :: 1 << 1

SOUND_PLAY :: 1 << 1
SOUND_LOOP :: 1 << 2
SOUND_SPATIAL :: 1 << 3

Anim_Desc :: struct {
	set:   Anim_Set,
	flags: i32,
	anim:  i32,
}

Sound_Desc :: struct {
	sound:     Sound,
	vol:       f32,
	min_range: f32,
	max_range: f32,
	flags:     u32,
}

foreign import env "env"

@(default_calling_convention = "c")
foreign env {
	@(link_name = "lo_load_texture")
	load_texture :: proc(path: cstring) -> Texture ---

	@(link_name = "lo_release_texture")
	release_texture :: proc(tex: Texture) ---

	@(link_name = "lo_load_model")
	load_model :: proc(path: cstring) -> Model ---

	@(link_name = "lo_release_model")
	release_model :: proc(model: Model) ---

	@(link_name = "lo_load_anims")
	load_anims :: proc(path: cstring) -> Anim_Set ---

	@(link_name = "lo_release_anims")
	release_anims :: proc() ---

	@(link_name = "lo_load_sound")
	load_sound :: proc(path: cstring) -> Sound ---

	@(link_name = "lo_release_sound")
	release_sound :: proc(sound: Sound) ---

	@(link_name = "lo_create")
	create :: proc() -> Entity ---

	@(link_name = "lo_valid")
	valid :: proc(entity: Entity) -> bool ---

	@(link_name = "lo_destroy")
	destroy :: proc(entity: Entity) ---

	@(link_name = "lo_set_position")
	set_position :: proc(e: Entity, pos: [^]f32) ---

	@(link_name = "lo_get_position")
	get_position :: proc(e: Entity, out: [^]f32) ---

	@(link_name = "lo_set_rotation")
	set_rotation :: proc(e: Entity, rot: [^]f32) ---

	@(link_name = "lo_get_rotation")
	get_rotation :: proc(e: Entity, out: [^]f32) ---

	@(link_name = "lo_set_scale")
	set_scale :: proc(e: Entity, scale: [^]f32) ---

	@(link_name = "lo_get_scale")
	get_scale :: proc(e: Entity, out: [^]f32) ---

	@(link_name = "lo_set_parent")
	set_parent :: proc(entity: Entity, parent: Entity) ---

	@(link_name = "lo_remove_parent")
	remove_parent :: proc(entity: Entity) ---

	@(link_name = "lo_add_child")
	add_child :: proc(entity: Entity, child: Entity) ---

	@(link_name = "lo_set_children")
	set_children :: proc(e: Entity, children: [^]Entity) ---

	@(link_name = "lo_clear_children")
	clear_children :: proc(e: Entity) ---

	@(link_name = "lo_set_model")
	set_model :: proc(entity: Entity, model: Model) ---

	@(link_name = "lo_clear_model")
	clear_model :: proc(entity: Entity) ---

	@(link_name = "lo_set_texture")
	set_texture :: proc(entity: Entity, handle: Texture, slot: i32) ---

	@(link_name = "lo_clear_textures")
	clear_textures :: proc(entity: Entity) ---

	@(link_name = "lo_set_anims")
	set_anims :: proc(entity: Entity, desc: ^Anim_Desc) ---

	@(link_name = "lo_clear_anims")
	clear_anims :: proc(entity: Entity) ---

	@(link_name = "lo_set_sound")
	set_sound :: proc(e: Entity, desc: ^Sound_Desc) ---

	@(link_name = "lo_play_sound")
	play_sound :: proc(e: Entity) ---

	@(link_name = "lo_stop_sound")
	stop_sound :: proc(e: Entity) ---

	@(link_name = "lo_clear_sound")
	clear_sound :: proc(e: Entity) ---

	@(link_name = "lo_sinf")
	sinf :: proc(x: f32) -> f32 ---

	@(link_name = "lo_cosf")
	cosf :: proc(x: f32) -> f32 ---

	@(link_name = "lo_set_campos")
	set_cam_pos :: proc(pos: [^]f32) ---

	@(link_name = "lo_set_cam_target")
	set_cam_target :: proc(target: [^]f32) ---

	@(link_name = "lo_dtx_layer")
	dtx_layer :: proc(layer_id: i32) ---

	@(link_name = "lo_dtx_font")
	dtx_font :: proc(font_index: i32) ---

	@(link_name = "lo_dtx_canvas")
	dtx_canvas :: proc(w: f32, h: f32) ---

	@(link_name = "lo_dtx_origin")
	dtx_origin :: proc(x: f32, y: f32) ---

	@(link_name = "lo_dtx_home")
	dtx_home :: proc() ---

	@(link_name = "lo_dtx_pos")
	dtx_pos :: proc(x: f32, y: f32) ---

	@(link_name = "lo_dtx_pos_x")
	dtx_pos_x :: proc(x: f32) ---

	@(link_name = "lo_dtx_pos_y")
	dtx_pos_y :: proc(y: f32) ---

	@(link_name = "lo_dtx_move")
	dtx_move :: proc(dx: f32, dy: f32) ---

	@(link_name = "lo_dtx_move_x")
	dtx_move_x :: proc(dx: f32) ---

	@(link_name = "lo_dtx_move_y")
	dtx_move_y :: proc(dy: f32) ---

	@(link_name = "lo_dtx_crlf")
	dtx_crlf :: proc() ---

	@(link_name = "lo_dtx_color3b")
	dtx_color3b :: proc(r: u8, g: u8, b: u8) ---

	@(link_name = "lo_dtx_color3f")
	dtx_color3f :: proc(r: f32, g: f32, b: f32) ---

	@(link_name = "lo_dtx_color4f")
	dtx_color4f :: proc(rgba: [^]f32) ---

	@(link_name = "lo_dtx_color1i")
	dtx_color1i :: proc(rgba: u32) ---

	@(link_name = "lo_dtx_putc")
	dtx_putc :: proc(c: u8) ---

	@(link_name = "lo_dtx_puts")
	dtx_puts :: proc(str: cstring) ---

	@(link_name = "lo_dtx_putr")
	dtx_putr :: proc(str: cstring, len: i32) ---
}
