/*  Copyright 1992 John Bovey, University of Kent at Canterbury.
 *
 *  You can do what you like with this source code as long as
 *  you don't try to make money out of it and you include an
 *  unaltered copy of this message (including the copyright).
 */

char xvt_command_c_sccsid[] = "@(#)command.c	1.1 14/7/92 (UKC)";

#include <stdarg.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xauth.h>
#include <X11/keysym.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifdef _AIX
#include <termios.h>
#else
#include <sys/termios.h>
#endif

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <netdb.h>
#include <utmp.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>

#include "rvt.h"
#include "token.h"
#include "command.h"
#include "screen.h"
#include "xsetup.h"
#include "../common/common.h"

#define NLMAX	15		/* max number of lines to scroll */

#define KBUFSIZE	256	/* size of keyboard mapping buffer */
#define COM_BUF_SIZE	2048	/* size of buffer used to read from the
				 * command */
#define COM_PUSH_MAX	20	/* max number of characters that can be pushed
				 * back into the input queue */
#define MP_INTERVAL	500	/* multi-press interval in milliseconds */

/*  Special character returned by get_com_char().
 */
#define GCC_NULL	0x100	/* Input buffer is empty */
#define ESC		033

/*  Flags used to control get_com_char();
 */
#define BUF_ONLY	1
#define GET_XEVENTS	2

/*  Global variables that are set up at the beginning and then not changed
 */
extern Display *display;
extern Window vt_win;
extern Window sb_win;
extern Window main_win;

static int comm_fd = -1;	/* file descriptor connected to the command */
static int comm_pid;		/* process id if child */
static int x_fd;		/* file descriptor of the X server connection */
static int fd_width;		/* width of file descriptors being used */
static int app_cur_keys = 0;	/* flag to say cursor keys are in application
				 * mode */
static int app_kp_keys = 0;	/* flag to set application keypad keys */
static char *ttynam;
static Atom wm_del_win;

static char *send_buf = NULL;	/* characters waiting to be sent to the
				 * command */
static char *send_nxt = NULL;	/* next character to be sent */
static int send_count = 0;	/* number of characters waiting to be sent */

/*  Terminal mode structures.

static struct termios ttmode = {
	BRKINT | IGNPAR | ISTRIP | ICRNL | IXON | IMAXBEL,
	OPOST | ONLCR,
	B9600 | PARENB | CS7 | CREAD,
	ISIG | IEXTEN | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE,
	{003, 034, 0177, 025,
		004, 000, 000, 000,
		021, 023, 032, 022,
		022, 017, 027, 026
	},
	0, 0
};*/
/*  Static variables used to record interesting X events.
 */
/*  Small X event structure used to queue interesting X events that need to
 *  be converted into tokens.
 */
struct xeventst {
	int xe_type;
	int xe_button;
	int xe_state;
	int xe_x;
	int xe_y;
	int xe_width;
	int xe_height;
	int xe_detail;
	unsigned long xe_time;
	Window xe_window;
	Atom xe_property;	/* for selection requests */
	Atom xe_target;
	Window xe_requestor;	/* ditto */
	struct xeventst *xe_next;
	struct xeventst *xe_prev;
};

static struct xeventst *xevent_start = NULL;
static struct xeventst *xevent_last = NULL;	/* start and end of queue */

/*  Variables used for buffered command input.
 */
static char com_buf[COM_BUF_SIZE];
static char *com_buf_next, *com_buf_top;
static char com_stack[COM_PUSH_MAX];	/* stack of pushed back characters */
static char *com_stack_top;

static void push_xevent(struct xeventst *);
static struct xeventst *pop_xevent(void);
static void catch_child(int);
static void catch_sig(int);
static int run_command(char *, char **);
static const char *lookup_key(XEvent *, int *);
static int get_com_char(int);
static void push_com_char(int);
static void show_token_args(struct tokenst *);
static void show_hex_token_args(struct tokenst *);

/*  Send a 'Magic Cookie' authorisation string to the command.
 */
