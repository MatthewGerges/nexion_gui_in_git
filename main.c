//#if NewMain

#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_demos/lv_demo.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include "lv_drivers/indev/evdev.h"

#include "src/mainScreen.h"

#include "src/splashScreen.h"

//extern int _tmain(int argc, char** argv);

int main2(int, char**);

//#define COMPLETED

#define LV_BUF_SIZE  960000
//#define DISP_BUF_SIZE (128 * 1024)
#define DISP_BUF_SIZE 960000

static lv_disp_draw_buf_t	disp_buf;
static lv_color_t		lvbuf1[LV_BUF_SIZE];
static lv_color_t		lvbuf2[LV_BUF_SIZE];
static void delay(int milliseconds);
static lv_timer_t * load_main_screen;
static void Update_Screen(lv_timer_t * timer);

/*
LodePNG Examples
Copyright (c) 2005-2012 Lode Vandevenne
This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.
Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:
    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source
    distribution.
*/

#include "lvgl/src/extra/libs/png/lodepng.h"

//void * (*open_cb)(lv_fs_drv_t * drv, const char * path, lv_fs_mode_t mode)
//		{
//
//		}


int main(int argc, char** argv)
{
	//const char* filename = argc > 1 ? argv[1] : "test.png";

	//decodeOneStep(filename);


	int		fd;
	char *	cursorOffCmd = "\033[?16;0;224c";

	//main2(argc, argv);


	/* Hide the bash cursor */
	fd = open("/dev/tty1", O_RDWR);
	write(fd, cursorOffCmd, strlen(cursorOffCmd));
	close(fd);

	/* LVGL init */
	printf("LVGL Initialization..\n");
	lv_init();

	/* Linux frame buffer init */
	printf("Linux frame buffer init..\n");
	fbdev_init();

	/* Linux input device init */
	printf("Linux HID (touch screen) init..\n");
	evdev_init();

	/*initialize device drivers registration*/
//	lv_fs_if_init();

	/* Initialize and register display driver */
	/*A small buffer for LittlevGL to draw the screen's content*/
	static lv_color_t buf[DISP_BUF_SIZE];

	/*Initialize a descriptor for the buffer*/
	static lv_disp_draw_buf_t disp_buf;
	lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);

	/*Initialize and register a display driver*/
	static lv_disp_drv_t disp_drv;
	lv_disp_drv_init(&disp_drv);
	disp_drv.draw_buf   = &disp_buf;
	disp_drv.flush_cb   = fbdev_flush;
	disp_drv.hor_res    = 800;
	disp_drv.ver_res    = 480;
	lv_disp_drv_register(&disp_drv);


	/* Initialize png decoder */
	lv_png_init();

	/* Initialize and register input driver */
	printf("Registering HID input driver with LVGL..\n");
	lv_indev_drv_t indev_drv;
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_POINTER;
	indev_drv.read_cb = evdev_read;
	lv_indev_t * touch_indev = lv_indev_drv_register(&indev_drv);
	if (touch_indev == 0) {
		printf("Touch device failed to register!\n");
	}

	/* for demo purposes show a mouse cursor */
	#ifdef SHOW_TOUCH_CURSOR
		lv_obj_t * cursor_obj = lv_img_create(lv_scr_act(), NULL);	/* Create an image object for the cursor */
		lv_img_set_src(cursor_obj, LV_SYMBOL_EDIT);					/* Set the image source */
		lv_indev_set_cursor(touch_indev, cursor_obj);				/* Connect the image  object to the driver */
	#endif

	/* Initialize loop-back connection to NexION application */
	printf("Opening TCP/IP loop-back connection to NexION..\n");


	Nexion_InitCom(1);

	printf("Drawing SplashScreen for 3 seconds...\n");
	//drawSplash();
	//delay(3000);
	/* Initialize main screen */
	printf("Drawing main screen..\n");
	drawMainScreen();
	//lv_demo_widgets();
	

	/* task loop */

	/*Handle LitlevGL tasks (tickless mode)*/
	    while(1) {
	        lv_timer_handler();
	        usleep(5000);
	    }


	//UNCOMMENT??
	/*
	printf("Running LVGL tasks..\n");
	while (1) {
		lv_tick_inc(5);
		lv_task_handler();
		usleep(5000);
	}
	*/

	return 0;
}

static void Update_Screen(lv_timer_t * timer)
{
	//printf("Drawing main screen..\n");
	drawMainScreen();
}

void delay(int milliseconds)
{
    long pause;
    clock_t now,then;

    pause = milliseconds*(CLOCKS_PER_SEC/1000);
    now = then = clock();
    while( (now-then) < pause )
        now = clock();
}

/*Set in lv_conf.h as `LV_TICK_CUSTOM_SYS_TIME_EXPR`*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
