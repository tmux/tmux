/* $XTermId: xstrings.c,v 1.71 2017/11/10 00:52:29 tom Exp $ */

/*
 * Copyright 2000-2016,2017 by Thomas E. Dickey
 *
 *                         All Rights Reserved
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization.
 */

#include <xterm.h>

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <xstrings.h>

static void
alloc_pw(struct passwd *target, struct passwd *source)
{
    *target = *source;
    /* we care only about these strings */
    target->pw_dir = x_strdup(source->pw_dir);
    target->pw_name = x_strdup(source->pw_name);
    target->pw_shell = x_strdup(source->pw_shell);
}

static void
free_pw(struct passwd *source)
{
    free(source->pw_dir);
    free(source->pw_name);
    free(source->pw_shell);
}

void
x_appendargv(char **target, char **source)
{
    if (target && source) {
	target += x_countargv(target);
	while ((*target++ = *source++) != 0) ;
    }
}

char *
x_basename(char *name)
{
    char *cp;

    cp = strrchr(name, '/');
    return (cp ? cp + 1 : name);
}

unsigned
x_countargv(char **argv)
{
    unsigned result = 0;
    if (argv) {
	while (*argv++) {
	    ++result;
	}
    }
    return result;
}

/*
 * Decode a hexadecimal string, returning the decoded string.
 * On return, 'next' points to the first character not part of the input.
 * The caller must free the result.
 */
char *
x_decode_hex(const char *source, const char **next)
{
    char *result = 0;
    int pass;
    size_t j, k;

    for (pass = 0; pass < 2; ++pass) {
	for (j = k = 0; isxdigit(CharOf(source[j])); ++j) {
	    if ((pass != 0) && (j & 1) != 0) {
		result[k++] = (char) ((x_hex2int(source[j - 1]) << 4)
				      | x_hex2int(source[j]));
	    }
	}
	*next = (source + j);
	if ((j & 1) == 0) {
	    if (pass) {
		result[k] = '\0';
	    } else {
		result = malloc(++j);
		if (result == 0)
		    break;	/* not enough memory */
	    }
	} else {
	    break;		/* must have an even number of digits */
	}
    }
    return result;
}

/*
 * Encode a string into hexadecimal, returning the encoded string.
 * The caller must free the result.
 */
char *
x_encode_hex(const char *source)
{
    size_t need = (strlen(source) * 2) + 1;
    char *result = malloc(need);

    if (result != 0) {
	unsigned j, k;
	for (j = k = 0; source[j] != '\0'; ++j) {
	    sprintf(result + k, "%02X", CharOf(source[j]));
	    k += 2;
	}
    }
    return result;
}

char *
x_getenv(const char *name)
{
    char *result;
    result = x_strdup(x_nonempty(getenv(name)));
    TRACE2(("getenv(%s) %s\n", name, result));
    return result;
}

static char *
login_alias(char *login_name, uid_t uid, struct passwd *in_out)
{
    /*
     * If the logon-name differs from the value we get by looking in the
     * password file, check if it does correspond to the same uid.  If so,
     * allow that as an alias for the uid.
     */
    if (!IsEmpty(login_name)
	&& strcmp(login_name, in_out->pw_name)) {
	struct passwd pw2;
	Boolean ok2;

	if ((ok2 = x_getpwnam(login_name, &pw2))) {
	    uid_t uid2 = pw2.pw_uid;
	    struct passwd pw3;
	    Boolean ok3;

	    if ((ok3 = x_getpwuid(uid, &pw3))
		&& ((uid_t) pw3.pw_uid == uid2)) {
		/* use the other passwd-data including shell */
		alloc_pw(in_out, &pw2);
	    } else {
		free(login_name);
		login_name = NULL;
	    }
	    if (ok2)
		free_pw(&pw2);
	    if (ok3)
		free_pw(&pw3);
	}
    }
    return login_name;
}

/*
 * Call this with in_out pointing to data filled in by x_getpwnam() or by
 * x_getpwnam().  It finds the user's logon name, if possible.  As a side
 * effect, it updates in_out to fill in possibly more-relevant data, i.e.,
 * in case there is more than one alias for the same uid.
 */
