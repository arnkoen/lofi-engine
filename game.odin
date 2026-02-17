package game
import lo "lofi-odin"

ent: lo.Entity
model: lo.Model
tex_body: lo.Texture
tex_head: lo.Texture
snd: lo.Sound
time: f32

@(export, link_name = "lo_init")
lo_init :: proc "c" () {
	ent = lo.create()
	model = lo.load_model("assets/game_base.iqm")
	anims := lo.load_anims("assets/game_base.iqm")
	tex_body = lo.load_texture("assets/skin_body.dds")
	tex_head = lo.load_texture("assets/skin_head.dds")
	lo.set_model(ent, model)
	lo.set_texture(ent, tex_body, 0)
	lo.set_texture(ent, tex_head, 1)
	desc := lo.Anim_Desc {
		set   = anims,
		anim  = 2,
		flags = lo.ANIM_LOOP | lo.ANIM_PLAY,
	}
	lo.set_anims(ent, &desc)
	snd = lo.load_sound("assets/loop.ogg")
	snd_desc := lo.Sound_Desc {
		sound = snd,
		vol   = 0.75,
		flags = lo.SOUND_PLAY | lo.SOUND_LOOP,
	}
	lo.set_sound(ent, &snd_desc)
}

@(export, link_name = "lo_frame")
lo_frame :: proc "c" (dt: f32) {
	time += dt
	radius: f32 = 4.0
	speed: f32 = 0.5
	angle := time * speed
	cam_pos := [3]f32{lo.sinf(angle) * radius, 1.5, lo.cosf(angle) * radius}
	cam_target := [3]f32{0, 0.75, 0}
	lo.set_cam_pos(raw_data(&cam_pos))
	lo.set_cam_target(raw_data(&cam_target))

	lo.dtx_canvas(800.0, 600.0)
	lo.dtx_origin(1.0, 1.0)
	lo.dtx_color3b(255, 255, 255)
	lo.dtx_puts("Hello Lofi")
}

@(export, link_name = "lo_cleanup")
lo_cleanup :: proc "c" () {
	lo.destroy(ent)
	lo.release_model(model)
	lo.release_texture(tex_body)
	lo.release_texture(tex_head)
	lo.release_anims()
	lo.release_sound(snd)
}
