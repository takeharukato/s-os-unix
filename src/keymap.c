/*
   SWORD Emulator keymap module
   tate@spa.is.uec.ac.jp
*/

#include "config.h"

#include <stdlib.h>
#include "screen.h"
#include "keymap.h"
/*
   define default keymap

   XX: multiple keymaps should be regsterd once, and
       be selected by user.
*/
static struct map {
    int	code;		/* code + '@' */
    char *funcname;
} defaultmap[] = {
#ifdef OPT_KEYMAP_WM
    {'S', KEYMAP_NAME_BACK},
    {'D', KEYMAP_NAME_FWD},
    {'E', KEYMAP_NAME_UP},
    {'X', KEYMAP_NAME_DOWN},
    {'J', KEYMAP_NAME_REDRAW},
    {'G', KEYMAP_NAME_DELETE},
    {'B', KEYMAP_NAME_BACKSPACE},
    {'I', KEYMAP_NAME_TAB},
    {'H', KEYMAP_NAME_BACKSPACE},
    {'O', KEYMAP_NAME_IMODE},
    {'Q', KEYMAP_NAME_CLEAR},
    {'C', KEYMAP_NAME_BREAK},
    {'\0', NULL}
#else
    {'H', KEYMAP_NAME_BACKSPACE},
    {'D', KEYMAP_NAME_DELETE},
    {'A', KEYMAP_NAME_BEGIN},
    {'E', KEYMAP_NAME_END},
    {'P', KEYMAP_NAME_UP},
    {'N', KEYMAP_NAME_DOWN},
    {'F', KEYMAP_NAME_FWD},
    {'B', KEYMAP_NAME_BACK},
    {'L', KEYMAP_NAME_REDRAW},
    {'K', KEYMAP_NAME_KILL},
    {'I', KEYMAP_NAME_TAB},
    {'Y', KEYMAP_NAME_YANK},
    {'O', KEYMAP_NAME_IMODE},
    {'Q', KEYMAP_NAME_CLEAR},
    {'C', KEYMAP_NAME_BREAK},
    {'\0', NULL}
#endif
};

void
setdefaultkeymap(void){
    struct map *mp;

    mp = defaultmap;
    while(mp->code != '\0'){
	if (scr_mapadd(mp->code - '@', mp->funcname)){
	    scr_puts("error in default map\r");
	}
	mp++;
    }
}
