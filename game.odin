package game

ent: Entity
time: f32

@(export, link_name = "lo_init")
lo_init :: proc "c" () {
	ent = create()
	model := load_model("assets/game_base.iqm")
	anims := load_anims("assets/game_base.iqm")
	tex_body := load_texture("assets/skin_body.dds")
	tex_head := load_texture("assets/skin_head.dds")
	set_model(ent, model)
	set_texture(ent, tex_body, 0)
	set_texture(ent, tex_head, 1)
	desc := Anim_Desc {
		set   = anims,
		anim  = 2,
		flags = ANIM_LOOP | ANIM_PLAY,
	}
	set_anims(ent, &desc)
	snd := load_sound("assets/loop.ogg")
	snd_desc := Sound_Desc {
		sound = snd,
		vol   = 0.75,
		flags = SOUND_PLAY | SOUND_LOOP,
	}
	set_sound(ent, &snd_desc)
}

@(export, link_name = "lo_frame")
lo_frame :: proc "c" (dt: f32) {
	time += dt
	radius: f32 = 4.0
	speed: f32 = 0.5
	angle := time * speed
	cam_pos := [3]f32{sinf(angle) * radius, 1.5, cosf(angle) * radius}
	cam_target := [3]f32{0, 0.75, 0}
	set_cam_pos(raw_data(&cam_pos))
	set_cam_target(raw_data(&cam_target))

	dtx_canvas(800.0, 600.0)
	dtx_origin(1.0, 1.0)
	dtx_color3b(255, 255, 255)
	dtx_puts("Hello Lofi")
}

@(export, link_name = "lo_cleanup")
lo_cleanup :: proc "c" () {
	destroy(ent)
}
