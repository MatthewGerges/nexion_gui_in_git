/**
 * @file evdev.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "evdev.h"
#if USE_EVDEV != 0 || USE_BSD_EVDEV

#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#if USE_BSD_EVDEV
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif

#if USE_XKB
#include "xkb.h"
#endif /* USE_XKB */

/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max);

/**********************
 *  STATIC VARIABLES
 **********************/
int evdev_fd;
int evdev_root_x;
int evdev_root_y;
int evdev_button;

int evdev_key_val;
int	touch_HorzMin;
int	touch_HorzMax;
int	touch_VertMin;
int	touch_VertMax;
bool maskErrorMsg = false;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * Initialize the evdev interface
 */
void evdev_init(void)
{
char *		eventDevice;
	struct stat	buffer;
	bool		calFileFound = false;
	FILE *		fp;

	/* if touch screen calibration file found then use it.. */
	if (stat(EVDEV_CAL_FILE, &buffer) == 0) {
		fp = fopen(EVDEV_CAL_FILE, "r");
		fscanf(fp, "%d %d %d %d", &touch_HorzMin, &touch_HorzMax, &touch_VertMin, &touch_VertMax);
		fclose(fp);
		if (touch_HorzMin < 100 && touch_HorzMax > 1900 && touch_VertMin < 100 && touch_VertMax > 1900) {
			calFileFound = true;
		}
	}

	/* if matrix orbital LCD touch screen device found.. */
	if (stat(DEVICE_USB_TOUCH_MTX_OB, &buffer) == 0)
	{
		/* assign this device as our touch screen with default resolution */
		eventDevice   = DEVICE_USB_TOUCH_MTX_OB;
		if (calFileFound == false) {
			touch_HorzMin = EVDEV_MTX_OB_HOR_MIN;
			touch_HorzMax = EVDEV_MTX_OB_HOR_MAX;
			touch_VertMin = EVDEV_MTX_OB_VER_MIN;
			touch_VertMax = EVDEV_MTX_OB_VER_MAX;
		}
	}

	/* else if Kuman touch screen device found.. */
	else if (stat(DEVICE_USB_TOUCH_KUMAN, &buffer) == 0)
	{
		/* assign this device as our touch screen with default resolution */
		eventDevice   = DEVICE_USB_TOUCH_KUMAN;
		if (calFileFound == false) {
			touch_HorzMin = EVDEV_KUMAN_HOR_MIN;
			touch_HorzMax = EVDEV_KUMAN_HOR_MAX;
			touch_VertMin = EVDEV_KUMAN_VER_MIN;
			touch_VertMax = EVDEV_KUMAN_VER_MAX;
		}
	}

	/* else use default touch device with default resolution */
	else {
		eventDevice   = DEVICE_USB_TOUCH_DEV;
		if (calFileFound == false) {
			touch_HorzMin = EVDEV_HOR_MIN;
			touch_HorzMax = EVDEV_HOR_MAX;
			touch_VertMin = EVDEV_VER_MIN;
			touch_VertMax = EVDEV_VER_MAX;
		}
	}

	#if USE_BSD_EVDEV
    	evdev_fd = open(eventDevice, O_RDWR | O_NOCTTY);
	#else
    	evdev_fd = open(eventDevice, O_RDWR | O_NOCTTY | O_NDELAY);
	#endif
		if (evdev_fd == -1) {
			if (!maskErrorMsg) {
				perror("unable open evdev interface:");
				maskErrorMsg = true;
			}
			
			return;
    }

#if USE_BSD_EVDEV
		fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
	#else
		fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
	#endif

    evdev_root_x = 0;
    evdev_root_y = 0;
    evdev_key_val = 0;
    evdev_button = LV_INDEV_STATE_REL;

#if USE_XKB
    xkb_init();
#endif
}
/**
 * reconfigure the device file for evdev
 * @param dev_name set the evdev device filename
 * @return true: the device file set complete
 *         false: the device file doesn't exist current system
 */
bool evdev_set_file(char* dev_name)
{ 
     if(evdev_fd != -1) {
        close(evdev_fd);
     }
#if USE_BSD_EVDEV
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY);
#else
     evdev_fd = open(dev_name, O_RDWR | O_NOCTTY | O_NDELAY);
#endif

     if(evdev_fd == -1) {
        perror("unable open evdev interface:");
        return false;
     }

#if USE_BSD_EVDEV
     fcntl(evdev_fd, F_SETFL, O_NONBLOCK);
#else
     fcntl(evdev_fd, F_SETFL, O_ASYNC | O_NONBLOCK);
