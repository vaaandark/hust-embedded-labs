#include <stdbool.h>
#include <stdio.h>

#include "../common/common.h"

#define MAX_FINGER_NUM 5

#define POINT_RADIUS 10

#define COLOR_BACKGROUND FB_COLOR(0xff, 0xff, 0xff)

#define RED FB_COLOR(255, 0, 0)
#define ORANGE FB_COLOR(255, 165, 0)
#define YELLOW FB_COLOR(255, 255, 0)
#define GREEN FB_COLOR(0, 255, 0)
#define CYAN FB_COLOR(0, 127, 255)
#define BLUE FB_COLOR(0, 0, 255)
#define PURPLE FB_COLOR(139, 0, 255)
#define WHITE FB_COLOR(255, 255, 255)
#define BLACK FB_COLOR(0, 0, 0)

const int color_range[] = {
	RED,
	ORANGE,
	YELLOW,
	GREEN,
	CYAN,
	BLUE,
	PURPLE,
	WHITE,
	BLACK,
};

struct last_position {
	int x, y;
} last_positions[MAX_FINGER_NUM];

static int touch_fd;
static void touch_event_cb(int fd) {
	int type, x, y, finger;
	type = touch_read(fd, &x, &y, &finger);
	if (type == TOUCH_ERROR) {
		printf("close touch fd\n");
		close(fd);
		task_delete_file(fd);
	}
	int color = color_range[finger];
	struct last_position *last = last_positions + finger;
	switch (type) {
		case TOUCH_PRESS:
			// printf("TOUCH_PRESS：x=%d,y=%d,finger=%d\n", x, y, finger);
			fb_draw_circle(x, y, POINT_RADIUS, color);
			last->x = x;
			last->y = y;
			break;
		case TOUCH_MOVE:
			// printf("TOUCH_MOVE：x=%d,y=%d,finger=%d\n", x, y, finger);
			fb_draw_circle(
				last->x,
				last->y,
				POINT_RADIUS + 2,
				COLOR_BACKGROUND);
			fb_draw_circle(x, y, POINT_RADIUS, color);
			last->x = x;
			last->y = y;
			break;
		case TOUCH_RELEASE:
			// printf("TOUCH_RELEASE：x=%d,y=%d,finger=%d\n", x, y, finger);
			fb_draw_circle(
				last->x,
				last->y,
				POINT_RADIUS + 2,
				COLOR_BACKGROUND);
			break;
		default:
			return;
	}
	fb_update();
	return;
}

int main(int argc, char *argv[]) {
	fb_init("/dev/fb0");
	fb_draw_rect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, COLOR_BACKGROUND);
	fb_update();

	//打开多点触摸设备文件, 返回文件fd
	touch_fd = touch_init("/dev/input/event2");
	//添加任务, 当touch_fd文件可读时, 会自动调用touch_event_cb函数
	task_add_file(touch_fd, touch_event_cb);

	task_loop();  //进入任务循环
	return 0;
}
