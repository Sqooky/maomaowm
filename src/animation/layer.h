void layer_actual_size(LayerSurface *l, unsigned int *width,
					   unsigned int *height) {
	struct wlr_box box;

	if (l->animation.running) {
		*width = l->animation.current.width;
		*height = l->animation.current.height;
	} else {
		get_layer_target_geometry(l, &box);
		*width = box.width;
		*height = box.height;
	}
}

void get_layer_target_geometry(LayerSurface *l, struct wlr_box *target_box) {

	if (!l || !l->mapped)
		return;

	const struct wlr_layer_surface_v1_state *state = &l->layer_surface->current;

	// 限制区域
	// waybar一般都是大于0,表示要占用多少区域，所以计算位置也要用全部区域作为基准
	// 如果是-1可能表示独占所有可用空间
	// 如果是0，应该是表示使用exclusive_zone外的可用区域
	struct wlr_box bounds;
	if (state->exclusive_zone > 0 || state->exclusive_zone == -1)
		bounds = l->mon->m;
	else
		bounds = l->mon->w;

	// 初始化几何位置
	struct wlr_box box = {.width = state->desired_width,
						  .height = state->desired_height};

	// 水平方向定位
	const uint32_t both_horiz =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	if (box.width == 0) {
		box.x = bounds.x;
	} else if ((state->anchor & both_horiz) == both_horiz) {
		box.x = bounds.x + ((bounds.width - box.width) / 2);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
		box.x = bounds.x;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
		box.x = bounds.x + (bounds.width - box.width);
	} else {
		box.x = bounds.x + ((bounds.width - box.width) / 2);
	}

	// 垂直方向定位
	const uint32_t both_vert =
		ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
	if (box.height == 0) {
		box.y = bounds.y;
	} else if ((state->anchor & both_vert) == both_vert) {
		box.y = bounds.y + ((bounds.height - box.height) / 2);
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
		box.y = bounds.y;
	} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
		box.y = bounds.y + (bounds.height - box.height);
	} else {
		box.y = bounds.y + ((bounds.height - box.height) / 2);
	}

	// 应用边距
	if (box.width == 0) {
		box.x += state->margin.left;
		box.width = bounds.width - (state->margin.left + state->margin.right);
	} else {
		if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT) {
			box.x += state->margin.left;
		} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT) {
			box.x -= state->margin.right;
		}
	}

	if (box.height == 0) {
		box.y += state->margin.top;
		box.height = bounds.height - (state->margin.top + state->margin.bottom);
	} else {
		if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP) {
			box.y += state->margin.top;
		} else if (state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM) {
			box.y -= state->margin.bottom;
		}
	}

	target_box->x = box.x;
	target_box->y = box.y;
	target_box->width = box.width;
	target_box->height = box.height;
}

void set_layer_open_animaiton(LayerSurface *l, struct wlr_box geo) {
	int slide_direction;
	int horizontal, horizontal_value;
	int vertical, vertical_value;
	int center_x, center_y;

	if (!l || !l->mapped)
		return;

	center_x = l->geom.x + l->geom.width / 2;
	center_y = l->geom.y + l->geom.height / 2;
	horizontal =
		l->mon->w.x + l->mon->w.width - center_x < center_x - l->mon->w.x
			? RIGHT
			: LEFT;
	horizontal_value = horizontal == LEFT
						   ? center_x - l->mon->w.x
						   : l->mon->w.x + l->mon->w.width - center_x;
	vertical =
		l->mon->w.y + l->mon->w.height - center_y < center_y - l->mon->w.y
			? DOWN
			: UP;
	vertical_value = vertical == UP ? center_y - l->mon->w.y
									: l->mon->w.y + l->mon->w.height - center_y;
	slide_direction = horizontal_value < vertical_value ? horizontal : vertical;

	l->animainit_geom.width = l->geom.width;
	l->animainit_geom.height = l->geom.height;
	switch (slide_direction) {
	case UP:
		l->animainit_geom.x = l->geom.x;
		l->animainit_geom.y = l->mon->m.y - l->geom.height;
		break;
	case DOWN:
		l->animainit_geom.x = l->geom.x;
		l->animainit_geom.y =
			l->geom.y + l->mon->m.height - (l->geom.y - l->mon->m.y);
		break;
	case LEFT:
		l->animainit_geom.x = l->mon->m.x - l->geom.width;
		l->animainit_geom.y = l->geom.y;
		break;
	case RIGHT:
		l->animainit_geom.x =
			l->geom.x + l->mon->m.width - (l->geom.x - l->mon->m.x);
		l->animainit_geom.y = l->geom.y;
		break;
	default:
		l->animainit_geom.x = l->geom.x;
		l->animainit_geom.y = 0 - l->geom.height;
	}
}

