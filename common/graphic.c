#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <omp.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "common.h"

static int LCD_FB_FD;
static int *LCD_FB_BUF = NULL;
static int DRAW_BUF[SCREEN_WIDTH * SCREEN_HEIGHT];

static struct area { int x1, x2, y1, y2; } update_area = {0, 0, 0, 0};

#define AREA_SET_EMPTY(pa) \
    do { \
        (pa)->x1 = SCREEN_WIDTH; \
        (pa)->x2 = 0; \
        (pa)->y1 = SCREEN_HEIGHT; \
        (pa)->y2 = 0; \
    } while (0)

void fb_init(char *dev) {
    int fd;
    struct fb_fix_screeninfo fb_fix;
    struct fb_var_screeninfo fb_var;

    if (LCD_FB_BUF != NULL)
        return; /*already done*/

    //进入终端图形模式
    fd = open("/dev/tty0", O_RDWR, 0);
    ioctl(fd, KDSETMODE, KD_GRAPHICS);
    close(fd);

    //First: Open the device
    if ((fd = open(dev, O_RDWR)) < 0) {
        printf("Unable to open framebuffer %s, errno = %d\n", dev, errno);
        return;
    }
    if (ioctl(fd, FBIOGET_FSCREENINFO, &fb_fix) < 0) {
        printf("Unable to FBIOGET_FSCREENINFO %s\n", dev);
        return;
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &fb_var) < 0) {
        printf("Unable to FBIOGET_VSCREENINFO %s\n", dev);
        return;
    }

    printf(
        "framebuffer info: bits_per_pixel=%u,size=(%d,%d),virtual_pos_size=(%d,%d)(%d,%d),line_length=%u,smem_len=%u\n",
        fb_var.bits_per_pixel,
        fb_var.xres,
        fb_var.yres,
        fb_var.xoffset,
        fb_var.yoffset,
        fb_var.xres_virtual,
        fb_var.yres_virtual,
        fb_fix.line_length,
        fb_fix.smem_len);

    //Second: mmap
    void *addr =
        mmap(NULL, fb_fix.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == (void *)-1) {
        printf("failed to mmap memory for framebuffer.\n");
        return;
    }

    if ((fb_var.xoffset != 0) || (fb_var.yoffset != 0)) {
        fb_var.xoffset = 0;
        fb_var.yoffset = 0;
        if (ioctl(fd, FBIOPAN_DISPLAY, &fb_var) < 0) {
            printf("FBIOPAN_DISPLAY framebuffer failed\n");
        }
    }

    LCD_FB_FD = fd;
    LCD_FB_BUF = addr;

    //set empty
    AREA_SET_EMPTY(&update_area);
    return;
}

static void _copy_area(int *dst, int *src, struct area *pa) {
    int x, y, w, h;
    x = pa->x1;
    w = pa->x2 - x;
    y = pa->y1;
    h = pa->y2 - y;
    src += y * SCREEN_WIDTH + x;
    dst += y * SCREEN_WIDTH + x;
    while (h-- > 0) {
        memcpy(dst, src, w * 4);
        src += SCREEN_WIDTH;
        dst += SCREEN_WIDTH;
    }
}

static int _check_area(struct area *pa) {
    if (pa->x2 == 0)
        return 0;  //is empty

    if (pa->x1 < 0)
        pa->x1 = 0;
    if (pa->x2 > SCREEN_WIDTH)
        pa->x2 = SCREEN_WIDTH;
    if (pa->y1 < 0)
        pa->y1 = 0;
    if (pa->y2 > SCREEN_HEIGHT)
        pa->y2 = SCREEN_HEIGHT;

    if ((pa->x2 > pa->x1) && (pa->y2 > pa->y1))
        return 1;  //no empty

    //set empty
    AREA_SET_EMPTY(pa);
    return 0;
}

void fb_update(void) {
    if (_check_area(&update_area) == 0)
        return;	 //is empty
    _copy_area(LCD_FB_BUF, DRAW_BUF, &update_area);
    AREA_SET_EMPTY(&update_area);  //set empty
    return;
}

/*======================================================================*/

static void *_begin_draw(int x, int y, int w, int h) {
    int x2 = x + w;
    int y2 = y + h;
    if (update_area.x1 > x)
        update_area.x1 = x;
    if (update_area.y1 > y)
        update_area.y1 = y;
    if (update_area.x2 < x2)
        update_area.x2 = x2;
    if (update_area.y2 < y2)
        update_area.y2 = y2;
    return DRAW_BUF;
}

