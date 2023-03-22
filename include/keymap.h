/*
   SWORD Emulator keymap header
*/

#ifndef _KEYMAP_H_
#define _KEYMAP_H_

/*
 * keymap search key strings
 */
#define KEYMAP_NAME_BACKSPACE "backspace"
#define KEYMAP_NAME_DELETE    "delete"
#define KEYMAP_NAME_BEGIN     "begin"
#define KEYMAP_NAME_END       "end"
#define KEYMAP_NAME_UP        "up"
#define KEYMAP_NAME_DOWN      "down"
#define KEYMAP_NAME_FWD       "forward"
#define KEYMAP_NAME_BACK      "back"
#define KEYMAP_NAME_REDRAW    "redraw"
#define KEYMAP_NAME_KILL      "kill"
#define KEYMAP_NAME_TAB       "tab"
#define KEYMAP_NAME_YANK      "yank"
#define KEYMAP_NAME_IMODE     "imode"
#define KEYMAP_NAME_CLEAR     "clear"
#define KEYMAP_NAME_BREAK     "break"

/*
 * proto types
 */
void setdefaultkeymap(void);
#endif  /*  _KEYMAP_H_  */
