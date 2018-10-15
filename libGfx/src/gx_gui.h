#include "gx_surf.h"

#define KEY_PRESS 1
#define KEY_RELEASE 2
#define MOUSE_PRESS 3
#define MOUSE_RELEASE 4
#define MOUSE_MOTION 5
#define WINDOW_CLOSE 6

#define KEY_MASK_SHIFT 1
#define KEY_MASK_CONTROL 2


typedef int (*peek_msg)(int32_t *action, int32_t *button, int32_t *x, int32_t *y);
typedef int (*flip_scr)(gx_Surf);

extern int initWin(gx_Surf offs, flip_scr *flip, peek_msg *msgp);
extern void doneWin();

extern void setCaption(char* str);
