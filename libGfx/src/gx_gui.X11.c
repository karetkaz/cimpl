#include "gx_gui.h"
#include <sched.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <time.h>

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
	XSelectInput(display, window, StructureNotifyMask// | VisibilityChangeMask
		| ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
		| KeyPressMask | KeyReleaseMask
		//| EnterWindowMask | LeaveWindowMask	// 
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

static int winmsg(int timeout, int *button, int *x, int *y) {
	static int btnstate = 0;
	XEvent event;

	if (timeout > 0) {
		struct timeval tv;
		struct timespec ts;

		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		gettimeofday(&tv, NULL);
		uint64_t start = tv.tv_sec * (uint64_t) 1000 + tv.tv_usec / 1000;
		while (!XPending(display)) {
			gettimeofday(&tv, NULL);
			uint64_t now = tv.tv_sec * (uint64_t) 1000 + tv.tv_usec / 1000;
			if (now - start > timeout) {
				*button = 0;
				*x = *y = 0;
				return EVENT_TIMEOUT;
			}
			nanosleep(&ts, NULL);
			// FIXME: sched_yield();
		}
	}

	event.type = 0;
	while (timeout == 0 || XPending(display)) {
		XNextEvent(display, &event);
		timeout = 1;
		if (event.type != MotionNotify) {
			// consume MotionNotify events
			break;
		}
	}
	*button = 0;
	*x = *y = 0;
	switch (event.type) {
		default:
			*button = event.type;
			break;

		case CreateNotify:
			return WINDOW_CREATE;

		case DestroyNotify:
			return WINDOW_CLOSE;

		case EnterNotify:
			return WINDOW_ENTER;

		case LeaveNotify:
			return WINDOW_LEAVE;

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
			*button = btnstate;
			*x = event.xmotion.x;
			*y = event.xmotion.y;
			return MOUSE_PRESS;

		case ButtonRelease:
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
			return MOUSE_RELEASE;

		case MotionNotify:
			*button = btnstate;
			*x = event.xmotion.x;
			*y = event.xmotion.y;
			return MOUSE_MOTION;

		case KeyPress:
		case KeyRelease: {
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
			switch (event.type) {
				case KeyPress:
					return KEY_PRESS;

				case KeyRelease:
					return KEY_RELEASE;
			}
			break;
		}
	}
	return 0;
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
