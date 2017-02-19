#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "punycode.h"
#include "url.h"

static struct url_opts opts = {
  .host_normalizer = punycode_encode,
  .flags = URLFL_REMOVE_EMPTY_QUERY | URLFL_REMOVE_EMPTY_FRAGMENT,
};

static int test_remove_dot_segments() {
	size_t i, len, nlen;
	struct {
		const char *input;
		const char *expected;
	} *tv, testvals[] = {
		{"", ""},
		{".", ""},
		{"/", "/"},
		{"..", ""},
		{"../", ""},
		{"./", ""},
		{"./.", ""},
		{"/..", "/"},
		{"/../", "/"},
		{"././..", ""},
		{"././../", ""},
		{"foo", "foo"},
		{"foo/", "foo/"},
		{"foo/bar", "foo/bar"},
		{"/foo", "/foo"},
		{"/foo.", "/foo."},
		{"/foo..", "/foo.."},
		{"/foo/", "/foo/"},
		{"/foo/x", "/foo/x"},
		{"/foo//x", "/foo//x"},
		{"/foo/x/", "/foo/x/"},
		{"/foo//x/", "/foo//x/"},
		{"/foo/x/.", "/foo/x/"},
		{"/foo/x/./..", "/foo/"},
		{"/foo/x/./../", "/foo/"},
		{"/foo/x/./../..", "/"},
		{"/foo/x/./../../", "/"},
		{"/foo/x/./../../..", "/"},
		{"/foo/x/./../../../", "/"},
		{"/foo/x/./../../../.", "/"},
		{"/foo/x/./../../.././", "/"},
		{NULL, NULL},
	};

	for(i=0; testvals[i].input != NULL; i++) {
		tv = &testvals[i];
		len = strlen(tv->input);
		char icopy[len+1];
		strcpy(icopy, tv->input);
		if ((nlen = url_remove_dot_segments(icopy, len)) > len) {
			fprintf(stderr, "  overflow: %zu\n", i);
			goto fail;
		}

		len = strlen(tv->expected);
		if (len != nlen) {
			fprintf(stderr, "  input:\"%s\" expected:\"%*s\""
					" actual:\"%*s\"\n (mismatched lengths)",
					tv->input, (int)len, tv->expected,
					(int)nlen, icopy);
			goto fail;
		}

		if (nlen > 0 && strncmp(tv->expected, icopy, nlen) != 0) {
			fprintf(stderr, "  input:\"%s\" (i:%zu) expected:\"%s\""
					" actual:\"%s\"\n",
					tv->input, i, tv->expected, icopy);
			goto fail;
		}
	}

	return 0;
fail:
	return -1;
}

static void url_parts_flagstr(int flags, char *buf, size_t len) {
  int next;

  next = snprintf(buf, len, "0");
  buf += next; len -= (size_t)next;

  if (flags & URLPART_HAS_USERINFO) {
    next = snprintf(buf, len, "|URLPART_HAS_USERINFO");
    buf += next; len -= (size_t)next;
  }

  if (flags & URLPART_HAS_PORT) {
    next = snprintf(buf, len, "|URLPART_HAS_PORT");
    buf += next; len -= (size_t)next;
  }

  if (flags & URLPART_HAS_QUERY) {
    next = snprintf(buf, len, "|URLPART_HAS_QUERY");
    buf += next; len -= (size_t)next;
  }

  if (flags & URLPART_HAS_FRAGMENT) {
    next = snprintf(buf, len, "|URLPART_HAS_FRAGMENT");
    buf += next; len -= (size_t)next;
  }
}


static void print_url_parts(const char *name, struct url_parts *p) {
    char flagsbuf[512];

    url_parts_flagstr(p->flags, flagsbuf, sizeof(flagsbuf));
	fprintf(stderr, "  %s:\n    scheme: %zu %zu\n    auth: %zu %zu\n"
			"    userinfo %zu %zu\n    host: %zu %zu\n    port: %zu %zu\n"
			"    path: %zu %zu\n    query: %zu %zu\n    fragment: %zu %zu\n"
            "    flags: %s\n",
			name,
			p->scheme, p->schemelen,
			p->auth, p->authlen,
			p->userinfo, p->userinfolen,
			p->host, p->hostlen,
			p->port, p->portlen,
			p->path, p->pathlen,
			p->query, p->querylen,
			p->fragment, p->fragmentlen,
            flagsbuf);
}

