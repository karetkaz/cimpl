#include "gx_surf.h"

#define KEY_PRESS 1
#define KEY_RELEASE 2
#define MOUSE_PRESS 3
#define MOUSE_RELEASE 4
#define MOUSE_MOTION 5
#define EVENT_TIMEOUT 6
#define WINDOW_CLOSE 100
#define WINDOW_ENTER 101
#define WINDOW_LEAVE 102

#define KEY_MASK_SHIFT 1
#define KEY_MASK_CONTROL 2

typedef struct gxWindow *gxWindow;

extern gxWindow createWindow(gx_Surf offs);

extern int getWindowEvent(gxWindow window, int *button, int *x, int *y);

extern void setWindowText(gxWindow window, char *caption);

extern void flushWindow(gxWindow window);

extern void destroyWindow(gxWindow window);

// TODO: find a common place and merge platform specific implementations
uint64_t timeMillis();
void sleepMillis(int64_t millis);
void parkThread();
