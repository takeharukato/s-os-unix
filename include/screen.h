/*
   SWORD Emurator screen control module

   tate@spa.is.uec.ac.jp
*/

#ifndef	_SCREEN_H_
#define	_SCREEN_H_

void	scr_caps(int s);
int	scr_initx(void);
int	scr_finish(void);
void	scr_redraw(void);
void    scr_locate_cursor(int _y, int _x);

void scr_putchar(char c);
void scr_asyncputchar(char c);
void scr_sync(void);
void scr_ltnl(void);
void scr_nl(void);
void scr_puts(char *buf);
void scr_tab(int x);
int scr_getl(char *buf);
int scr_getky(void);
int scr_brkey(void);
int scr_inkey(void);
int scr_pause(void);
void scr_bell(void);
void scr_csr(int *y, int *x);
int scr_scrn(int y, int x);
void scr_loc(int y, int x);
int scr_flget(void);
void scr_width(int x);

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
#define SCR_KEYMAP_IDX_NULL       (0xff)

/* key mapping functions */
#define SCR_MAPERR_CODE	(1)
#define	SCR_MAPERR_FUNC (2)
void scr_mapclear(void);
int scr_mapadd(char code, char *funcname);
char *scr_maplook(char code);

#endif
