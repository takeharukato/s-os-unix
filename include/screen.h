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

/* key mapping functions */
#define SCR_MAPERR_CODE	(1)
#define	SCR_MAPERR_FUNC (2)
void scr_mapclear(void);
int scr_mapadd(char code, char *funcname);
char *scr_maplook(char code);

#endif