void layer_draw_shadow(LayerSurface *l) {

	if (!l->mapped || !l->shadow)
		return;

	if (!shadows || !layer_shadows || l->noshadow) {
		wlr_scene_shadow_set_size(l->shadow, 0, 0);
		return;
	}

	uint32_t width, height;
	layer_actual_size(l, &width, &height);

	uint32_t delta = shadows_size;

	/* we calculate where to clip the shadow */
	struct wlr_box layer_box = {
		.x = 0,
		.y = 0,
		.width = width,
		.height = height,
	};

	struct wlr_box shadow_box = {
		.x = shadows_position_x,
		.y = shadows_position_y,
		.width = width + 2 * delta,
		.height = height + 2 * delta,
	};

	struct wlr_box intersection_box;
	wlr_box_intersection(&intersection_box, &layer_box, &shadow_box);
	/* clipped region takes shadow relative coords, so we translate everything
	 * by its position */
	intersection_box.x -= shadows_position_x;
	intersection_box.y -= shadows_position_y;

	struct clipped_region clipped_region = {
		.area = intersection_box,
		.corner_radius = border_radius,
		.corners = border_radius_location_default,
	};

	wlr_scene_node_set_position(&l->shadow->node, shadow_box.x, shadow_box.y);

	wlr_scene_shadow_set_size(l->shadow, shadow_box.width, shadow_box.height);
	wlr_scene_shadow_set_clipped_region(l->shadow, clipped_region);
}

void fadeout_layer_animation_next_tick(LayerSurface *l) {
	if (!l)
		return;

	double animation_passed =
		(double)l->animation.passed_frames / l->animation.total_frames;
	int type = l->animation.action = l->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);
	unsigned int width =
		l->animation.initial.width +
		(l->current.width - l->animation.initial.width) * factor;
	unsigned int height =
		l->animation.initial.height +
		(l->current.height - l->animation.initial.height) * factor;

	unsigned int x = l->animation.initial.x +
					 (l->current.x - l->animation.initial.x) * factor;
	unsigned int y = l->animation.initial.y +
					 (l->current.y - l->animation.initial.y) * factor;

	wlr_scene_node_set_position(&l->scene->node, x, y);

	l->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	double opacity = MAX(fadeout_begin_opacity - animation_passed, 0);

	if (animation_fade_out)
		wlr_scene_node_for_each_buffer(&l->scene->node,
									   scene_buffer_apply_opacity, &opacity);

	if (animation_passed == 1.0) {
		wl_list_remove(&l->fadeout_link);
		wlr_scene_node_destroy(&l->scene->node);
		free(l);
		l = NULL;
	} else {
		l->animation.passed_frames++;
	}
}

void layer_animation_next_tick(LayerSurface *l) {

	if (!l || !l->mapped)
		return;

	double animation_passed =
		(double)l->animation.passed_frames / l->animation.total_frames;

	int type = l->animation.action == NONE ? MOVE : l->animation.action;
	double factor = find_animation_curve_at(animation_passed, type);

	unsigned int width =
		l->animation.initial.width +
		(l->current.width - l->animation.initial.width) * factor;
	unsigned int height =
		l->animation.initial.height +
		(l->current.height - l->animation.initial.height) * factor;

	unsigned int x = l->animation.initial.x +
					 (l->current.x - l->animation.initial.x) * factor;
	unsigned int y = l->animation.initial.y +
					 (l->current.y - l->animation.initial.y) * factor;

	wlr_scene_node_set_position(&l->scene->node, x, y);

	l->animation.current = (struct wlr_box){
		.x = x,
		.y = y,
		.width = width,
		.height = height,
	};

	if (animation_passed == 1.0) {
		l->animation.running = false;
		l->need_output_flush = false;
		l->animation.action = MOVE;
	} else {
		l->animation.passed_frames++;
	}
}

