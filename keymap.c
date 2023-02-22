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
    {'S', "back"},
    {'D', "forward"},
    {'E', "up"},
    {'X', "down"},
    {'J', "redraw"},
    {'G', "delete"},
    {'B', "backspace"},
    {'I', "tab"},
    {'H', "backspace"},
    {'O', "imode"},
    {'Q', "clear"},
    {'\0', NULL}
#else
    {'H', "backspace"},
    {'D', "delete"},
    {'A', "begin"},
    {'E', "end"},
    {'P', "up"},
    {'N', "down"},
    {'F', "forward"},
    {'B', "back"},
    {'L', "redraw"},
    {'K', "kill"},
    {'I', "tab"},
    {'Y', "yank"},
    {'O', "imode"},
    {'Q', "clear"},
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
