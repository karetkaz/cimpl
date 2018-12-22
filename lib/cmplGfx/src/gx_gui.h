#include "gx_surf.h"

#define KEY_PRESS 1
#define KEY_RELEASE 2
#define MOUSE_PRESS 3
#define MOUSE_RELEASE 4
#define MOUSE_MOTION 5
#define EVENT_TIMEOUT 6
#define WINDOW_CREATE 100
#define WINDOW_CLOSE 101
#define WINDOW_ENTER 102
#define WINDOW_LEAVE 103

#define KEY_MASK_SHIFT 1
#define KEY_MASK_CONTROL 2

typedef struct gxWindow *gxWindow;

extern gxWindow createWindow(gx_Surf offs);

extern int getWindowEvent(gxWindow window, int timeout, int *button, int *x, int *y);

extern void setWindowText(gxWindow window, char *caption);

extern void flushWindow(gxWindow window);

extern void destroyWindow(gxWindow window);
