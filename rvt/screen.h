/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

/* @(#)screen.h	1.1 14/7/92 (UKC) */

/*  flags for scr_move()
 */
#define COL_RELATIVE	1	/* column movement is relative */
#define ROW_RELATIVE	2	/* row movement is relative */

#define MAX_SCROLL	50	/* max number of lines that can scroll at once */

/*  arguments to the screen delete functions
 */
#define END	0
#define START	1
#define ENTIRE	2

/*  rendition style flags.
 */
#define RS_NONE		0	/* Normal */
#define RS_BOLD		1	/* Bold face */
#define RS_ULINE	2	/* underline */
#define RS_BLINK	4	/* blinking */
#define RS_RVID		8	/* reverse video */

/*  The current selection unit
 */
enum selunit {
	CHAR,
	WORD,
	LINE
};
void scr_init(int);
void scr_reset(void);
void scr_char_class(char *);
void scr_backspace(void);
void scr_bell(void);
void scr_change_screen(int);
void scr_change_rendition(int);
void scr_get_size(int *, int *);
void scr_focus(int, int);
void scr_string(char *, int, int);
void scr_move(int, int, int);
void scr_index(void);
void scr_rindex(void);
void scr_save_cursor(void);
void scr_restore_cursor(void);
void scr_erase_line(int);
void scr_erase_screen(int);
void scr_delete_lines(int);
void scr_insert_lines(int);
void scr_delete_characters(int);
void scr_insert_characters(int);
void scr_tab(void);
void scr_set_margins(int, int);
void scr_set_wrap(int);
void scr_set_decom(int);
void scr_set_insert(int);
void scr_efill(void);
void scr_move_to(int);
void scr_move_by(int);
void scr_make_selection(int);
void scr_send_selection(int, int, int, int);
void scr_request_selection(int, int, int);
void scr_paste_primary(int, int, int);
void scr_clear_selection(void);
void scr_extend_selection(int, int, int);
void scr_start_selection(int, int, enum selunit);
void scr_report_display(void);
void scr_report_position(void);