#endif

     evdev_root_x = 0;
     evdev_root_y = 0;
     evdev_key_val = 0;
     evdev_button = LV_INDEV_STATE_REL;

     return true;
}
/**
 * Get the current position and state of the evdev
 * @param data store the evdev data here
 * @return false: because the points are not buffered, so no more data to be read
 */
bool evdev_read(lv_indev_drv_t * drv, lv_indev_data_t * data)
{
    struct input_event	in;
    int					n;

    for (;;)
    {
    	/* try read from event device */
        n = read(evdev_fd, &in, sizeof(struct input_event));

        /* if read failed.. */
        if (n <= 0)
        {
        	/* if failure due to disconnected device.. */
        	if (errno == ENODEV || errno == EBADF)
        	{
        		/* then try to re-init device */
        		evdev_init();
        	}
        	break;
        }

        /* read successful, allow disconnect message printing again */
        maskErrorMsg = false;

        if (in.type == EV_REL) {
            if (in.code == REL_X) {
				#if EVDEV_SWAP_AXES
					evdev_root_y += in.value;
				#else
					evdev_root_x += in.value;
				#endif
            } else if (in.code == REL_Y) {
				#if EVDEV_SWAP_AXES
					evdev_root_x += in.value;
				#else
					evdev_root_y += in.value;
				#endif
            }
        }

        else if (in.type == EV_ABS) {
            if (in.code == ABS_X) {
				#if EVDEV_SWAP_AXES
					evdev_root_y = in.value;
				#else
					evdev_root_x = in.value;
//					printf("x=%d\n", in.value);
				#endif
            }
            else if (in.code == ABS_Y) {
				#if EVDEV_SWAP_AXES
					evdev_root_x = in.value;
				#else
					evdev_root_y = in.value;
//					printf("y=%d\n", in.value);
				#endif
            }
            else if (in.code == ABS_MT_POSITION_X) {
				#if EVDEV_SWAP_AXES
					evdev_root_y = in.value;
				#else
					evdev_root_x = in.value;
				#endif
            }
            else if (in.code == ABS_MT_POSITION_Y) {
				#if EVDEV_SWAP_AXES
					evdev_root_x = in.value;
				#else
					evdev_root_y = in.value;
				#endif
            }
        } else if (in.type == EV_KEY) {
			if (in.code == BTN_MOUSE || in.code == BTN_TOUCH) {
				if (in.value == 0) {
					evdev_button = LV_INDEV_STATE_REL;
				} else if(in.value == 1) {
					evdev_button = LV_INDEV_STATE_PR;
				}
			} else if(drv->type == LV_INDEV_TYPE_KEYPAD) {
				data->state = (in.value) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
				switch (in.code) {
					case KEY_BACKSPACE:
						data->key = LV_KEY_BACKSPACE;
						break;
					case KEY_ENTER:
						data->key = LV_KEY_ENTER;
						break;
					case KEY_UP:
						data->key = LV_KEY_UP;
						break;
					case KEY_LEFT:
						data->key = LV_KEY_PREV;
						break;
					case KEY_RIGHT:
						data->key = LV_KEY_NEXT;
						break;
					case KEY_DOWN:
						data->key = LV_KEY_DOWN;
						break;
					default:
						data->key = 0;
						break;
				}
				evdev_key_val = data->key;
				evdev_button = data->state;
				return false;
			}
        }
    }

    if (drv->type == LV_INDEV_TYPE_KEYPAD) {
        /* No data retrieved */
        data->key = evdev_key_val;
        data->state = evdev_button;
        return false;
    }

    if (drv->type != LV_INDEV_TYPE_POINTER) {
        return false;
	}

    /*Store the collected data*/
	#if EVDEV_CALIBRATE
        data->point.x = map(evdev_root_x, touch_HorzMin, touch_HorzMax, 0, lv_disp_get_hor_res(drv->disp));
        data->point.y = map(evdev_root_y, touch_VertMin, touch_VertMax, 0, lv_disp_get_ver_res(drv->disp));
	#else
		data->point.x = evdev_root_x;
		data->point.y = evdev_root_y;
	#endif

    data->state = evdev_button;

    if (data->point.x < 0) {
    	data->point.x = 0;
    }
    if (data->point.y < 0) {
    	data->point.y = 0;
    }
    if (data->point.x >= lv_disp_get_hor_res(drv->disp)) {
    	data->point.x = lv_disp_get_hor_res(drv->disp) - 1;
    }
    if (data->point.y >= lv_disp_get_ver_res(drv->disp)) {
    	data->point.y = lv_disp_get_ver_res(drv->disp) - 1;
    }

    return false;

}

/**********************
 *   STATIC FUNCTIONS
 **********************/
int map(int x, int in_min, int in_max, int out_min, int out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#endif
