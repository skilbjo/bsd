# include	"curses.ext"

/*
 *	This routine clears the window.
 *
 * @(#)clear.c	1.2 (Berkeley) 05/01/85
 */
wclear(win)
reg WINDOW	*win; {

	werase(win);
	win->_clear = TRUE;
	return OK;
}
