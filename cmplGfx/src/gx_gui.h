#include "gx_surf.h"

#define KEY_PRESS 1
#define KEY_RELEASE 2
#define MOUSE_WHEEL 3
#define MOUSE_PRESS 4
#define MOUSE_MOTION 5
#define MOUSE_RELEASE 6
#define FINGER_PRESS 7
#define FINGER_MOTION 8
#define FINGER_RELEASE 9
#define EVENT_TIMEOUT 10
#define WINDOW_INIT 100
#define WINDOW_CLOSE 102
#define WINDOW_ENTER 103
#define WINDOW_LEAVE 104

#define KEY_MASK_SHIFT 1
#define KEY_MASK_CTRL 2
#define KEY_MASK_ALT 4

typedef struct GxWindow *GxWindow;

extern GxWindow createWindow(GxImage image, const char *title);

extern int getWindowEvent(GxWindow window, int *button, int *x, int *y);

extern void setWindowTitle(GxWindow window, const char *title);

extern void flushWindow(GxWindow window);

extern void destroyWindow(GxWindow window);

// TODO: find a common place and merge platform specific implementations
uint64_t timeMillis();
void sleepMillis(int64_t millis);
void parkThread();
