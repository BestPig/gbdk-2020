/* Unix */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#ifdef __WIN32__
#include <windows.h>
#endif

#ifndef GBDKLIBDIR
#define GBDKLIBDIR "\\gbdk\\"
#endif

extern char *progname;

typedef struct {
	const char *port;
	const char *plat;
	const char *default_plat;
	const char *cpp;
	const char *include;
	const char *com;
	const char *as;
	const char *bankpack;
	const char *ld;
	const char *ihxcheck;
	const char *mkbin;
} CLASS;

static struct {
	const char *name;
	const char *val;
} _tokens[] = {
		{ "port",		"gbz80" },
		{ "plat",		"gb" },
		{ "sdccdir", "%bindir%"},
		{ "cpp",		"%sdccdir%sdcpp" },
		{ "cppdefault", 	"-Wall -DSDCC=1 -DSDCC_PORT=%port% "
			"-DSDCC_PLAT=%plat% -D%cppmodel%"
		},
		{ "cppmodel",	"SDCC_MODEL_SMALL" },
		{ "includedefault",	"-I%includedir%" },
		{ "includedir", 	"%prefix%include" },
		{ "prefix",		GBDKLIBDIR },
		{ "comopt",		"--noinvariant --noinduction" },
		{ "commodel", 	"small" },
		{ "com",		"%sdccdir%sdcc" },
		{ "comflag",    "-c"},
		{ "comdefault",	"-mgbz80 --no-std-crt0 --fsigned-char --use-stdout" },
		{ "as",		"%sdccdir%sdasgb" },
		{ "bankpack", "%bindir%bankpack" },
		{ "ld",		"%sdccdir%sdldgb" },
		{ "libdir",		"%prefix%lib/%libmodel%/asxxxx/" },
		{ "libmodel",	"small" },
#ifndef GBDKBINDIR
		{ "bindir",		"%prefix%bin/" },
#else
		{ "bindir",		GBDKBINDIR },
#endif
		{ "ihxcheck", "%bindir%ihxcheck" },
		{ "mkbin", "%sdccdir%makebin" }
};

#define NUM_TOKENS	(sizeof(_tokens)/sizeof(_tokens[0]))

static char *getTokenVal(const char *key)
{
	int i;
	for (i = 0; i < NUM_TOKENS; i++) {
		if (!strcmp(_tokens[i].name, key))
			return strdup(_tokens[i].val);
	}
	assert(0);
	return NULL;
}

static void setTokenVal(const char *key, const char *val)
{
	int i;
	for (i = 0; i < NUM_TOKENS; i++) {
		if (!strcmp(_tokens[i].name, key)) {
			_tokens[i].val = strdup(val);
			return;
		}
	}
	assert(0);
}

/*
$1 are extra parameters passed using -W
$2 is the list of objects passed as parameters
$3 is the output file
*/
static CLASS classes[] = {
		{ "gbz80",
			"gb",
			"gb",
			"%cpp% %cppdefault% -DGB=1 -DGAMEBOY=1 -DINT_16_BITS $1 $2 $3",
			"%includedefault%",
			"%com% %comdefault% -Wa-pog -DGB=1 -DGAMEBOY=1 -DINT_16_BITS $1 %comflag% $2 -o $3",
			"%as% -pog $1 $3 $2",
			"%bankpack% $1 $2",
			"%ld% -n -i $1 -k %libdir%%port%/ -l %port%.lib "
				"-k %libdir%%plat%/ -l %plat%.lib $3 %libdir%%plat%/crt0.o $2",
			"%ihxcheck% $2 $1",
			"%mkbin% -Z $1 $2 $3"
		},
		{ "z80",
			"afghan",
			"afghan",
			"%cpp% %cppdefault% $1 $2 $3",
			"%includedefault%",
			"%com% %comdefault% $1 $2 $3",
			"%as% -pog $1 $3 $2",
			"%bankpack% $1 $2",
			"%ld% -n -- -i $1 -b_CODE=0x8100 -k%libdir%%port%/ -l%port%.lib "
				"-k%libdir%%plat%/ -l%plat%.lib $3 %libdir%%plat%/crt0.o $2",
			"%ihxcheck% $2 $1",
			"%mkbin% -Z $1 $2 $3"
		},
		{ "z80",
			NULL,
			"consolez80",
			"%cpp% %cppdefault% $1 $2 $3",
			"-I%includedir%/gbdk-lib",
			"%com% %comdefault% $1 $2 $3",
			"%as% -pog $1 $3 $2",
			"%bankpack% $1 $2",
			"%ld% -n -- -i $1 -b_DATA=0x8000 -b_CODE=0x200 -k%libdir%%port%/ -l%port%.lib "
				"-k%libdir%%plat%/ -l%plat%.lib $3 %libdir%%plat%/crt0.o $2",
			"%ihxcheck% $2 $1",
			"%mkbin% -Z $1 $2 $3"
		}
};

static CLASS *_class = &classes[0];

#define NUM_CLASSES 	(sizeof(classes)/sizeof(classes[0]))

static int setClass(const char *port, const char *plat)
{
	int i;
	for (i = 0; i < NUM_CLASSES; i++) {
		if (!strcmp(classes[i].port, port)) {
			if (plat && classes[i].plat && !strcmp(classes[i].plat, plat)) {
				_class = classes + i;
				return 1;
			}
			else if (!classes[i].plat || !plat) {
				_class = classes + i;
				return 1;
			}
		}
	}
	return 0;
}