void
send_auth(void)
{
	static char hexdigits[] = "0123456789abcdef";
	char *display_name, *nptr, *dot;
	char *buf, *optr;
	Xauth *auth;
	int i, nlen, len;
	struct hostent *h;
	char hostname[64];

	display_name = DisplayString(display);

	if ((nptr = strchr(display_name, ':')) == NULL)
		return;

	if (nptr == display_name || nptr - display_name > sizeof(hostname))
		return;

	memcpy(hostname, display_name, nptr - display_name);
	hostname[nptr - display_name] = '\0';
	++nptr;

	if ((h = gethostbyname(hostname)) == NULL)
		return;
	if (h->h_addrtype != AF_INET)
		return;

	if ((dot = strchr(nptr, '.')) != NULL)
		nlen = dot - nptr;
	else
		nlen = strlen(nptr);

	auth = XauGetAuthByAddr(FamilyInternet, 4, h->h_addr_list[0],
	    nlen, nptr, 0, "");
	if (auth == NULL)
		return;

	len = 2 + 2 +
	    2 + auth->address_length +
	    2 + auth->number_length +
	    2 + auth->name_length +
	    2 + auth->data_length;

	if ((buf = (char *) cmalloc(len * 2 + 1)) == NULL) {
		XauDisposeAuth(auth);
		return;
	}
	optr = buf;

#define PUTSHORT(o, n)	  *o++ = (n >> 8) & 0xff, *o++ = n & 0xff
#define PUTBYTES(o, s, n) PUTSHORT(o, n), memcpy(o, s, n), o += n

	PUTSHORT(optr, (len - 2) * 2);
	PUTSHORT(optr, auth->family);

	PUTBYTES(optr, auth->address, auth->address_length);
	PUTBYTES(optr, auth->number, auth->number_length);
	PUTBYTES(optr, auth->name, auth->name_length);
	PUTBYTES(optr, auth->data, auth->data_length);

#undef PUTSHORT
#undef PUTBYTES

	if (optr != buf + len)
		abort();

	for (i = len - 1; i >= 0; --i) {
		buf[i * 2 + 1] = hexdigits[buf[i] & 0xf];
		buf[i * 2] = hexdigits[(buf[i] >> 4) & 0xf];
	}

	buf[len * 2] = '\n';
	send_string(buf, len * 2 + 1);

	free(buf);
	XauDisposeAuth(auth);

	return;
}
/*  Push a mini X event onto the queue
 */
static void
push_xevent(xe)
	struct xeventst *xe;
{
	xe->xe_next = xevent_start;
	xe->xe_prev = NULL;
	if (xe->xe_next != NULL)
		xe->xe_next->xe_prev = xe;
	else
		xevent_last = xe;
}

static struct xeventst *
pop_xevent(void)
{
	struct xeventst *xe;

	if (xevent_last == NULL)
		return (NULL);

	xe = xevent_last;
	xevent_last = xe->xe_prev;
	if (xevent_last != NULL)
		xevent_last->xe_next = NULL;
	else
		xevent_start = NULL;
	return (xe);
}
/*  Catch a SIGCHLD signal and exit if the direct child has died.
 */

/* ARGSUSED */
static void
catch_child(sig)
	int sig;
{
	if (wait((int *) NULL) == comm_pid)
		quit(0);
}

/*  Catch a fatal signal and tidy up before quitting
 */
static void
catch_sig(sig)
	int sig;
{
	signal(sig, SIG_DFL);
	setuid(getuid());
	kill(getpid(), sig);
}

/*  Quit with the status after first removing our entry from the utmp file.
 */
void
quit(status)
	int status;
{
	exit(status);
}

/*  Run the command in a subprocess and return a file descriptor for the
 *  master end of the pseudo-teletype pair with the command talking to
 *  the slave.
 */
static int
run_command(command, argv)
	char *command;
	char **argv;
{
	int ptyfd, ttyfd;
	int i;

	for (i = 1; i <= 15; i++)
		signal(i, catch_sig);
	signal(SIGCHLD, catch_child);
	openpty(&ptyfd, &ttyfd, ttynam, NULL, NULL);
	comm_pid = fork();
	if (comm_pid < 0) {
		error("Can't fork");
		return (-1);
	}
	if (comm_pid == 0) {
		close(ptyfd);
		login_tty(ttyfd);
		execvp(command, argv);
		error("Couldn't execute %s", command);
		quit(1);
	}
	close(ttyfd);
	return (ptyfd);
}
/*  Tell the teletype handler what size the window is.  Called after a window
 *  size change.
 */
void
tty_set_size(width, height)
	int width, height;
{
	struct winsize wsize;

	if (comm_fd < 0)
		return;
	wsize.ws_row = height;
	wsize.ws_col = width;
	ioctl(comm_fd, TIOCSWINSZ, (char *) &wsize);
}
/*  Initialise the command connection.  This should be called after the X
 *  server connection is established.
 */