static int test_parse() {
	url_ctx_t *ctx = NULL;
	size_t i;
	struct url_parts parts;
	struct tv {
		const char *input;
		struct url_parts expected;
	} *tv, testvals[] = {
		{"", {
			.scheme = 0, .schemelen = 0,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 0, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = 0,
		}},
		{"//example.com", {
			.scheme = 0, .schemelen = 0,
			.auth = 2, .authlen = 11,
			.userinfo = 0, .userinfolen = 0,
			.host = 2, .hostlen = 11,
			.port = 0, .portlen = 0,
			.path = 13, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = 0,
		}},
		{"#foo", {
			.scheme = 0, .schemelen = 0,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 0, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 1, .fragmentlen = 3,
			.flags = URLPART_HAS_FRAGMENT,
		}},
		{"?foo", {
			.scheme = 0, .schemelen = 0,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 0, .pathlen = 0,
			.query = 1, .querylen = 3,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_QUERY,
		}},
		{"foo", {
			.scheme = 0, .schemelen = 0,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 0, .pathlen = 3,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = 0,
		}},
		{"foo;bar:baz", {
			.scheme = 0, .schemelen = 0,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 0, .pathlen = 11,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = 0,
		}},
		{"foo:", {
			.scheme = 0, .schemelen = 3,
			.auth = 0, .authlen = 0,
			.userinfo = 0, .userinfolen = 0,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 4, .pathlen = 0, /* zero-len path is a bit unintuitive */
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = 0,
		}},
		{"foo://wiie:foo@", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 9,
			.userinfo = 6, .userinfolen = 8,
			.host = 0, .hostlen = 0,
			.port = 0, .portlen = 0,
			.path = 15, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO,
		}},
		{"foo://wiie:foo@bar", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 12,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 3,
			.port = 0, .portlen = 0,
			.path = 18, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO,
		}},
		{"foo://wiie:foo@bar:", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 13,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 3,
			.port = 0, .portlen = 0,
			.path = 19, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT,
		}},
		{"foo://wiie:foo@bar:22", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 15,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 3,
			.port = 19, .portlen = 2,
			.path = 21, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT,
		}},
		{"foo://wiie:foo@bar:22/", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 15,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 3,
			.port = 19, .portlen = 2,
			.path = 21, .pathlen = 1,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT,
		}},
		{"foo://wiie:foo@[::1%1]:22", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 0,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT,
		}},
		{"foo://wiie:foo@[::1%1]:22/", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 1,
			.query = 0, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT,
		}},
		{"foo://wiie:foo@[::1%1]:22/?", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 1,
			.query = 27, .querylen = 0,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT|URLPART_HAS_QUERY,
		}},
		{"foo://wiie:foo@[::1%1]:22/?foo", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 1,
			.query = 27, .querylen = 3,
			.fragment = 0, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT|URLPART_HAS_QUERY,
		}},
		{"foo://wiie:foo@[::1%1]:22/?foo#", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 1,
			.query = 27, .querylen = 3,
			.fragment = 31, .fragmentlen = 0,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT|URLPART_HAS_QUERY|
                URLPART_HAS_FRAGMENT,
		}},
		{"foo://wiie:foo@[::1%1]:22/?foo#bar", {
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 19,
			.userinfo = 6, .userinfolen = 8,
			.host = 15, .hostlen = 7,
			.port = 23, .portlen = 2,
			.path = 25, .pathlen = 1,
			.query = 27, .querylen = 3,
			.fragment = 31, .fragmentlen = 3,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT|URLPART_HAS_QUERY|
                URLPART_HAS_FRAGMENT,
		}},
		{"foo://wiie:foö@[::1%1]:22/?foo#bar", { /* UTF-8 ö = 0xC3 0xB6 */
			.scheme = 0, .schemelen = 3,
			.auth = 6, .authlen = 20,
			.userinfo = 6, .userinfolen = 9,
			.host = 16, .hostlen = 7,
			.port = 24, .portlen = 2,
			.path = 26, .pathlen = 1,
			.query = 28, .querylen = 3,
			.fragment = 32, .fragmentlen = 3,
			.flags = URLPART_HAS_USERINFO|URLPART_HAS_PORT|URLPART_HAS_QUERY|
                URLPART_HAS_FRAGMENT,
		}},
		{NULL, {0}},
	};

	if ((ctx = url_ctx_new(&opts)) == NULL) {
		fprintf(stderr, "url_ctx_new failed\n");
		goto fail;
	}

	for(i=0; testvals[i].input != NULL; i++) {
		tv = &testvals[i];
		if (url_parse(ctx, tv->input, &parts) != 0) {
			fprintf(stderr, "url_parse failed for str:\"%s\"\n", tv->input);
			goto fail;
		}

		if (memcmp(&tv->expected, &parts, sizeof(parts)) != 0) {
            fprintf(stderr, "  input[%zu]: %s\n", i, tv->input);
			print_url_parts("expected", &tv->expected);
			print_url_parts("actual", &parts);
			goto fail;
		}
	}

	url_ctx_free(ctx);
	return 0;