/* Algorithim
	 while (chars in string)
		if space, concat on end
	if %
		Copy off what we have sofar
		Call ourself on value of token
		Continue scanning
*/

/* src is destroyed */
static char **subBuildArgs(char **args, char *template)
{
	char *src = template;
	char *last = src;
	static int quoting = 0;

	/* Shared buffer between calls of this function. */
	static char buffer[128];
	static int indent = 0;

	indent++;
	while (*src) {
		if (isspace(*src) && !quoting) {
			/* End of set - add in the command */
			*src = '\0';
			strcat(buffer, last);
			*args = strdup(buffer);
			buffer[0] = '\0';
			args++;
			last = src + 1;
		}
		else if (*src == '%') {
			/* Again copy off what we already have */
			*src = '\0';
			strcat(buffer, last);
			*src = '%';
			src++;
			last = src;
			while (*src != '%') {
				if (!*src) {
					/* End of string without closing % */
					assert(0);
				}
				src++;
			}
			*src = '\0';
			/* And recurse :) */
			args = subBuildArgs(args, getTokenVal(last));
			*src = '%';
			last = src + 1;
		}
		else if (*src == '\"') {
			quoting = !quoting;
		}
		src++;
	}
	strcat(buffer, last);
	if (indent == 1) {
		*args = strdup(buffer);
		args++;
		buffer[0] = '\0';
	}

	indent--;
	return args;
}

static void buildArgs(char **args, const char *template)
{
	char *s = strdup(template);
	char **last;
	last = subBuildArgs(args, s);
	*last = NULL;
}

char *suffixes[] = { ".c", ".i", ".asm;.s", ".o;.obj", ".ihx;.gb", 0 };
char inputs[256] = "";

char *cpp[256];
char *include[256];
char *com[256] = { "", "", "" };
char *as[256];
char *ihxcheck[256];
char *ld[256];
char *bankpack[256];
char *mkbin[256];

const char *starts_with(const char *s1, const char *s2)
{
	if (!strncmp(s1, s2, strlen(s2))) {
		return s1 + strlen(s2);
	}
	return NULL;
}

int option(char *arg) {
	const char *tail;
	if ((tail = starts_with(arg, "--prefix="))) {
		/* Go through and set all of the paths */
		setTokenVal("prefix", tail);
		return 1;
	}
	else if ((tail = starts_with(arg, "--gbdklibdir="))) {
		setTokenVal("libdir", tail);
		return 1;
	}
	else if ((tail = starts_with(arg, "--gbdkincludedir="))) {
		setTokenVal("includedir", tail);
		return 1;
	}
	else if ((tail = starts_with(arg, "--sdccbindir="))) {
		// Allow to easily run with external SDCC snapshot / release
		setTokenVal("sdccdir", tail);
		return 1;
	}
	else if ((tail = starts_with(arg, "-S"))) {
		// -S is compile to ASM only
		// When composing the compile stage, swap in of -S instead of default -c
		setTokenVal("comflag", "-S");
	}
    else if ((tail = starts_with(arg, "-m"))) {
		/* Split it up into a asm/port pair */
		char *slash = strchr(tail, '/');
		if (slash) {
			*slash++ = '\0';
			setTokenVal("plat", slash);
		}
		setTokenVal("port", tail);
		if (!setClass(tail, slash)) {
			*(slash - 1) = '/';
			fprintf(stderr, "%s: unrecognised port/platform from %s\n", progname, arg);
			exit(-1);
		}
		return 1;
	}
	else if ((tail = starts_with(arg, "--model-"))) {
		if (!strcmp(tail, "small")) {
			setTokenVal("commodel", "small");
			setTokenVal("libmodel", "small");
			setTokenVal("cppmodel", "SDCC_MODEL_SMALL");
			return 1;
		}
		else if (!strcmp(tail, "medium")) {
			setTokenVal("commodel", "medium");
			setTokenVal("libmodel", "medium");
			setTokenVal("cppmodel", "SDCC_MODEL_MEDIUM");
			return 1;
		}
	}
	return 0;
}

void finalise(void)
{
	if (!_class->plat) {
		setTokenVal("plat", _class->default_plat);
	}
	buildArgs(cpp, _class->cpp);
	buildArgs(include, _class->include);
	buildArgs(com, _class->com);
	buildArgs(as, _class->as);
	buildArgs(bankpack, _class->bankpack);
	buildArgs(ld, _class->ld);
	buildArgs(ihxcheck, _class->ihxcheck);
	buildArgs(mkbin, _class->mkbin);
}

void set_gbdk_dir(char* argv_0)
{
	char buf[1024 - 2]; // Path will get quoted below, so reserve two characters for them
#ifdef __WIN32__
	char slash = '\\';
	if (GetModuleFileName(NULL,buf, sizeof(buf)) == 0)
	{
		return;
	}
#else
	char slash = '/';
	strncpy(buf, argv_0, sizeof(buf));
	buf[sizeof(buf) - 1] = '\0';
#endif

	// Strip of the trailing GBDKDIR/bin/lcc.exe and use it as the prefix.
	char *p = strrchr(buf, slash);
	if (p) {
		while(p != buf && *(p - 1) == slash) //Fixes https://github.com/Zal0/gbdk-2020/issues/29
			-- p;
		*p = '\0';

		p = strrchr(buf, slash);
		if (p) {
			*++p = '\0';
			char quotedBuf[1024];
			snprintf(quotedBuf, sizeof(quotedBuf), "\"%s\"", buf);
			setTokenVal("prefix", quotedBuf);
		}
	}
}
