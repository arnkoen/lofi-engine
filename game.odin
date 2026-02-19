package game

import "base:intrinsics"
import "base:runtime"
import lo "lofi-odin"

CAM_OFF_Y :: f32(7.0)
CAM_OFF_Z :: f32(-7.0)

IDLE :: i32(0)
RUNNING :: i32(1)
WALKING :: i32(2)

model: lo.Model
cube_model: lo.Model
floor_model: lo.Model
anims: lo.Anim_Set
tex_body: lo.Texture
tex_head: lo.Texture
tex_checker: lo.Texture
snd: lo.Sound

player_ent: lo.Entity
cube_ent: lo.Entity
floor_ent: lo.Entity

key_w: bool
key_a: bool
key_s: bool
key_d: bool
key_shift: bool
player_state: i32 = IDLE

//---------------------------------------------------------------------------

lenv3 :: proc(v: [3]f32) -> f32 {
	return intrinsics.sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])
}

normv3 :: proc(v: [3]f32) -> [3]f32 {
	len := lenv3(v)
	if len > 0.0 {return {v[0] / len, v[1] / len, v[2] / len}}
	return v
}

nlerpq :: proc(a, b: [4]f32, t: f32) -> [4]f32 {
	s: f32 = 1.0 if (a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3]) >= 0.0 else -1.0
	x := a[0] + t * (s * b[0] - a[0])
	y := a[1] + t * (s * b[1] - a[1])
	z := a[2] + t * (s * b[2] - a[2])
	w := a[3] + t * (s * b[3] - a[3])
	len := intrinsics.sqrt(x * x + y * y + z * z + w * w)
	return {x / len, y / len, z / len, w / len}
}

player_update :: proc "c" (dt: f32) {
	context = runtime.default_context()
	pos: [3]f32
	lo.get_position(player_ent, raw_data(&pos))

	move := [3]f32{0, 0, 0}
	if key_w {move[2] += 1.0}
	if key_s {move[2] -= 1.0}
	if key_a {move[0] += 1.0}
	if key_d {move[0] -= 1.0}

	is_moving := move[0] != 0.0 || move[2] != 0.0

	new_state: i32 = IDLE
	if is_moving {new_state = RUNNING if key_shift else WALKING}
	if new_state != player_state {
		player_state = new_state
		anim_desc := lo.Anim_Desc {
			set   = anims,
			anim  = player_state,
			flags = lo.ANIM_LOOP | lo.ANIM_PLAY,
		}
		lo.set_anims(player_ent, &anim_desc)
	}

	if is_moving {
		move = normv3(move)
		speed: f32 = 3.75 if player_state == RUNNING else 1.75
		pos[0] += move[0] * speed * dt
		pos[2] += move[2] * speed * dt
		lo.set_position(player_ent, raw_data(&pos))

		qy := move[0]
		qw := 1.0 + move[2]
		tmp := [3]f32{qy, qw, 0}
		len := lenv3(tmp)
		if len > 0.001 {qy /= len; qw /= len} else {qy = 1.0; qw = 0.0}
		target := [4]f32{0, qy, 0, qw}
		cur: [4]f32
		lo.get_rotation(player_ent, raw_data(&cur))
		blend := dt * 10.0
		if blend > 1.0 {blend = 1.0}
		rot := nlerpq(cur, target, blend)
		lo.set_rotation(player_ent, raw_data(&rot))
	}

	cam_target := [3]f32{pos[0], pos[1] + 1.5, pos[2]}
	cam_pos := [3]f32{cam_target[0], cam_target[1] + CAM_OFF_Y, cam_target[2] + CAM_OFF_Z}
	lo.set_cam_pos(raw_data(&cam_pos))
	lo.set_cam_target(raw_data(&cam_target))
}

// ---------------------------------------------------------------------------