void
init_command(command, argv)
	char *command;
	char **argv;
{
	/* Enable the delete window protocol. */
	wm_del_win = XInternAtom(display, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(display, main_win, &wm_del_win, 1);

	if ((comm_fd = run_command(command, argv)) < 0) {
		error("Quitting");
		quit(1);
	}
	x_fd = XConnectionNumber(display);
	fd_width = getdtablesize();
	com_buf_next = com_buf_top = com_buf;
	com_stack_top = com_stack;
}
/*  Set the current cursor keys mode.
 */
void
set_cur_keys(mode)
	int mode;
{
	app_cur_keys = (mode == HIGH);
}
/*  Set the current keypad keys mode.
 */
void
set_kp_keys(mode)
	int mode;
{
	app_kp_keys = (mode == HIGH);
}
/*  Convert the keypress event into a string.
 */
static const char *
lookup_key(ev, pcount)
	int *pcount;
	XEvent *ev;
{
	KeySym keysym;
	XComposeStatus compose;
	int count;
	static char kbuf[KBUFSIZE];
	const char *s;

	count = XLookupString(&ev->xkey, kbuf, KBUFSIZE, &keysym, &compose);
	s = NULL;
	switch (keysym) {
	case XK_Up:
		s = app_cur_keys ? "\033OA" : "\033[A";
		break;
	case XK_Down:
		s = app_cur_keys ? "\033OB" : "\033[B";
		break;
	case XK_Right:
		s = app_cur_keys ? "\033OC" : "\033[C";
		break;
	case XK_Left:
		s = app_cur_keys ? "\033OD" : "\033[D";
		break;
	case XK_KP_F1:
		s = "\033OP";
		break;
	case XK_KP_F2:
		s = "\033OQ";
		break;
	case XK_KP_F3:
		s = "\033OR";
		break;
	case XK_KP_F4:
		s = "\033OS";
		break;
	case XK_KP_0:
		s = app_kp_keys ? "\033Op" : "0";
		break;
	case XK_KP_1:
		s = app_kp_keys ? "\033Oq" : "1";
		break;
	case XK_KP_2:
		s = app_kp_keys ? "\033Or" : "2";
		break;
	case XK_KP_3:
		s = app_kp_keys ? "\033Os" : "3";
		break;
	case XK_KP_4:
		s = app_kp_keys ? "\033Ot" : "4";
		break;
	case XK_KP_5:
		s = app_kp_keys ? "\033Ou" : "5";
		break;
	case XK_KP_6:
		s = app_kp_keys ? "\033Ov" : "6";
		break;
	case XK_KP_7:
		s = app_kp_keys ? "\033Ow" : "7";
		break;
	case XK_KP_8:
		s = app_kp_keys ? "\033Ox" : "8";
		break;
	case XK_KP_9:
		s = app_kp_keys ? "\033Oy" : "9";
		break;
	case XK_KP_Subtract:
		s = app_kp_keys ? "\033Om" : "-";
		break;
	case XK_KP_Separator:
		s = app_kp_keys ? "\033Ol" : ",";
		break;
	case XK_KP_Decimal:
		s = app_kp_keys ? "\033On" : ".";
		break;
	case XK_KP_Enter:
		s = app_kp_keys ? "\033OM" : "\r";
		break;
	}
	if (s != NULL)
		*pcount = strlen(s);
	else {
		s = kbuf;
		*pcount = count;
	}
	return (s);
}
/*  Return the next input character after first passing any keyboard input
 *  to the command.  If flags & BUF_ONLY is true then only buffered characters are
 *  returned and once the buffer is empty the special value GCC_NULL is
 *  returned.  If flags and GET_XEVENTS is true then GCC_NULL is returned
 *  when an X event arrives.
 */
static int
get_com_char(flags)
	int flags;
{
	XEvent event;
	struct xeventst *xe;
	fd_set in_fdset, out_fdset;
	const char *s;
	int count, sv;

	if (com_stack_top > com_stack)
		return (*--com_stack_top);

	if (com_buf_next < com_buf_top)
		return (*com_buf_next++ & 0177);
	else if (flags & BUF_ONLY)
		return (GCC_NULL);

	for (;;) {
		FD_ZERO(&in_fdset);
		while (XPending(display) == 0) {
			if (FD_ISSET(x_fd, &in_fdset))
				/* If we get to this point something is wrong
				 * because there is X input available but no
				 * events.  Exit the program to avoid looping
				 * forever. */
				quit(0);
			FD_SET(comm_fd, &in_fdset);
			FD_SET(x_fd, &in_fdset);
			FD_ZERO(&out_fdset);
			if (send_count > 0)
				FD_SET(comm_fd, &out_fdset);
			if ((sv = select(fd_width, &in_fdset, &out_fdset, NULL, NULL)) < 0) {
				error("select failed");
				quit(-1);
			}
			if (FD_ISSET(comm_fd, &in_fdset))
				break;

			if (FD_ISSET(comm_fd, &out_fdset)) {
				count = send_count < 100 ? send_count : 100;
				count = write(comm_fd, send_nxt, count);
				if (count < 0) {
					error("failed to write to command");
					quit(-1);
				}
				send_count -= count;
				send_nxt += count;
			}
		}
		if (FD_ISSET(comm_fd, &in_fdset))
			break;
		XNextEvent(display, &event);
		if (event.type == KeyPress) {
			s = lookup_key(&event, &count);
			if (count != 0) {
				if (write(comm_fd, s, count) <= 0) {
					if (errno == EWOULDBLOCK)
						XBell(display, 0);
					else {
						error("write to pty failed");
						quit(1);
					}
				}
			}
		} else if (event.type == ClientMessage) {
			if (event.xclient.format == 32 && event.xclient.data.l[0] == wm_del_win)
				quit(0);
		} else if (event.type == MappingNotify) {
			XRefreshKeyboardMapping(&event.xmapping);
		} else if (event.type == SelectionRequest) {
			xe = (struct xeventst *) cmalloc(sizeof(struct xeventst));
			xe->xe_type = event.type;
			xe->xe_window = event.xselectionrequest.owner;
			xe->xe_time = event.xselectionrequest.time;
			xe->xe_requestor = event.xselectionrequest.requestor;
			xe->xe_target = event.xselectionrequest.target;
			xe->xe_property = event.xselectionrequest.property;
			push_xevent(xe);
			if (flags & GET_XEVENTS)
				return (GCC_NULL);
		} else if (event.type == SelectionNotify) {
			xe = (struct xeventst *) cmalloc(sizeof(struct xeventst));
			xe->xe_type = event.type;
			xe->xe_time = event.xselection.time;
			xe->xe_requestor = event.xselection.requestor;
			xe->xe_property = event.xselection.property;
			push_xevent(xe);
			if (flags & GET_XEVENTS)
				return (GCC_NULL);
		} else if (event.type == FocusIn || event.type == FocusOut) {
			if (event.xfocus.mode != NotifyNormal)
				continue;
			switch (event.xfocus.detail) {
			case NotifyAncestor:
			case NotifyInferior:
			case NotifyNonlinear:
				break;
			default:
				continue;
			}
			xe = (struct xeventst *) cmalloc(sizeof(struct xeventst));
			xe->xe_type = event.type;
			xe->xe_time = event.xselection.time;
			xe->xe_detail = event.xfocus.detail;
			push_xevent(xe);
			if (flags & GET_XEVENTS)
				return (GCC_NULL);
		} else if ((event.type == Expose || event.type == GraphicsExpose) &&
		    event.xexpose.count != 0)
			continue;
		else {
			xe = (struct xeventst *) cmalloc(sizeof(struct xeventst));
			xe->xe_type = event.type;
			xe->xe_window = event.xany.window;
			if (event.type == Expose || event.type == GraphicsExpose) {
				if (event.xexpose.count != 0)
					continue;
				xe->xe_x = event.xexpose.x;
				xe->xe_y = event.xexpose.y;
				xe->xe_width = event.xexpose.width;
				xe->xe_height = event.xexpose.height;
			} else {
				xe->xe_time = event.xbutton.time;
				xe->xe_x = event.xbutton.x;
				xe->xe_y = event.xbutton.y;
				xe->xe_state = event.xbutton.state;
				xe->xe_button = event.xbutton.button;
			}
			push_xevent(xe);
			if (flags & GET_XEVENTS)
				return (GCC_NULL);
		}
	}

	count = read(comm_fd, com_buf, COM_BUF_SIZE);
	if (count <= 0)
		return (EOF);
	com_buf_next = com_buf;
	com_buf_top = com_buf + count;
	return (*com_buf_next++ & 0177);
}
/*  Push an input character back into the input queue.
 */
static void
push_com_char(c)
	int c;
{
	if (com_stack_top < com_stack + COM_PUSH_MAX)
		*com_stack_top++ = c;
}
/*  Send count characters directly to the command.
 */
void
send_string(buf, count)
	char *buf;
	int count;
{
	char *s;
	register char *s1, *s2;
	register int i;

	if (count == 0)
		return;

	if (send_count == 0) {
		if (send_buf != NULL) {
			free(send_buf);
			send_buf = NULL;
		}
		send_buf = (char *) cmalloc(count);
		s2 = send_buf;
		s1 = buf;
		for (i = 0; i < count; i++, s1++, s2++)
			*s2 = *s1 == '\n' ? '\r' : *s1;
		send_nxt = send_buf;
		send_count = count;
	} else {
		s = (char *) cmalloc(send_count + count);
		memcpy(s, send_nxt, send_count);
		s2 = s + send_count;
		s1 = buf;
		for (i = 0; i < count; i++, s1++, s2++)
			*s2 = *s1 == '\n' ? '\r' : *s1;
		free(send_buf);
		send_buf = send_nxt = s;
		send_count += count;
	}
}

/*  Send printf formatted output to the command.  Only used for small amounts
 *  of data.
 */
/*VARARGS1*/
void
cprintf(const char *fmt, ...)
{
	va_list args;
	static char buf[1024];

	va_start(args, fmt);

	vsprintf(buf, fmt, args);
	va_end(args);
	send_string(buf, strlen(buf));
}

/*  Return an input token
 */
void
get_token(tk)
	struct tokenst *tk;
{
	int c, i, n;
	struct xeventst *xe;
	static unsigned int time1 = 0, time2 = 0;

	tk->tk_private = 0;
	tk->tk_type = TK_NULL;

	if ((xe = pop_xevent()) != NULL) {
		if (xe->xe_window == vt_win)
			tk->tk_region = SCREEN;
		else if (xe->xe_window == sb_win)
			tk->tk_region = SCROLLBAR;
		else if (xe->xe_window == main_win)
			tk->tk_region = MAINWIN;
		else
			tk->tk_region = -1;
		switch (xe->xe_type) {
		case EnterNotify:
			tk->tk_type = TK_ENTRY;
			tk->tk_arg[0] = 1;
			tk->tk_nargs = 1;
			break;
		case LeaveNotify:
			tk->tk_type = TK_ENTRY;
			tk->tk_arg[0] = 0;
			tk->tk_nargs = 1;
			break;
		case FocusIn:
			tk->tk_type = TK_FOCUS;
			tk->tk_arg[0] = 1;
			tk->tk_arg[1] = xe->xe_detail;
			tk->tk_nargs = 2;
			break;
		case FocusOut:
			tk->tk_type = TK_FOCUS;
			tk->tk_arg[0] = 0;
			tk->tk_arg[1] = xe->xe_detail;
			tk->tk_nargs = 2;
			break;
		case Expose:
			tk->tk_type = TK_EXPOSE;
			tk->tk_arg[0] = xe->xe_x;
			tk->tk_arg[1] = xe->xe_y;
			tk->tk_arg[2] = xe->xe_width;
			tk->tk_arg[3] = xe->xe_height;
			tk->tk_nargs = 4;
			break;
		case ConfigureNotify:
			tk->tk_type = TK_RESIZE;
			tk->tk_nargs = 0;
			break;
		case SelectionClear:
			tk->tk_type = TK_SELCLEAR;
			tk->tk_arg[0] = xe->xe_time;
			tk->tk_nargs = 1;
			break;
		case SelectionNotify:
			tk->tk_type = TK_SELNOTIFY;
			tk->tk_arg[0] = xe->xe_time;
			tk->tk_arg[1] = xe->xe_requestor;
			tk->tk_arg[2] = xe->xe_property;
			tk->tk_nargs = 3;
			break;
		case SelectionRequest:
			tk->tk_type = TK_SELREQUEST;
			tk->tk_arg[0] = xe->xe_time;
			tk->tk_arg[1] = xe->xe_requestor;
			tk->tk_arg[2] = xe->xe_target;
			tk->tk_arg[3] = xe->xe_property;
			tk->tk_nargs = 4;
			break;
		case ButtonPress:
			if (xe->xe_state == ControlMask) {
				tk->tk_type = TK_SBSWITCH;
				tk->tk_nargs = 0;
				break;
			}
			if (xe->xe_state == Mod5Mask) {
				switch (xe->xe_button) {
				case Button1:
					tk->tk_type = TK_SBUP;
					tk->tk_arg[0] = 300;
					tk->tk_nargs = 1;
					break;
				case Button3:
					tk->tk_type = TK_SBDOWN;
					tk->tk_arg[0] = 300;
					tk->tk_nargs = 1;
					break;
				}
			}
			if (xe->xe_window == vt_win && xe->xe_state == 0) {
				switch (xe->xe_button) {
				case Button1:
					if (xe->xe_time - time2 < MP_INTERVAL) {
						time1 = 0;
						time2 = 0;
						tk->tk_type = TK_SELLINE;
					} else if (xe->xe_time - time1 < MP_INTERVAL) {
						time2 = xe->xe_time;
						tk->tk_type = TK_SELWORD;
					} else {
						time1 = xe->xe_time;
						tk->tk_type = TK_SELSTART;
					}
					break;
				case Button2:
					tk->tk_type = TK_NULL;
					break;
				case Button3:
					tk->tk_type = TK_SELEXTND;
					break;
				}
				tk->tk_arg[0] = xe->xe_x;
				tk->tk_arg[1] = xe->xe_y;
				tk->tk_nargs = 2;
				break;
			}
			if (xe->xe_window == sb_win) {
				if (xe->xe_button == Button2) {
					tk->tk_type = TK_SBGOTO;
					tk->tk_arg[0] = xe->xe_y;
					tk->tk_nargs = 1;
				}
			}
			break;
		case ButtonRelease:
			if (xe->xe_window == sb_win) {
				switch (xe->xe_button) {
				case Button1:
					tk->tk_type = TK_SBUP;
					tk->tk_arg[0] = xe->xe_y;
					tk->tk_nargs = 1;
					break;
				case Button3:
					tk->tk_type = TK_SBDOWN;
					tk->tk_arg[0] = xe->xe_y;
					tk->tk_nargs = 1;
					break;
				}
			} else if ((xe->xe_state & ControlMask) == 0) {
				switch (xe->xe_button) {
				case Button1:
				case Button3:
					tk->tk_type = TK_SELECT;
					tk->tk_arg[0] = xe->xe_time;
					tk->tk_nargs = 1;
					break;
				case Button2:
					tk->tk_type = TK_SELINSRT;
					tk->tk_arg[0] = xe->xe_time;
					tk->tk_arg[1] = xe->xe_x;
					tk->tk_arg[2] = xe->xe_y;
					tk->tk_nargs = 3;
					break;
				}
			}
			break;
		case MotionNotify:
			if (xe->xe_window == sb_win && (xe->xe_state & Button2Mask)) {
				Window root, child;
				int root_x, root_y, x, y;
				unsigned int mods;

				XQueryPointer(display, sb_win, &root, &child,
				    &root_x, &root_y, &x, &y, &mods);
				if (mods & Button2Mask) {
					tk->tk_type = TK_SBGOTO;
					tk->tk_arg[0] = y;
					tk->tk_nargs = 1;
				}
				break;
			}
			if (xe->xe_window == vt_win && (xe->xe_state == Button1Mask)) {
				tk->tk_type = TK_SELDRAG;
				tk->tk_arg[0] = xe->xe_x;
				tk->tk_arg[1] = xe->xe_y;
				tk->tk_nargs = 2;
				break;
			}
			break;

		}
		free((char *) xe);
		return;
	}
	if ((c = get_com_char(GET_XEVENTS)) == GCC_NULL) {
		tk->tk_type = TK_NULL;
		return;
	}
	if (c == EOF) {
		tk->tk_type = TK_EOF;
		return;
	}
	if (c >= ' ' || c == '\n' || c == '\r' || c == '\t') {
		i = 0;
		tk->tk_nlcount = 0;
		do {
			tk->tk_string[i++] = c;
			c = get_com_char(1);
			if (c == '\n' && ++tk->tk_nlcount >= NLMAX) {
				tk->tk_nlcount--;
				break;
			}
		} while (!(c & ~0177) &&
		    (c >= ' ' || c == '\n' || c == '\r' || c == '\t') && i < TKS_MAX);
		tk->tk_length = i;
		tk->tk_string[i] = 0;
		tk->tk_type = TK_STRING;
		if (c != GCC_NULL)
			push_com_char(c);
	} else if (c == ESC) {
		c = get_com_char(0);
		if (c == '[') {
			c = get_com_char(0);
			if (c >= '<' && c <= '?') {
				tk->tk_private = c;
				c = get_com_char(0);
			}
			/* read any numerical arguments */
			i = 0;
			do {
				n = 0;
				while (c >= '0' && c <= '9') {
					n = n * 10 + c - '0';
					c = get_com_char(0);
				}
				if (i < TK_MAX_ARGS)
					tk->tk_arg[i++] = n;
				if (c == ESC)
					push_com_char(c);
				if (c < ' ')
					return;
				if (c < '@')
					c = get_com_char(0);
			} while (c < '@' && c >= ' ');
			if (c == ESC)
				push_com_char(c);
			if (c < ' ')
				return;
			tk->tk_nargs = i;
			tk->tk_type = c;
		} else if (c == ']') {
			c = get_com_char(0);
			n = 0;
			while (c >= '0' && c <= '9') {
				n = n * 10 + c - '0';
				c = get_com_char(0);
			}
			tk->tk_arg[0] = n;
			tk->tk_nargs = 1;
			c = get_com_char(0);
			i = 0;
			while (!(c & ~0177) && c != 7 && i < TKS_MAX) {
				if (c >= ' ')
					tk->tk_string[i++] = c;
				c = get_com_char(0);
			}
			tk->tk_length = i;
			tk->tk_string[i] = 0;
			tk->tk_type = TK_TXTPAR;
		} else if (c == '#' || c == '(' || c == ')') {
			tk->tk_type = c;
			c = get_com_char(0);
			tk->tk_arg[0] = c;
			tk->tk_nargs = 1;
		} else if (c == '7' || c == '8' || c == '=' || c == '>') {
			tk->tk_type = c;
			tk->tk_nargs = 0;
		} else {
			switch (c) {
			case 'D':
				tk->tk_type = TK_IND;
				break;
			case 'E':
				tk->tk_type = TK_NEL;
				break;
			case 'H':
				tk->tk_type = TK_HTS;
				break;
			case 'M':
				tk->tk_type = TK_RI;
				break;
			case 'N':
				tk->tk_type = TK_SS2;
				break;
			case 'O':
				tk->tk_type = TK_SS3;
				break;
			case 'Z':
				tk->tk_type = TK_DECID;
				break;
			default:
				return;
			}
		}
	} else {
		tk->tk_type = TK_CHAR;
		tk->tk_char = c;
	}
}
/*  Print out a token's numerical arguments. Just used by show_token()
 */
static void
show_token_args(tk)
	struct tokenst *tk;
{
	int i;

	for (i = 0; i < tk->tk_nargs; i++) {
		if (i == 0)
			printf(" (%d", tk->tk_arg[i]);
		else
			printf(",%d", tk->tk_arg[i]);
	}
	if (tk->tk_nargs > 0)
		printf(")");
	if (tk->tk_private != 0)
		putchar(tk->tk_private);
}
/*  Print out a token's numerical arguments in hex. Just used by show_token()
 */
static void
show_hex_token_args(tk)
	struct tokenst *tk;
{
	int i;

	for (i = 0; i < tk->tk_nargs; i++) {
		if (i == 0)
			printf(" (0x%x", tk->tk_arg[i]);
		else
			printf(",0x%x", tk->tk_arg[i]);
	}
	if (tk->tk_nargs > 0)
		printf(")");
	if (tk->tk_private != 0)
		putchar(tk->tk_private);
}
/*  Print out the contents of an input token - used for debugging.
 */
void
show_token(tk)
	struct tokenst *tk;
{

	/* Screen out token types that are not currently of interest. */
	switch (tk->tk_type) {
	case TK_SELDRAG:
		return;
	}

	switch (tk->tk_type) {
	case TK_STRING:
		printf("token(TK_STRING)");
		printf(" \"%s\"", tk->tk_string);
		break;
	case TK_TXTPAR:
		printf("token(TK_TXTPAR)");
		printf(" (%d) \"%s\"", tk->tk_arg[0], tk->tk_string);
		break;
	case TK_CHAR:
		printf("token(TK_CHAR)");
		printf(" <%o>", tk->tk_char);
		break;
	case TK_EOF:
		printf("token(TK_EOF)");
		show_token_args(tk);
		break;
	case TK_FOCUS:
		printf("token(TK_FOCUS)");
		printf(" <%d>", tk->tk_region);
		show_token_args(tk);
		break;
	case TK_ENTRY:
		printf("token(TK_ENTRY)");
		printf(" <%d>", tk->tk_region);
		show_token_args(tk);
		break;
	case TK_SBSWITCH:
		printf("token(TK_SBSWITCH)");
		show_token_args(tk);
		break;
	case TK_SBGOTO:
		printf("token(TK_SBGOTO)");
		show_token_args(tk);
		break;
	case TK_SBUP:
		printf("token(TK_SBUP)");
		show_token_args(tk);
		break;
	case TK_SBDOWN:
		printf("token(TK_SBDOWN)");
		show_token_args(tk);
		break;
	case TK_EXPOSE:
		printf("token(TK_EXPOSE)");
		printf("(%d)", tk->tk_region);
		show_token_args(tk);
		break;
	case TK_RESIZE:
		printf("token(TK_RESIZE)");
		show_token_args(tk);
		break;
	case TK_SELSTART:
		printf("token(TK_SELSTART)");
		show_token_args(tk);
		break;
	case TK_SELEXTND:
		printf("token(TK_SELEXTND)");
		show_token_args(tk);
		break;
	case TK_SELDRAG:
		printf("token(TK_SELDRAG)");
		show_token_args(tk);
		break;
	case TK_SELINSRT:
		printf("token(TK_SELINSRT)");
		show_token_args(tk);
		break;
	case TK_SELECT:
		printf("token(TK_SELECT)");
		show_token_args(tk);
		break;
	case TK_SELWORD:
		printf("token(TK_SELWORD)");
		show_token_args(tk);
		break;
	case TK_SELLINE:
		printf("token(TK_SELLINE)");
		show_token_args(tk);
		break;
	case TK_SELCLEAR:
		printf("token(TK_SELCLEAR)");
		show_token_args(tk);
		break;
	case TK_SELNOTIFY:
		printf("token(TK_SELNOTIFY)");
		show_hex_token_args(tk);
		break;
	case TK_SELREQUEST:
		printf("token(TK_SELREQUEST)");
		show_hex_token_args(tk);
		break;
	case TK_CUU:
		printf("token(TK_CUU)");
		show_token_args(tk);
		break;
	case TK_CUD:
		printf("token(TK_CUD)");
		show_token_args(tk);
		break;
	case TK_CUF:
		printf("token(TK_CUF)");
		show_token_args(tk);
		break;
	case TK_CUB:
		printf("token(TK_CUB)");
		show_token_args(tk);
		break;
	case TK_CUP:
		printf("token(TK_CUP)");
		show_token_args(tk);
		break;
	case TK_ED:
		printf("token(TK_ED)");
		show_token_args(tk);
		break;
	case TK_EL:
		printf("token(TK_EL)");
		show_token_args(tk);
		break;
	case TK_IL:
		printf("token(TK_IL)");
		show_token_args(tk);
		break;
	case TK_DL:
		printf("token(TK_DL)");
		show_token_args(tk);
		break;
	case TK_DCH:
		printf("token(TK_DCH)");
		show_token_args(tk);
		break;
	case TK_ICH:
		printf("token(TK_ICH)");
		show_token_args(tk);
		break;
	case TK_DA:
		printf("token(TK_DA)");
		show_token_args(tk);
		break;
	case TK_HVP:
		printf("token(TK_HVP)");
		show_token_args(tk);
		break;
	case TK_TBC:
		printf("token(TK_TBC)");
		show_token_args(tk);
		break;
	case TK_SET:
		printf("token(TK_SET)");
		show_token_args(tk);
		break;
	case TK_RESET:
		printf("token(TK_RESET)");
		show_token_args(tk);
		break;
	case TK_SGR:
		printf("token(TK_SGR)");
		show_token_args(tk);
		break;
	case TK_DSR:
		printf("token(TK_DSR)");
		show_token_args(tk);
		break;
	case TK_DECSTBM:
		printf("token(TK_DECSTBM)");
		show_token_args(tk);
		break;
	case TK_DECSWH:
		printf("token(TK_DECSWH)");
		show_token_args(tk);
		break;
	case TK_SCS0:
		printf("token(TK_SCS0)");
		show_token_args(tk);
		break;
	case TK_SCS1:
		printf("token(TK_SCS1)");
		show_token_args(tk);
		break;
	case TK_DECSC:
		printf("token(TK_DECSC)");
		show_token_args(tk);
		break;
	case TK_DECRC:
		printf("token(TK_DECRC)");
		show_token_args(tk);
		break;
	case TK_DECPAM:
		printf("token(TK_DECPAM)");
		show_token_args(tk);
		break;
	case TK_DECPNM:
		printf("token(TK_DECPNM)");
		show_token_args(tk);
		break;
	case TK_IND:
		printf("token(TK_IND)");
		show_token_args(tk);
		break;
	case TK_NEL:
		printf("token(TK_NEL)");
		show_token_args(tk);
		break;
	case TK_HTS:
		printf("token(TK_HTS)");
		show_token_args(tk);
		break;
	case TK_RI:
		printf("token(TK_RI)");
		show_token_args(tk);
		break;
	case TK_SS2:
		printf("token(TK_SS2)");
		show_token_args(tk);
		break;
	case TK_SS3:
		printf("token(TK_SS3)");
		show_token_args(tk);
		break;
	case TK_DECID:
		printf("token(TK_DECID)");
		show_token_args(tk);
		break;
	case TK_NULL:
		return;
	default:
		printf("unknown token <%o>", tk->tk_type);
		show_token_args(tk);
		break;
	}
	printf("\n");
}