fail:
	if (ctx != NULL) {
		url_ctx_free(ctx);
	}
	return -1;
}

static int test_normalize() {
	size_t i;
	buf_t buf;
	url_ctx_t *ctx = NULL;
	struct {
		const char *input;
		const char *expected;
	} *tv, testvals[] = {
		/* these URLs are relative paths, and should only have
		 * remove_dot_segments applied to them. Trailing slashes for empty
		 * paths are done only when the URL has an authority component.
		 * Allows us to use url_normalize within url_resolve for the reference
		 * URL. */
		{"https://www.straße.de/straße",
			"https://www.xn--strae-oqa.de/stra%c3%9fe"},
		{"", ""},
		{".", ""},
		{"/", "/"},
		{"..", ""},
		{"../", ""},
		{"./", ""},
		{"./.", ""},
		{"/..", "/"},
		{"/../", "/"},
		{"././..", ""},
		{"././../", ""},
		{"././../?foo", "?foo"},

		{"foo", "foo"},
		{"//example.com", "//example.com/"},
		{"http://example.com", "http://example.com/"},
		{"http://example.com/", "http://example.com/"},
		{"http://example.com/.", "http://example.com/"},
		{"http://example.com/./", "http://example.com/"},
		{"http://example.com/./foo", "http://example.com/foo"},
		{"http://example.com/./foo/", "http://example.com/foo/"},
		{"http://example.com/..", "http://example.com/"},
		{"http://example.com/../", "http://example.com/"},
		{"http://example.com/foo/..", "http://example.com/"},
		{"http://example.com/foo/../bar", "http://example.com/bar"},
		{"http://example.com/foo/../bar/", "http://example.com/bar/"},
		{"http://example.com/föö/../bar/", "http://example.com/bar/"},
		{"http://example.com/foo/../bör/", "http://example.com/b%c3%b6r/"},
		{"http://example.com/foo/../bar/?q=?&bar=öh",
				"http://example.com/bar/?q=?&bar=%c3%b6h"},
		{NULL, NULL},
	};

	if (buf_init(&buf, 2) == NULL) {
		goto fail;
	}
	if ((ctx = url_ctx_new(&opts)) == NULL) {
		goto fail;
	}
	for(i=0; testvals[i].input != NULL; i++) {
		tv = &testvals[i];
		if (url_normalize(ctx, tv->input, &buf) != EURL_OK) {
			fprintf(stderr, "  parse failure for index %zu\n", i);
			goto fail;
		}

		if (strcmp(tv->expected, buf.data) != 0) {
			fprintf(stderr, "  input:\"%s\" expected:\"%s\" was:\"%s\"\n",
				tv->input, tv->expected, buf.data);
			goto fail;
		}

	}

	url_ctx_free(ctx);
	buf_cleanup(&buf);
	return 0;
fail:
	if (ctx != NULL) {
		url_ctx_free(ctx);
	}

	buf_cleanup(&buf);
	return -1;

}