char *
x_getlogin(uid_t uid, struct passwd *in_out)
{
    char *login_name;

    login_name = login_alias(x_getenv("LOGNAME"), uid, in_out);
    if (IsEmpty(login_name)) {
	free(login_name);
	login_name = login_alias(x_getenv("USER"), uid, in_out);
    }
#ifdef HAVE_GETLOGIN
    /*
     * Of course getlogin() will fail if we're started from a window-manager,
     * since there's no controlling terminal to fuss with.  For that reason, we
     * tried first to get something useful from the user's $LOGNAME or $USER
     * environment variables.
     */
    if (IsEmpty(login_name)) {
	TRACE2(("...try getlogin\n"));
	free(login_name);
	login_name = login_alias(x_strdup(getlogin()), uid, in_out);
    }
#endif

    if (IsEmpty(login_name)) {
	free(login_name);
	login_name = x_strdup(in_out->pw_name);
    }

    TRACE2(("x_getloginid ->%s\n", NonNull(login_name)));
    return login_name;
}

/*
 * Simpler than getpwnam_r, retrieves the passwd result by name and stores the
 * result via the given pointer.  On failure, wipes the data to prevent use.
 */
Boolean
x_getpwnam(const char *name, struct passwd *result)
{
    struct passwd *ptr = getpwnam(name);
    Boolean code;

    if (ptr != 0 && OkPasswd(ptr)) {
	code = True;
	alloc_pw(result, ptr);
    } else {
	code = False;
	memset(result, 0, sizeof(*result));
    }
    return code;
}

/*
 * Simpler than getpwuid_r, retrieves the passwd result by uid and stores the
 * result via the given pointer.  On failure, wipes the data to prevent use.
 */
Boolean
x_getpwuid(uid_t uid, struct passwd *result)
{
    struct passwd *ptr = getpwuid((uid_t) uid);
    Boolean code;

    if (ptr != 0 && OkPasswd(ptr)) {
	code = True;
	alloc_pw(result, ptr);
    } else {
	code = False;
	memset(result, 0, sizeof(*result));
    }
    TRACE2(("x_getpwuid(%d) %d\n", (int) uid, (int) code));
    return code;
}

/*
 * Decode a single hex "nibble", returning the nibble as 0-15, or -1 on error.
 */
int
x_hex2int(int c)
{
    if (c >= '0' && c <= '9')
	return c - '0';
    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;
    return -1;
}

/*
 * Check if the given string is nonnull/nonempty.  If so, return a pointer
 * to the beginning of its content, otherwise return null.
 */
String
x_nonempty(String s)
{
    if (s != 0) {
	if (*s == '\0') {
	    s = 0;
	} else {
	    s = x_skip_blanks(s);
	    if (*s == '\0')
		s = 0;
	}
    }
    return s;
}

String
x_skip_blanks(String s)
{
    while (IsSpace(CharOf(*s)))
	++s;
    return s;
}

String
x_skip_nonblanks(String s)
{
    while (*s != '\0' && !IsSpace(CharOf(*s)))
	++s;
    return s;
}

static const char *
skip_blanks(const char *s)
{
    while (IsSpace(CharOf(*s)))
	++s;
    return s;
}

/*
 * Split a command-string into an argv[]-style array.
 */
char **
x_splitargs(const char *command)
{
    char **result = 0;

    if (command != 0) {
	const char *first = skip_blanks(command);
	char *blob = x_strdup(first);

	if (blob != 0) {
	    int pass;

	    for (pass = 0; pass < 2; ++pass) {
		int state;
		size_t count;
		size_t n;

		for (n = count = 0, state = 0; first[n] != '\0'; ++n) {

		    switch (state) {
		    case 0:
			if (!IsSpace(CharOf(first[n]))) {
			    state = 1;
			    if (pass)
				result[count] = blob + n;
			    ++count;
			} else {
			    blob[n] = '\0';
			}
			break;
		    case 1:
			if (IsSpace(CharOf(first[n]))) {
			    blob[n] = '\0';
			    state = 0;
			}
			break;
		    }
		}
		if (!pass) {
		    result = TypeCallocN(char *, count + 1);
		    if (!result) {
			free(blob);
			break;
		    }
		}
	    }
	}
    } else {
	result = TypeCalloc(char *);
    }
    return result;
}

