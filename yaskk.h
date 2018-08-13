#define _XOPEN_SOURCE 600
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <locale.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <inttypes.h>
#include <execinfo.h>

#define SIGWINCH 28

enum {
	BUFSIZE        = 1024,
	MAX_ARGS       = 256,
	INPUT_THRESHLD = 6,
	SELECT_TIMEOUT = 15000,
	SLEEP_TIME     = 30000,
	NHASH          = 512,
	INIT_ENTRY     = 8,
	KEYSIZE        = 64,
	LBUFSIZE       = 1024,
	MAX_CELLS      = 16,
	UTF8_LEN_MAX   = 3,
	LINE_LEN_MAX   = UTF8_LEN_MAX * MAX_CELLS + 1,
};

enum ctrl_code {
	/* 7 bit */
	BEL = 0x07, BS  = 0x08, HT  = 0x09,
	LF  = 0x0A, VT  = 0x0B, FF  = 0x0C,
	CR  = 0x0D, ESC = 0x1B, DEL = 0x7F,
	/* others */
	SPACE     = 0x20,
	BACKSLASH = 0x5C,
};

enum key_name_t {
	CTRL_AT = 0,
	CTRL_A, CTRL_B, CTRL_C, CTRL_D,
	CTRL_E, CTRL_F, CTRL_G, CTRL_H,
	CTRL_I, CTRL_J, CTRL_K, CTRL_L,
	CTRL_M, CTRL_N, CTRL_O, CTRL_P,
	CTRL_Q, CTRL_R, CTRL_S, CTRL_T,
	CTRL_U, CTRL_V, CTRL_W, CTRL_X,
	CTRL_Y, CTRL_Z,
	CTRL_BRACE_L, CTRL_BACKSLASH,
	CTRL_BRACE_R, CTRL_HAT, CTRL_UNDERBAR,
};

enum skk_mode_t {
	MODE_ASCII = 0,
	MODE_CURSIVE,
	MODE_SQUARE,
	MODE_COOK,
	MODE_APPEND,
	MODE_SELECT,
};

const char *mode2str[] = {
	[MODE_ASCII]   = "MODE_ASCII",
	[MODE_CURSIVE] = "MODE_CURSIVE",
	[MODE_SQUARE]  = "MODE_SQUARE",
	[MODE_COOK]    = "MODE_COOK",
	[MODE_APPEND]  = "MODE_APPEND",
	[MODE_SELECT]  = "MODE_SELECT",
};

/* struct for parse_args() */
struct args_t {
	char *buf;         /* buffer of dictionary entry (include composition word and candidate words) */
	int len;           /* number of candidate */
	char *v[MAX_ARGS]; /* pointer to each candidate */
};

/* struct for entry of dictionary */
struct entry_t {
	char *word;         /* composition word */
	struct args_t args; /* candidate words */
};

struct dict_t {
	struct entry_t *entry; /* dynamic array of entries of dictionary */
	int size;              /* num of real entry size */
	int mem_size;          /* num of allocated entry size */
};

/* struct for line edit */
struct line_t {
	uint32_t cells[MAX_CELLS]; /* array of UCS2 codepoint */
	struct cursor_t {
		int insert;              /* character insert position */
		int preedit;             /* position of first preedit character */
	} cursor;
};

/* for line edit */
struct edit_t {
	int fd;                /* master of pseudo terminal */
	struct line_t current; /* current line status */
	struct line_t next;    /* next line status */
	bool need_clear;       /* output all characters and clear line buffer */
};

/* struct for skk */
struct skk_t {
	int mode;              /* skk mode */
	/* for candidate */
	struct args_t *select; /* matched candidate */
	int index;             /* index of selected candidate currently */
	/* saved string */
	/*
	 * in select mode:
	 *   cook: keyword of dict/hash lookup and restored string after select mode
	 */
	char restore[LINE_LEN_MAX];
	/*
	 * in append mode:
	 *   cook + append[0]              : keyword of dict/hash lookup
	 *   cook + append[1] ... append[n]: restored string after append -> cook mode
	 */
	struct append_t {
		int pos;                /* position of MARK_APPEND */
		char ch;                /* cook + append.ch : keyword of dict/hash lookup */
		char str[LINE_LEN_MAX]; /* cook + append.str: restored string after append -> cook mode */
	} append;
};

/* global */
volatile sig_atomic_t child_is_alive = true;
volatile sig_atomic_t window_resized = false;