void fb_draw_pixel(int x, int y, int color) {
    if (x < 0 || y < 0 || x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT)
        return;
    int *buf = _begin_draw(x, y, 1, 1);
    /*---------------------------------------------------*/
    *(buf + y * SCREEN_WIDTH + x) = color;
    /*---------------------------------------------------*/
    return;
}

void fb_draw_circle(int x, int y, int r, int color) {
    x = x < 0 ? 0 : x;
    x = x >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : x;
    y = y < 0 ? 0 : y;
    y = y >= SCREEN_HEIGHT ? SCREEN_HEIGHT - 1 : y;
    int x_lower_bound = x - r < 0 ? 0 : x - r;
    int x_upper_bound = x + r >= SCREEN_WIDTH ? SCREEN_WIDTH - 1 : x + r;
    int y_lower_bound = y - r < 0 ? 0 : y - r;
    int y_upper_bound = y + r >= SCREEN_HEIGHT ? SCREEN_HEIGHT - 1 : y + r;
    for (int i = y_lower_bound; i <= y_upper_bound; ++i) {
        for (int j = x_lower_bound; j <= x_upper_bound; ++j) {
            int delta_x = j - x;
            int delta_y = i - y;
            if (delta_x * delta_x + delta_y * delta_y <= r * r) {
                fb_draw_pixel(j, i, color);
            }
        }
    }
}

void fb_draw_rect(int x, int y, int w, int h, int color) {
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (x + w > SCREEN_WIDTH) {
        w = SCREEN_WIDTH - x;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (y + h > SCREEN_HEIGHT) {
        h = SCREEN_HEIGHT - y;
    }
    if (w <= 0 || h <= 0)
        return;
    int *buf = _begin_draw(x, y, w, h);
    /*---------------------------------------------------*/
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            *(buf + (y + j) * SCREEN_WIDTH + (x + i)) = color;
        }
    }
    /*---------------------------------------------------*/
    return;
}

void fb_draw_line(int x1, int y1, int x2, int y2, int color) {
    int x_min = x1 < x2 ? x1 : x2;
    int y_min = y1 < y2 ? y1 : y2;
    int x_max = x1 > x2 ? x1 : x2;
    int y_max = y1 > y2 ? y1 : y2;
    int *buf = _begin_draw(x_min, y_min, x_max - x_min + 1, y_max - y_min + 1);
    if (x1 == x2) {
#pragma omp parallel for simd proc_bind(close)
        for (int yy = y_min; yy <= y_max; ++yy) {
            *(buf + yy * SCREEN_WIDTH + x1) = color;
        }
        return;
    }
    int xx, yy;
    if ((y2 - y1) / (x2 - x1) == 0) {
        if (x2 > x1) {
#pragma omp parallel for simd proc_bind(close)
            for (xx = x1; xx != (x2 + 1); ++xx) {
                yy = (y2 - y1) * (xx - x1) / (x2 - x1) + y1;
                *(buf + yy * SCREEN_WIDTH + xx) = color;
            }
        } else {
#pragma omp parallel for simd proc_bind(close)
            for (xx = x1; xx != (x2 - 1); --xx) {
                yy = (y2 - y1) * (xx - x1) / (x2 - x1) + y1;
                *(buf + yy * SCREEN_WIDTH + xx) = color;
            }
        }
    } else {
        if (y2 > y1) {
#pragma omp parallel for simd proc_bind(close)
            for (yy = y1; yy != (y2 + 1); ++yy) {
                xx = (x2 - x1) * (yy - y1) / (y2 - y1) + x1;
                *(buf + yy * SCREEN_WIDTH + xx) = color;
            }
        } else {
#pragma omp parallel for simd proc_bind(close)
            for (yy = y1; yy != (y2 - 1); --yy) {
                xx = (x2 - x1) * (yy - y1) / (y2 - y1) + x1;
                *(buf + yy * SCREEN_WIDTH + xx) = color;
            }
        }
    }
    return;
}