void init_fadeout_layers(LayerSurface *l) {

	if (!layer_animations || l->noanim) {
		return;
	}

	if (!l->mon)
		return;

	if (!l->scene) {
		return;
	}

	if (l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM ||
		l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND)
		return;

	LayerSurface *fadeout_layer = ecalloc(1, sizeof(*fadeout_layer));

	wlr_scene_node_set_enabled(&l->scene->node, true);
	fadeout_layer->scene =
		wlr_scene_tree_snapshot(&l->scene->node, layers[LyrFadeOut]);
	wlr_scene_node_set_enabled(&l->scene->node, false);

	if (!fadeout_layer->scene) {
		free(fadeout_layer);
		return;
	}

	fadeout_layer->animation.duration = animation_duration_close;
	fadeout_layer->geom = fadeout_layer->current =
		fadeout_layer->animainit_geom = fadeout_layer->animation.initial =
			l->animation.current;
	fadeout_layer->mon = l->mon;
	fadeout_layer->animation.action = CLOSE;

	// 这里snap节点的坐标设置是使用的相对坐标，所以不能加上原来坐标
	// 这跟普通node有区别

	fadeout_layer->animation.initial.x = 0;
	fadeout_layer->animation.initial.y = 0;

	fadeout_layer->current.y =
		l->geom.y + l->geom.height / 2 > l->mon->m.y + l->mon->m.height / 2
			? l->mon->m.height -
				  (l->animation.current.y - l->mon->m.y) // down out
			: l->mon->m.y - l->geom.height;				 // up out
	fadeout_layer->current.x = 0;						 // x无偏差，垂直划出

	fadeout_layer->animation.passed_frames = 0;
	fadeout_layer->animation.total_frames =
		fadeout_layer->animation.duration / output_frame_duration_ms();
	wlr_scene_node_set_enabled(&fadeout_layer->scene->node, true);
	wl_list_insert(&fadeout_layers, &fadeout_layer->fadeout_link);
}

void layer_set_pending_state(LayerSurface *l) {

	if (!l || !l->mapped)
		return;

	l->pending = l->geom;
	if (l->animation.action == OPEN)
		set_layer_open_animaiton(l, l->geom);
	else
		l->animainit_geom = l->animation.current;
	// 判断是否需要动画
	if (!animations || !layer_animations || l->noanim ||
		l->layer_surface->current.layer ==
			ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND ||
		l->layer_surface->current.layer == ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM) {
		l->animation.should_animate = false;
	} else {
		l->animation.should_animate = true;
	}

	l->animation.duration = animation_duration_open;
	l->animation.action = OPEN;
	// 开始动画
	layer_commit(l);
	l->dirty = true;
}

void layer_commit(LayerSurface *l) {

	if (!l || !l->mapped)
		return;

	l->current = l->pending; // 设置动画的结束位置

	if (l->animation.should_animate) {
		if (!l->animation.running) {
			l->animation.current = l->animainit_geom;
		}

		l->animation.initial = l->animainit_geom;
		// 设置动画速度
		l->animation.passed_frames = 0;
		l->animation.total_frames =
			l->animation.duration / output_frame_duration_ms();

		// 标记动画开始
		l->animation.running = true;
		l->animation.should_animate = false;
	}
	// 请求刷新屏幕
	wlr_output_schedule_frame(l->mon->wlr_output);
}

bool layer_draw_frame(LayerSurface *l) {

	if (!l || !l->mapped)
		return false;

	if (!l->need_output_flush)
		return false;

	if (l->layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_TOP &&
		l->layer_surface->current.layer != ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY) {
		return false;
	}

	if (animations && layer_animations && l->animation.running && !l->noanim) {
		layer_animation_next_tick(l);
		layer_draw_shadow(l);
	} else {
		layer_draw_shadow(l);
		l->need_output_flush = false;
	}
	return true;
}

bool layer_draw_fadeout_frame(LayerSurface *l) {
	if (!l)
		return false;

	fadeout_layer_animation_next_tick(l);
	return true;
}
