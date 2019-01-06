#include "../gx_gui.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>


struct gxWindow {
	gx_Surf image;

	Display *display;
	Window window;
	XImage *g_image;

	Atom deleteWindow;
	XGCValues gr_values;
	GC gc;
};

gxWindow createWindow(gx_Surf offs) {
	if (offs == NULL) {
		return NULL;
	}

	gxWindow result = malloc(sizeof(struct gxWindow));
	if (result == NULL) {
		return NULL;
	}

	result->display = XOpenDisplay(NULL);
	int screen = DefaultScreen(result->display);
	int depth = DefaultDepth(result->display, screen);

	result->g_image = XCreateImage(result->display, CopyFromParent, depth, ZPixmap, 0, NULL, offs->width, offs->height, 32, 0);
	result->window = XCreateSimpleWindow(result->display, XDefaultRootWindow(result->display), 0, 0, offs->width, offs->height, 0, BlackPixel(result->display, 0), WhitePixel(result->display, 0));

	XSelectInput(result->display, result->window, StructureNotifyMask// | VisibilityChangeMask
		| ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
		| EnterWindowMask | LeaveWindowMask   // mouse enter and leave the window
		| KeyPressMask | KeyReleaseMask
	);

	result->gc = XCreateGC(result->display, result->window, GCForeground, &result->gr_values);
	XMapWindow(result->display, result->window);

	result->deleteWindow = XInternAtom(result->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(result->display, result->window, &result->deleteWindow, 1);

	result->image = offs;

	return result;
}

int getWindowEvent(gxWindow window, int timeout, int *button, int *x, int *y) {
	static int btnstate = 0;
	XEvent event;

	if (timeout > 0) {
		struct timeval tv;
		struct timespec ts;

		ts.tv_sec = 0;
		ts.tv_nsec = 0;
		gettimeofday(&tv, NULL);
		uint64_t start = tv.tv_sec * (uint64_t) 1000 + tv.tv_usec / 1000;
		while (!XPending(window->display)) {
			gettimeofday(&tv, NULL);
			uint64_t now = tv.tv_sec * (uint64_t) 1000 + tv.tv_usec / 1000;
			if (now - start > (uint64_t) timeout) {
				*button = 0;
				*x = *y = 0;
				return EVENT_TIMEOUT;
			}
			nanosleep(&ts, NULL);
			// FIXME: sched_yield();
		}
	}

	event.type = 0;
	while (timeout == 0 || XPending(window->display)) {
		XNextEvent(window->display, &event);
		if (event.type != MotionNotify) {
			// consume mouse motion events
			break;
		}
		timeout = 1;
	}
	*button = 0;
	*x = *y = 0;
	switch (event.type) {
		default:
			*button = event.type;
			break;

		case ClientMessage:
			if (event.xclient.data.l[0] == window->deleteWindow) {
				return WINDOW_CLOSE;
			}
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

void setWindowText(gxWindow window, char *caption) {
	XSetStandardProperties(window->display, window->window, caption, "3d", None, NULL, 0, NULL);
}

void flushWindow(gxWindow window) {
	gx_Surf src = window->image;
	XImage *dst = window->g_image;

	dst->data = src->basePtr;
	dst->width = src->width;
	dst->height = src->height;
	dst->bits_per_pixel = src->depth;
	dst->bytes_per_line = src->scanLen;

	XPutImage(window->display, window->window, window->gc, dst, 0, 0, 0, 0, src->width, src->height);
	XFlush(window->display);
}

void destroyWindow(gxWindow window) {

	if (window->g_image) {
		window->g_image->data = NULL;
		XDestroyImage(window->g_image);
	}

	if (window->window) {
		XDestroyWindow(window->display, window->window);
		XFreeGC(window->display, window->gc);
		XFlush(window->display);
		window->window = 0;
	}

	if (window->display) {
		XCloseDisplay(window->display);
	}

	free(window);
}
