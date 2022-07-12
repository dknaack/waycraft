static void
block_texcoords(enum block_type block, v2 *uv)
{
	uv[0] = v2_mulf(v2_add(v2(block, 0), v2(0, 0)), 1 / 16.f);
	uv[1] = v2_mulf(v2_add(v2(block, 0), v2(1, 0)), 1 / 16.f);
	uv[2] = v2_mulf(v2_add(v2(block, 0), v2(0, 1)), 1 / 16.f);
	uv[3] = v2_mulf(v2_add(v2(block, 0), v2(1, 1)), 1 / 16.f);
}

static void
block_texcoords_top(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_GRASS:
		block_texcoords(BLOCK_GRASS_TOP, uv);
		break;
	case BLOCK_MONITOR_UP:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_DOWN:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_RIGHT:
	case BLOCK_MONITOR_LEFT:
	case BLOCK_MONITOR_FORWARD:
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

static void
block_texcoords_bottom(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_GRASS:
		block_texcoords(BLOCK_DIRT, uv);
		break;
	case BLOCK_MONITOR_UP:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_DOWN:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_RIGHT:
	case BLOCK_MONITOR_LEFT:
	case BLOCK_MONITOR_FORWARD:
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

static void
block_texcoords_right(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_MONITOR_LEFT:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_RIGHT:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_UP:
	case BLOCK_MONITOR_DOWN:
	case BLOCK_MONITOR_FORWARD:
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

static void
block_texcoords_left(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_MONITOR_RIGHT:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_LEFT:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_UP:
	case BLOCK_MONITOR_DOWN:
	case BLOCK_MONITOR_FORWARD:
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

static void
block_texcoords_front(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_FORWARD:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_UP:
	case BLOCK_MONITOR_DOWN:
	case BLOCK_MONITOR_RIGHT:
	case BLOCK_MONITOR_LEFT:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

static void
block_texcoords_back(enum block_type block, v2 *uv)
{
	switch (block) {
	case BLOCK_MONITOR_FORWARD:
		block_texcoords(BLOCK_MONITOR_BACK, uv);
		break;
	case BLOCK_MONITOR_BACKWARD:
		block_texcoords(BLOCK_MONITOR_FRONT, uv);
		break;
	case BLOCK_MONITOR_RIGHT:
	case BLOCK_MONITOR_LEFT:
	case BLOCK_MONITOR_UP:
	case BLOCK_MONITOR_DOWN:
		block_texcoords(BLOCK_MONITOR_SIDE, uv);
		break;
	default:
		block_texcoords(block, uv);
		break;
	}
}