/*
 * Free storage allocated by x_splitargs().
 */
void
x_freeargs(char **argv)
{
    if (argv != 0) {
	if (*argv != 0)
	    free(*argv);
	free(argv);
    }
}

int
x_strcasecmp(const char *s1, const char *s2)
{
    size_t len = strlen(s1);

    if (len != strlen(s2))
	return 1;

    return x_strncasecmp(s1, s2, (unsigned) len);
}

int
x_strncasecmp(const char *s1, const char *s2, unsigned n)
{
    while (n-- != 0) {
	char c1 = x_toupper(*s1);
	char c2 = x_toupper(*s2);
	if (c1 != c2)
	    return 1;
	if (c1 == 0)
	    break;
	s1++, s2++;
    }

    return 0;
}

/*
 * Allocates a copy of a string
 */
char *
x_strdup(const char *s)
{
    char *result = 0;

    if (s != 0) {
	char *t = TextAlloc(4 + strlen(s));
	if (t != 0) {
	    strcpy(t, s);
	}
	result = t;
    }
    return result;
}

/*
 * Returns a pointer to the first occurrence of s2 in s1,
 * or NULL if there are none.
 */
char *
x_strindex(char *s1, const char *s2)
{
    char *s3;
    size_t s2len = strlen(s2);

    while ((s3 = (strchr) (s1, *s2)) != NULL) {
	if (strncmp(s3, s2, s2len) == 0)
	    return (s3);
	s1 = ++s3;
    }
    return (NULL);
}

/*
 * Trims leading/trailing spaces from a copy of the string.
 */
char *
x_strtrim(const char *source)
{
    char *result;

    if (source != 0 && *source != '\0') {
	char *t = x_strdup(source);
	if (t != 0) {
	    char *s = t;
	    char *d = s;
	    while (IsSpace(CharOf(*s)))
		++s;
	    while ((*d++ = *s++) != '\0') {
		;
	    }
	    if (*t != '\0') {
		s = t + strlen(t);
		while (s != t && IsSpace(CharOf(s[-1]))) {
		    *--s = '\0';
		}
	    }
	}
	result = t;
    } else {
	result = x_strdup("");
    }
    return result;
}

/*
 * Trims trailing whitespace from a copy of the string.
 */
char *
x_strrtrim(const char *source)
{
    char *result;

    if (source != 0 && *source != '\0') {
	char *t = x_strdup(source);
	if (t != 0) {
	    if (*t != '\0') {
		char *s = t + strlen(t);
		while (s != t && IsSpace(CharOf(s[-1]))) {
		    *--s = '\0';
		}
	    }
	}
	result = t;
    } else {
	result = x_strdup("");
    }
    return result;
}

/*
 * Avoid using system locale for upper/lowercase conversion, since there are
 * a few locales where toupper(tolower(c)) != c.
 */
char
x_toupper(int ch)
{
    static char table[256];
    char result = table[CharOf(ch)];

    if (result == '\0') {
	unsigned n;
	static const char s[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

	for (n = 0; n < sizeof(table); ++n) {
	    table[n] = (char) n;
	}
	for (n = 0; s[n] != '\0'; ++n) {
	    table[CharOf(s[n])] = s[n % 26];
	}
	result = table[CharOf(ch)];
    }

    return result;
}

/*
 * Match strings ignoring case and allowing glob-like '*' and '?'
 */
int
x_wildstrcmp(const char *pattern, const char *actual)
{
    int result = 0;

    while (*pattern && *actual) {
	char c1 = x_toupper(*pattern);
	char c2 = x_toupper(*actual);

	if (c1 == '*') {
	    Boolean found = False;
	    pattern++;
	    while (*actual != '\0') {
		if (!x_wildstrcmp(pattern, actual++)) {
		    found = True;
		    break;
		}
	    }
	    if (!found) {
		result = 1;
		break;
	    }
	} else if (c1 == '?') {
	    ++pattern;
	    ++actual;
	} else if ((result = (c1 != c2)) == 0) {
	    ++pattern;
	    ++actual;
	} else {
	    break;
	}
    }
    return result;
}
