/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#ifndef YANS_X509_H__
#define YANS_X509_H__

#include <openssl/x509v3.h>

#define X509_IS_ERR(x) ((x) < 0)
#define X509_OK     0
#define X509_ERR   -1
#define X509_NOMEM -2

struct x509_certchain {
  STACK_OF(X509_INFO) *certs;
};

struct x509_cert {
  X509 *cert;
};

struct x509_sans {
  GENERAL_NAMES *names;
};

struct x509_san {
  char *str;
};

#define x509_san_get_data(san__)   ((san__)->str)

int x509_certchain_from_mem(struct x509_certchain *chain, const void *data,
    size_t len);

void x509_certchain_cleanup(struct x509_certchain *chain);

size_t x509_certchain_get_ncerts(struct x509_certchain *chain);

int x509_certchain_get_cert(struct x509_certchain *chain, size_t depth,
    struct x509_cert *dst);

int x509_cert_get_subject_name(struct x509_cert *cert, char **strname);

int x509_cert_get_issuer_name(struct x509_cert *cert, char **strname);

int x509_cert_get_valid_not_before(struct x509_cert *cert, char *out,
    size_t len);
int x509_cert_get_valid_not_after(struct x509_cert *cert, char *out,
    size_t len);

/* TODO: Implement serial accessor */

int x509_cert_get_sans(struct x509_cert *cert, struct x509_sans *sans);

void x509_sans_cleanup(struct x509_sans *sans);

size_t x509_sans_get_nelems(struct x509_sans *sans);

int x509_sans_get_san(struct x509_sans *sans, size_t index,
    struct x509_san *san);

void x509_free_san(struct x509_san *san);

void x509_free_data(void *data);

#endif