@(export, link_name = "lo_init")
lo_init :: proc "c" () {
	model = lo.load_model("assets/game_base.iqm")
	anims = lo.load_anims("assets/game_base.iqm")
	tex_body = lo.load_texture("assets/skin_body.dds")
	tex_head = lo.load_texture("assets/skin_head.dds")
	tex_checker = lo.load_texture("assets/floor.dds")
	floor_model = lo.load_model("assets/plane.iqm")
	cube_model = lo.load_model("assets/cube.iqm")
	snd = lo.load_sound("assets/loop.ogg")

	//player
	player_ent = lo.create()
	lo.set_model(player_ent, model)
	lo.set_texture(player_ent, tex_body, 0)
	lo.set_texture(player_ent, tex_head, 1)
	player_anim := lo.Anim_Desc {
		set   = anims,
		anim  = IDLE,
		flags = lo.ANIM_LOOP | lo.ANIM_PLAY,
	}
	lo.set_anims(player_ent, &player_anim)
	player_body := lo.create_anim_body()
	player_geom := lo.Geom_Desc {
		type = lo.GEOM_CYLINDER,
		pos  = {0, 0.75, 0},
		rot  = {0, 0, 0, 1},
		size = {0.75, 1.0, 0},
	}
	lo.ab_add_geom(player_body, &player_geom)
	lo.set_anim_body(player_ent, player_body)

	//floor
	floor_ent = lo.create()
	lo.set_model(floor_ent, floor_model)
	lo.set_texture(floor_ent, tex_checker, 0)
	floor_scale := [3]f32{10, 10, 10}
	lo.set_scale(floor_ent, raw_data(&floor_scale))
	floor_body := lo.create_anim_body()
	floor_geom := lo.Geom_Desc {
		type = lo.GEOM_BOX,
		pos  = {0, 0, 0},
		rot  = {0, 0, 0, 1},
		size = {40, 0.1, 40},
	}
	lo.ab_add_geom(floor_body, &floor_geom)
	lo.set_anim_body(floor_ent, floor_body)

	//cube
	cube_ent = lo.create()
	lo.set_model(cube_ent, cube_model)
	lo.set_texture(cube_ent, tex_checker, 0)
	cube_scale := [3]f32{0.5, 0.5, 0.5}
	lo.set_scale(cube_ent, raw_data(&cube_scale))
	rb := lo.create_rigid_body()
	rb_pos := [3]f32{0, 5, 0}
	lo.rb_set_pos(rb, raw_data(&rb_pos))
	lo.rb_set_mass(rb, 3.25)
	rb_geom := lo.Geom_Desc {
		type = lo.GEOM_BOX,
		pos  = {0, 0, 0},
		rot  = {0, 0, 0, 1},
		size = {1.1, 1.1, 1.1},
	}
	lo.rb_add_geom(rb, &rb_geom)
	lo.set_rigid_body(cube_ent, rb)
	sound_desc := lo.Sound_Desc {
		sound     = snd,
		vol       = 0.75,
		min_range = 0.1,
		max_range = 100.0,
		flags     = lo.SOUND_SPATIAL,
	}
	lo.set_sound(cube_ent, &sound_desc)
}

@(export, link_name = "lo_frame")
lo_frame :: proc "c" (dt: f32) {
	player_update(dt)
	lo.dtx_canvas(800.0 * 0.5, 600.0 * 0.5)
	lo.dtx_origin(1.0, 1.0)
	lo.dtx_color3b(255, 255, 255)
	lo.dtx_puts("WASD: move\nShift: run\nSpace: play sound")
}

@(export, link_name = "lo_cleanup")
lo_cleanup :: proc "c" () {
	lo.release_model(model)
	lo.release_model(cube_model)
	lo.release_model(floor_model)
	lo.release_texture(tex_body)
	lo.release_texture(tex_head)
	lo.release_texture(tex_checker)
	lo.release_anims()
	lo.release_sound(snd)
}

@(export, link_name = "lo_mouse_pos")
lo_mouse_pos :: proc "c" (dx: f32, dy: f32) {}

@(export, link_name = "lo_mouse_button")
lo_mouse_button :: proc "c" (button: i32, down: bool) {}

@(export, link_name = "lo_key")
lo_key :: proc "c" (keycode: i32, down: bool, repeat: bool) {
	if repeat {return}
	switch keycode {
	case 87:
		key_w = down
	case 65:
		key_a = down
	case 83:
		key_s = down
	case 68:
		key_d = down
	case 340, 344:
		key_shift = down
	case 32:
		if down {lo.play_sound(cube_ent)}
	}
}
