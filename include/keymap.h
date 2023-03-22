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
 * keymap index
 */
#define SCR_KEYMAP_IDX_BACKSPACE  (0x00)
#define SCR_KEYMAP_IDX_DELETE     (0x01)
#define SCR_KEYMAP_IDX_BEGIN      (0x02)
#define SCR_KEYMAP_IDX_END        (0x03)
#define SCR_KEYMAP_IDX_UP         (0x04)
#define SCR_KEYMAP_IDX_DOWN       (0x05)
#define SCR_KEYMAP_IDX_FWD        (0x06)
#define SCR_KEYMAP_IDX_BWD        (0x07)
#define SCR_KEYMAP_IDX_REDRAW     (0x08)
#define SCR_KEYMAP_IDX_KILL       (0x09)
#define SCR_KEYMAP_IDX_TAB        (0x0a)
#define SCR_KEYMAP_IDX_YANK       (0x0b)
#define SCR_KEYMAP_IDX_IMODE      (0x0c)
#define SCR_KEYMAP_IDX_CLEAR      (0x0d)
#define SCR_KEYMAP_IDX_BREAK      (0x0e)
#define SCR_KEYMAP_IDX_NULL       (0xff)

/*
 * proto types
 */
void setdefaultkeymap(void);
#endif  /*  _KEYMAP_H_  */
