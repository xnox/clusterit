/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

/* @(#)command.h	1.1 14/7/92 (UKC) */

#define TKS_MAX		5000	/* max length of a string token */
#define TK_MAX_ARGS	200	/* max number of numerical arguments */

/*  Structure used to represent a piece of input from the program
 *  or an interesting X event.
 */
struct tokenst {
	int tk_type;		/* the token type */
	int tk_private;		/* non zero for private control sequences */
	int tk_char;		/* single (unprintable) character */
	char tk_string[TKS_MAX + 1];	/* the text for string tokens */
	int tk_nlcount;		/* number of newlines in the string */
	int tk_length;		/* length of string */
	int tk_arg[TK_MAX_ARGS];/* first two numerical arguments */
	int tk_nargs;		/* number of numerical arguments */
	int tk_region;		/* terminal or scrollbar */
};
void send_auth(void);
void quit(int);
void tty_set_size(int, int);
void init_command(char *, char **);
void set_cur_keys(int);
void set_kp_keys(int);
void send_string(char *, int);
void cprintf(const char *,...);
void get_token(struct tokenst *);
void show_token(struct tokenst *);
