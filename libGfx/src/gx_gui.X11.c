#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "gx_gui.h"

static Display *display = NULL;
static Window window = 0;
static XImage *g_image = NULL;

static XGCValues gr_values;
static GC gc;

static int create_window(int width, int height) {
	display = XOpenDisplay(NULL);
	int screen = DefaultScreen(display);
	int depth = DefaultDepth(display, screen);

	g_image = XCreateImage(display, CopyFromParent, depth, ZPixmap, 0, NULL, width, height, 32, 0);
	window = XCreateSimpleWindow(display, XDefaultRootWindow(display), 0, 0, width, height, 0, BlackPixel(display, 0), WhitePixel(display, 0));
	XSelectInput(display, window,  StructureNotifyMask | SubstructureNotifyMask | ExposureMask | DestroyNotify |
		ButtonPressMask | ButtonReleaseMask | KeyPressMask | KeyReleaseMask |
		VisibilityChangeMask | EnterWindowMask | LeaveWindowMask |
		PointerMotionMask | ButtonMotionMask
	);

	gc = XCreateGC(display, window, GCForeground, &gr_values);
	XMapWindow(display, window);
	return 1;
}

static int close_window(void) {

	if (window) {
		XDestroyWindow(display, window);
		XFreeGC(display, gc);
		XFlush(display);
		window = 0;
	}

	return 1;
}

static int copy_window(gx_Surf src) {
	int x = 0, y = 0;
	int w = src->width;
	int h = src->height;

	XImage image = *g_image;
	image.data = src->basePtr;
	image.width = src->width;
	image.height = src->height;
	image.bytes_per_line = src->scanLen;
	image.bits_per_pixel = src->depth;

	XPutImage(display, window, gc, &image, x, y, x, y, w, h);
	XFlush(display);
	return 0;
}

static int winmsg(int *action, int *button, int *x, int *y) {
	XEvent event;
	int result = 0;
	static int btnstate = 0;

	if (*action) {
		XPeekEvent(display, &event);
	}

	while (XPending(display)) {

		XNextEvent(display, &event);

		switch (event.type) {

			case DestroyNotify:
				*action = WINDOW_CLOSE;
				*button = 0;
				*x = 0;
				*y = 0;
				return -1;

			case ButtonPress:
				switch (event.xbutton.button) {
					case 1:
						btnstate |= 1;
						break;
					case 2:
						btnstate |= 4;
						break;
					case 3:
						btnstate |= 2;
						break;
				}
				*action = MOUSE_PRESS;
				*button = btnstate;
				*x = event.xmotion.x;
				*y = event.xmotion.y;
				break;

			case ButtonRelease:
				*action = MOUSE_RELEASE;
				*button = btnstate;
				*x = event.xmotion.x;
				*y = event.xmotion.y;
				switch (event.xbutton.button) {
					case 1:
						btnstate &= ~1;
						break;
					case 2:
						btnstate &= ~4;
						break;
					case 3:
						btnstate &= ~2;
						break;
				}
				break;

			case MotionNotify:
				*action = MOUSE_MOTION;
				*button = btnstate;
				*x = event.xmotion.x;
				*y = event.xmotion.y;
				break;

			case KeyPress:
				*action = KEY_PRESS;
				goto KeyCase;

			case KeyRelease:
				*action = KEY_RELEASE;
				goto KeyCase;

			KeyCase: {
				char buffer[1];
				KeySym keysym = XLookupKeysym(&(event.xkey), 0);
				XLookupString(&(event.xkey), buffer, sizeof(buffer), &keysym, NULL);
				*button = *buffer;
				*x = event.xkey.keycode;
				*y = 0;
				if (event.xkey.state & ShiftMask) {
					*y |= KEY_MASK_SHIFT;
				}
				if (event.xkey.state & ControlMask) {
					*y |= KEY_MASK_CONTROL;
				}
				break;
			}

			default:
				break;
		}
	}
	return result;
}

void setCaption(char *str) {
	XSetStandardProperties(display, window, str, "3d", None, NULL, 0, NULL);
}

void doneWin(void) {

	if (window)
		close_window();

	if (display)
		XCloseDisplay(display);

	if (g_image) {
		g_image->data = NULL; // static data must not be freed.
		XDestroyImage(g_image);
	}

	//~ pthread_join(thread_id, NULL);
}

int initWin(gx_Surf offs, flip_scr *flip, peek_msg *msgp) {
	/*if (XInitThreads() == 0) {
		printf("WARNING - cannot support multi threads\n");
		return -2;
	}// */

	if (offs && create_window(offs->width, offs->height)) {
		copy_window(offs);
		*flip = copy_window;
		*msgp = winmsg;
		/*if (pthread_create(&thread_id, NULL, event_thread, NULL) != 0) {
			printf("ERROR - cannot create event thread for X11\n");
			return -3;
		}// */
		return 0;
	}
	return offs ? -1 : 0;
}