void fb_draw_image(int x, int y, fb_image *image, int color) {
    if (image == NULL)
        return;

    int ix = 0;	 //image x
    int iy = 0;	 //image y
    int w = image->pixel_w;	 //draw width
    int h = image->pixel_h;	 //draw height

    if (x < 0) {
        w += x;
        ix -= x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        iy -= y;
        y = 0;
    }

    if (x + w > SCREEN_WIDTH) {
        w = SCREEN_WIDTH - x;
    }
    if (y + h > SCREEN_HEIGHT) {
        h = SCREEN_HEIGHT - y;
    }
    if ((w <= 0) || (h <= 0))
        return;

    int *buf = _begin_draw(x, y, w, h);
    int *dst = buf + y * SCREEN_WIDTH + x;
    char *src;

    if (image->color_type == FB_COLOR_RGB_8880) /*lab3: jpg*/
    {
        int *jpg_src = (int *)image->content + iy * image->pixel_w + ix;
        int len = 4 * w;
#pragma omp parallel for simd
        for (int i = 0; i < h; ++i) {
            // for (int j = 0; j < w; ++j) {
            // 	dst[i * SCREEN_WIDTH + j] = jpg_src[i * image->pixel_w + j];
            // }
            memcpy(dst + i * SCREEN_WIDTH, jpg_src + i * image->pixel_w, len);
        }
        return;
    } else if (image->color_type == FB_COLOR_RGBA_8888) /*lab3: png*/
    {
        src = image->content + iy * image->pixel_w * 4 + ix * 4;
#pragma omp parallel for simd
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) {
                char *src_ = src + i * image->pixel_w * 4 + j * 4;
                int alpha = src_[0];
                int *dst_ = dst + i * SCREEN_WIDTH + j;
                char *bgr = (char *)dst_;
                switch (alpha) {
                    case 0:
                        break;
                    case 255:
                        bgr[0] = src_[0];
                        bgr[1] = src_[1];
                        bgr[2] = src_[2];
                        break;
                    default:
                        bgr[0] += ((src_[0] - bgr[0]) * alpha) >> 8;
                        bgr[1] += ((src_[1] - bgr[1]) * alpha) >> 8;
                        bgr[2] += ((src_[2] - bgr[2]) * alpha) >> 8;
                        break;
                }
            }
        }
        return;
    } else if (image->color_type == FB_COLOR_ALPHA_8) /*lab3: font*/
    {
        src = image->content + iy * image->pixel_w + ix;
        char r = color >> 16;
        char g = color >> 8;
        char b = color;
#pragma omp parallel for simd
        for (int i = 0; i < h; ++i) {
            for (int j = 0; j < w; ++j) {
                int alpha = *(src + i * image->pixel_w + j);
                int *dst_ = dst + i * SCREEN_WIDTH + j;
                char *bgr = (char *)dst_;
                switch (alpha) {
                    case 0:
                        break;
                    case 255:
                        *dst_ = color;
                        break;
                    default:
                        bgr[0] += ((b - bgr[0]) * alpha) >> 8;
                        bgr[1] += ((g - bgr[1]) * alpha) >> 8;
                        bgr[2] += ((r - bgr[2]) * alpha) >> 8;
                        break;
                }
            }
        }
        return;
    }
    /*---------------------------------------------------------------*/
    return;
}

void fb_draw_border(int x, int y, int w, int h, int color) {
    if (w <= 0 || h <= 0)
        return;
    fb_draw_rect(x, y, w, 1, color);
    if (h > 1) {
        fb_draw_rect(x, y + h - 1, w, 1, color);
        fb_draw_rect(x, y + 1, 1, h - 2, color);
        if (w > 1)
            fb_draw_rect(x + w - 1, y + 1, 1, h - 2, color);
    }
}

/** draw a text string **/
void fb_draw_text(int x, int y, char *text, int font_size, int color) {
    fb_font_info info;
    int i = 0;
    int len = strlen(text);
    fb_image **imgs = (fb_image **)malloc(sizeof(fb_image *) * len);
    int cnt = 0;
    int *font_left_bound = (int *)malloc(sizeof(int) * len);
    int *font_top_bound = (int *)malloc(sizeof(int) * len);
    while (i < len) {
        imgs[cnt] = fb_read_font_image(text + i, font_size, &info);
        if (imgs[cnt] == NULL)
            break;
        font_left_bound[cnt] = x + info.left;
        font_top_bound[cnt] = y - info.top;
        x += info.advance_x;
        i += info.bytes;
        cnt += 1;
    }

    #pragma omp parallel for
    for (i = 0; i < cnt; ++i) {
        fb_draw_image(font_left_bound[i], font_top_bound[i], imgs[i], color);
        fb_free_image(imgs[i]);
    }

    free(imgs);
    free(font_left_bound);
    free(font_top_bound);

    // fb_image *img;
    // while (i < len) {
    // 	img = fb_read_font_image(text + i, font_size, &info);
    // 	if (img == NULL)
    // 		break;
    // 	fb_draw_image(x + info.left, y - info.top, img, color);
    // 	fb_free_image(img);

    // 	x += info.advance_x;
    // 	i += info.bytes;
    // }
    return;
}
