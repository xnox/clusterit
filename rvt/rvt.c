/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

char xvt_xvt_c_sccsid[] = "@(#)xvt.c	1.2 18/9/92 (UKC)";

#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "rvt.h"
#include "command.h"
#include "xsetup.h"
#include "screen.h"
#include "sbar.h"
#include "token.h"

extern int debugging;
extern char *username;
extern char *nodename;
extern char *remotecmd;

static int size_set = 0;	/* flag set once the window size has been set */

/*  Malloc that checks for NULL return.
 */
void *
cmalloc(size)
	int size;
{
	void *s;

	if ((s = (void *)malloc((unsigned int) size)) == NULL)
		abort();
	return (s);
}
/*  Utility function to return a malloced copy of a string.
 */
char *
scopy(str)
	char *str;
{
	char *s;

	if ((s = (char *)malloc(strlen(str) + 1)) == NULL)
		abort();
	strcpy(s, str);
	return (s);
}
/*  Run the command in a subprocess and return a file descriptor for the
 *  master end of the pseudo-teletype pair with the command talking to
 *  the slave.
 */
int
main(int argc, char *argv[])
{
	int i, n, x, y;
	int mode;
	int envc, term_set, iargc;
	char **iargv;
	struct tokenst token;
	char *title;
	char **com_argv, **com_env;
	extern char **environ;

	/* Check for a -V option. */
	for (i = 0; i < argc; i++)
		if (strcmp(argv[i], "-V") == 0) {
			printf("xvt version %s\n", VERSION);
			exit(0);
		}
	/* Make a copy of the command line argument array */
	iargv = (char **) cmalloc((argc + 1) * sizeof(char *));
	for (i = 0; i < argc; i++)
		iargv[i] = argv[i];
	iargv[i] = NULL;
	iargc = argc;

	for (i = 0; i < argc; i++)
		if (strcmp(argv[i], "-l") == 0)
			break;
	if (i < argc - 1 && argv[i+1] != NULL) {
		username = argv[i + 1];
		for (; i < argc && argv[i+2] != NULL; i++)
		   	argv[i] = argv[i+2];
		argc-=2;
		argv[argc] = NULL;
	} else {
		username = NULL;
	}

	i = argc - 1;
	if (argv[i] == NULL || i == 0) {
		usage();
		exit(1);
	}
	nodename = strdup(argv[i]);
	title = strdup(argv[i]);
	argv[i] = NULL;
	argc-=1;

	com_argv = (char **)malloc(sizeof(char *) * 5);
	if (com_argv == NULL)
		exit(1);

	remotecmd = getenv("RLOGIN_CMD");
	if (remotecmd == NULL)
		remotecmd = strdup("rsh");
	if (remotecmd == NULL)
		exit(1);
	i = 0;
	com_argv[i] = remotecmd;
	i++;
	if (username != NULL) {
		com_argv[i] = strdup("-l");
		if (com_argv[i] == NULL)
			exit(1);
		i++;
		com_argv[i] = strdup(username); i++;
	}
	com_argv[i] = strdup(nodename); i++;
	com_argv[i] = NULL;

	/* Add a TERM entry to the environment. */
	for (i = 0; environ[i] != NULL; i++);
	com_env = (char **) cmalloc((i + 2) * sizeof(char *));
	envc = i;
	term_set = 0;
	for (i = 0; i < envc; i++)
		if (strncmp(environ[i], "TERM=", 5) == 0) {
			com_env[i] = TERM_ENV;
			term_set = 1;
		} else
			com_env[i] = environ[i];
	if (!term_set)
		com_env[envc++] = TERM_ENV;
	com_env[envc] = NULL;
	environ = com_env;

	init_display(argc, argv, iargc, iargv, title);
	init_command(com_argv[0], com_argv);

	for (;;) {
		get_token(&token);
		switch (token.tk_type) {
		case TK_STRING:
			scr_string(token.tk_string, token.tk_length, token.tk_nlcount);
			break;
		case TK_CHAR:
			switch (token.tk_char) {
			case '\n':
				scr_index();
				break;
			case '\r':
				scr_move(0, 0, ROW_RELATIVE);
				break;
			case '\b':
				scr_backspace();
				break;
			case '\t':
				scr_tab();
				break;
			case '\007':	/* bell */
				scr_bell();
				break;
			}
			break;
		case TK_EOF:
			quit(0);
			break;
		case TK_ENTRY:	/* keyboard focus changed */
			scr_focus(1, token.tk_arg[0]);
			break;
		case TK_FOCUS:
			scr_focus(2, token.tk_arg[0]);
			break;
		case TK_EXPOSE:/* window exposed */
			if (!size_set) {

				/* Force a window resize if an exposure event
				 * arrives before the first resize event. */
				resize_window();
				size_set = 1;
			}
			switch (token.tk_region) {
			case SCREEN:
				scr_reset();
				break;
			case SCROLLBAR:
				sbar_reset();
				break;
			}
			break;
		case TK_RESIZE:
			resize_window();
			size_set = 1;
			break;
		case TK_TXTPAR:/* change title or icon name */
			switch (token.tk_arg[0]) {
			case 0:
				change_window_name(token.tk_string);
				change_icon_name(token.tk_string);
				break;
			case 1:
				change_icon_name(token.tk_string);
				break;
			case 2:
				change_window_name(token.tk_string);
				break;
			}
			break;
		case TK_SBSWITCH:
			switch_scrollbar();
			break;
		case TK_SBGOTO:
			scr_move_to(token.tk_arg[0]);
			break;
		case TK_SBUP:
			scr_move_by(token.tk_arg[0]);
			break;
		case TK_SBDOWN:
			scr_move_by(-token.tk_arg[0]);
			break;
		case TK_SELSTART:
			scr_start_selection(token.tk_arg[0], token.tk_arg[1], CHAR);
			break;
		case TK_SELEXTND:
			scr_extend_selection(token.tk_arg[0], token.tk_arg[1], 0);
			break;
		case TK_SELDRAG:
			scr_extend_selection(token.tk_arg[0], token.tk_arg[1], 1);
			break;
		case TK_SELWORD:
			scr_start_selection(token.tk_arg[0], token.tk_arg[1], WORD);
			break;
		case TK_SELLINE:
			scr_start_selection(token.tk_arg[0], token.tk_arg[1], LINE);
			break;
		case TK_SELECT:
			scr_make_selection(token.tk_arg[0]);
			break;
		case TK_SELCLEAR:
			scr_clear_selection();
			break;
		case TK_SELREQUEST:
			scr_send_selection(token.tk_arg[0], token.tk_arg[1],
			    token.tk_arg[2], token.tk_arg[3]);
			break;
		case TK_SELINSRT:
			scr_request_selection(token.tk_arg[0], token.tk_arg[1], token.tk_arg[2]);
			break;
		case TK_SELNOTIFY:
			scr_paste_primary(token.tk_arg[0], token.tk_arg[1], token.tk_arg[2]);
			break;
		case TK_CUU:	/* cursor up */
			n = token.tk_arg[0];
			n = n == 0 ? -1 : -n;
			scr_move(0, n, ROW_RELATIVE | COL_RELATIVE);
			break;
		case TK_CUD:	/* cursor down */
			n = token.tk_arg[0];
			n = n == 0 ? 1 : n;
			scr_move(0, n, ROW_RELATIVE | COL_RELATIVE);
			break;
		case TK_CUF:	/* cursor forward */
			n = token.tk_arg[0];
			n = n == 0 ? 1 : n;
			scr_move(n, 0, ROW_RELATIVE | COL_RELATIVE);
			break;
		case TK_CUB:	/* cursor back */
			n = token.tk_arg[0];
			n = n == 0 ? -1 : -n;
			scr_move(n, 0, ROW_RELATIVE | COL_RELATIVE);
			break;
		case TK_HVP:
		case TK_CUP:	/* position cursor */
			if (token.tk_nargs == 1)
				if (token.tk_arg[0] == 0) {
					x = 0;
					y = 0;
				} else {
					x = 0;
					y = token.tk_arg[0] - 1;
				}
			else {
				y = token.tk_arg[0] - 1;
				x = token.tk_arg[1] - 1;
			}
			scr_move(x, y, 0);
			break;
		case TK_ED:
			scr_erase_screen(token.tk_arg[0]);
			break;
		case TK_EL:
			scr_erase_line(token.tk_arg[0]);
			break;
		case TK_IL:
			n = token.tk_arg[0];
			if (n == 0)
				n = 1;
			scr_insert_lines(n);
			break;
		case TK_DL:
			n = token.tk_arg[0];
			if (n == 0)
				n = 1;
			scr_delete_lines(n);
			break;
		case TK_DCH:
			n = token.tk_arg[0];
			if (n == 0)
				n = 1;
			scr_delete_characters(n);
			break;
		case TK_ICH:
			n = token.tk_arg[0];
			if (n == 0)
				n = 1;
			scr_insert_characters(n);
			break;
		case TK_DA:
			cprintf("\033[?6c");	/* I am a VT102 */
			break;
		case TK_TBC:
			break;
		case TK_SET:
		case TK_RESET:
			mode = (token.tk_type == TK_SET) ? HIGH : LOW;
			if (token.tk_private == '?') {
				switch (token.tk_arg[0]) {
				case 1:
					set_cur_keys(mode);
					break;
				case 6:
					scr_set_decom(mode);
					break;
				case 7:
					scr_set_wrap(mode);
					break;
				case 47:	/* switch to main screen */
					scr_change_screen(mode);
					break;
				}
			} else if (token.tk_private == 0) {
				switch (token.tk_arg[0]) {
				case 4:
					scr_set_insert(mode);
					break;
				}
			}
			break;
		case TK_SGR:
			if (token.tk_nargs == 0)
				scr_change_rendition(RS_NONE);
			else {
				for (i = 0; i < token.tk_nargs; i++) {
					switch (token.tk_arg[i]) {
					case 0:
						scr_change_rendition(RS_NONE);
						break;
					case 1:
						scr_change_rendition(RS_BOLD);
						break;
					case 4:
						scr_change_rendition(RS_ULINE);
						break;
					case 5:
						scr_change_rendition(RS_BLINK);
						break;
					case 7:
						scr_change_rendition(RS_RVID);
						break;
					}
				}
			}
			break;
		case TK_DSR:	/* request for information */
			switch (token.tk_arg[0]) {
			case 6:
				scr_report_position();
				break;
			case 7:/* display name */
				scr_report_display();
				break;
			case 8:/* send magic cookie */
				send_auth();
				break;
			}
			break;
		case TK_DECSTBM:	/* set top and bottom margins */
			if (token.tk_nargs < 2 || token.tk_arg[0] >= token.tk_arg[1])
				scr_set_margins(0, 10000);
			else
				scr_set_margins(token.tk_arg[0] - 1, token.tk_arg[1] - 1);
			break;
		case TK_DECSWH:/* ESC # digit */
			if (token.tk_arg[0] == '8')
				scr_efill();	/* fill screen with Es */
			break;
		case TK_SCS0:
			break;
		case TK_SCS1:
			break;
		case TK_DECSC:
			scr_save_cursor();
			break;
		case TK_DECRC:
			scr_restore_cursor();
			break;
		case TK_DECPAM:
			set_kp_keys(HIGH);
			break;
		case TK_DECPNM:
			set_kp_keys(LOW);
			break;
		case TK_IND:	/* Index (same as \n) */
			scr_index();
			break;
		case TK_NEL:
			break;
		case TK_HTS:
			break;
		case TK_RI:	/* Reverse index */
			scr_rindex();
			break;
		case TK_SS2:
			break;
		case TK_SS3:
			break;
		case TK_DECID:
			cprintf("\033[?6c");	/* I am a VT102 */
			break;
		}
		if (debugging)
			show_token(&token);
	}
}
