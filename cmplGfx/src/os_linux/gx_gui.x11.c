#include "../gx_gui.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdlib.h>


struct GxWindow {
	Display *display;
	Window window;
	XImage *image;

	Atom deleteWindow;
	XGCValues gr_values;
	GC gc;
};

GxWindow createWindow(GxImage offs, const char *title) {
	if (offs == NULL) {
		return NULL;
	}

	GxWindow result = malloc(sizeof(struct GxWindow));
	if (result == NULL) {
		return NULL;
	}

	result->display = XOpenDisplay(NULL);
	int screen = DefaultScreen(result->display);
	int depth = DefaultDepth(result->display, screen);

	result->window = XCreateSimpleWindow(result->display, XDefaultRootWindow(result->display), 0, 0, offs->width, offs->height, 0, BlackPixel(result->display, 0), WhitePixel(result->display, 0));
	result->image = XCreateImage(result->display, CopyFromParent, depth, ZPixmap, 0, offs->basePtr, offs->width, offs->height, 32, offs->scanLen);
	result->gc = XCreateGC(result->display, result->window, GCForeground, &result->gr_values);
	XSetStandardProperties(result->display, result->window, title, "cmpl", None, NULL, 0, NULL);
	XMapWindow(result->display, result->window);

	XSelectInput(result->display, result->window, StructureNotifyMask// | VisibilityChangeMask
		| ButtonPressMask | ButtonReleaseMask | ButtonMotionMask
		| EnterWindowMask | LeaveWindowMask   // mouse enter and leave the window
		| KeyPressMask | KeyReleaseMask
	);

	// handle window close event
	result->deleteWindow = XInternAtom(result->display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(result->display, result->window, &result->deleteWindow, 1);

	// window is not resizeable
	XSizeHints *xsh = XAllocSizeHints();
	XWindowAttributes xwa;
	xsh->flags = PMinSize | PMaxSize;
	XGetWindowAttributes(result->display, result->window, &xwa);
	xsh->min_width = xwa.width;
	xsh->max_width = xwa.width;
	xsh->min_height = xwa.height;
	xsh->max_height = xwa.height;
	XSetWMNormalHints(result->display, result->window, xsh);
	XFree(xsh);

	return result;
}

int getWindowEvent(GxWindow window, int *button, int *x, int *y) {
	static int btnstate = 0;

	XEvent event;
	event.type = 0;
	while (XPending(window->display)) {
		XNextEvent(window->display, &event);
		if (event.type != MotionNotify) {
			// consume mouse motion events
			break;
		}
	}
	*button = *x = *y = 0;
	switch (event.type) {
		default:
			*button = event.type;
			break;

		case ClientMessage:
			if ((Atom)event.xclient.data.l[0] == window->deleteWindow) {
				return WINDOW_CLOSE;
			}
			break;

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
			KeySym keysym;
			char buffer[8];
			XLookupString(&(event.xkey), buffer, sizeof(buffer), &keysym, NULL);
			*button = buffer[0];
			*x = event.xkey.keycode;
			*y = keysym & ~255;
			if (event.xkey.state & ShiftMask) {
				*y |= KEY_MASK_SHIFT;
			}
			if (event.xkey.state & ControlMask) {
				*y |= KEY_MASK_CTRL;
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

void setWindowTitle(GxWindow window, const char *title) {
	XSetStandardProperties(window->display, window->window, title, "cmpl", None, NULL, 0, NULL);
}

void flushWindow(GxWindow window) {
	XImage *screen = window->image;
	XPutImage(window->display, window->window, window->gc, screen, 0, 0, 0, 0, screen->width, screen->height);
	XFlush(window->display);
}

void destroyWindow(GxWindow window) {

	if (window->image) {
		window->image->data = NULL;
		XDestroyImage(window->image);
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