static int test_resolve() {
	size_t i;
	buf_t buf;
	url_ctx_t *ctx = NULL;
	struct {
		const char *base;
		const char *ref;
		const char *expected;
	} *tv,  testvals[] = {
		{
			"",
			"",
			"",
		},
		{
			"foo",
			"/bar",
			"/bar",
		},
		{
			"foo",
			"?bar",
			"foo?bar",
		},
		{
			"/foo",
			"?bar",
			"/foo?bar",
		},
		{
			"/foo",
			"#bar",
			"/foo#bar",
		},
		{
			"http://example.com",
			"",
			"http://example.com", /* base is not normalized in url_resolve */
		},
		{
			"http://example.com/",
			"",
			"http://example.com/",
		},
		{
			"http://example.com/",
			"?",
			"http://example.com/",
		},
		{
			"http://example.com/",
			"#",
			"http://example.com/",
		},
		{
			"http://example.com/",
			"#bar",
			"http://example.com/#bar",
		},
		{
			"http://example.com/",
			"?#foo",
			"http://example.com/#foo",
		},
		{
			"http://example.com/",
			"?foo",
			"http://example.com/?foo",
		},
		{
			"http://example.com/foo",
			"?bar",
			"http://example.com/foo?bar",
		},
		{
			"http://example.com/foo",
			"?bar#baz",
			"http://example.com/foo?bar#baz",
		},
		{
			"http://example.com/foo",
			"/",
			"http://example.com/",
		},
		{
			"http://example.com/foo",
			"/..",
			"http://example.com/",
		},
		{
			"http://example.com/foo/",
			"/",
			"http://example.com/",
		},
		{
			"http://example.com/foo/",
			"/..",
			"http://example.com/",
		},
		{
			"http://example.com/foo/",
			"/bar",
			"http://example.com/bar",
		},
		{
			"http://example.com/foo/",
			"/bar/",
			"http://example.com/bar/",
		},
		{
			"http://example.com/",
			"http://www.example.com",
			"http://www.example.com/",
		},
		{
			"http://example.com/",
			"http://www.example.com/",
			"http://www.example.com/",
		},
		{
			"http://example.com/",
			"//www.example.com/",
			"http://www.example.com/",
		},
		{
			"//example.com/",
			"//www.example.com/",
			"//www.example.com/",
		},
		{
			"//example.com/",
			"//www.example.com",
			"//www.example.com/",
		},
		{
			"http://example.com/",
			"bar",
			"http://example.com/bar",
		},
		{
			"http://example.com/foo",
			"bar",
			"http://example.com/bar",
		},
		{
			"http://example.com/foo/",
			"bar",
			"http://example.com/foo/bar",
		},
		{
			"//example.com/",
			"bar",
			"//example.com/bar",
		},
		{
			"//example.com/foo",
			"bar",
			"//example.com/bar",
		},
		{
			"//example.com/foo/",
			"bar/",
			"//example.com/foo/bar/",
		},
		{
			"https://example.com/foo/",
			"bar/?",
			"https://example.com/foo/bar/",
		},
		{
			"https://example.com/foo/",
			"bar/?#",
			"https://example.com/foo/bar/",
		},
		{
			"https://example.com/foo/",
			"bar?baz",
			"https://example.com/foo/bar?baz",
		},
		{
			"https://example.com/foo/",
			"bar/?baz",
			"https://example.com/foo/bar/?baz",
		},
		{
			"https://example.com/foo/",
			"bar#baz",
			"https://example.com/foo/bar#baz",
		},
		{
			"https://example.com/foo/",
			"bar/#baz",
			"https://example.com/foo/bar/#baz",
		},
		{
			"https://example.com/",
			"//www.example.com/",
			"https://www.example.com/",
		},
		{
			"https://example.com/",
			"//www.example.com/?",
			"https://www.example.com/",
		},
		{
			"https://example.com/",
			"//www.example.com/?#",
			"https://www.example.com/",
		},
		{
			"https://example.com/",
			"//www.example.com/?foo#",
			"https://www.example.com/?foo",
		},
		{
			"https://example.com/",
			"//www.example.com/?foo#bar",
			"https://www.example.com/?foo#bar",
		},
		{
			"https://example.com/",
			"//www.example.com/?foö#bar",
			"https://www.example.com/?fo%c3%b6#bar",
		},
		{
			"https:",
			"//www.straße.de/straße",
			"https://www.xn--strae-oqa.de/stra%c3%9fe",
		},

		{NULL, NULL, NULL},
	};

	if (buf_init(&buf, 2) == NULL) {
		goto fail;
	}
	if ((ctx = url_ctx_new(&opts)) == NULL) {
		goto fail;
	}
	for(i=0; testvals[i].base != NULL; i++) {
		tv = &testvals[i];
		if (url_resolve(ctx, tv->base, tv->ref, &buf) != EURL_OK) {
			fprintf(stderr, "  resolve failure for index %zu\n", i);
			goto fail;
		}

		if (strcmp(tv->expected, buf.data) != 0) {
			fprintf(stderr, "  base:\"%s\""
			" ref:\"%s\""
			" expected:\"%s\""
			" was:\"%s\"\n",
				tv->base, tv->ref, tv->expected, buf.data);
			goto fail;
		}
	}

	url_ctx_free(ctx);
	buf_cleanup(&buf);
	return 0;
fail:
	if (ctx != NULL) {
		url_ctx_free(ctx);
	}

	buf_cleanup(&buf);
	return -1;

}

int main() {
	int ret = EXIT_SUCCESS;
	size_t i;
	struct {
		const char *name;
		int (*cb)(void);
	} *t, tests[] = {
		{"remove_dot_segments", test_remove_dot_segments},
		{"parse", test_parse},
		{"normalize", test_normalize},
		{"resolve", test_resolve},
		{NULL, NULL},
	};

	for(i=0; tests[i].name != NULL; i++) {
		t = &tests[i];
		if (t->cb() == 0) {
			fprintf(stderr, "OK  %s\n", t->name);
		} else {
			fprintf(stderr, "ERR %s\n", t->name);
			ret = EXIT_FAILURE;
		}
	}

	return ret;
}

