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
  BUF_MEM *str;
};

#define x509_san_get_data(san__)   ((san__)->str->data)
#define x509_san_get_length(san__) ((san__)->str->length)

int x509_certchain_from_mem(struct x509_certchain *chain, const void *data,
    size_t len);

void x509_certchain_cleanup(struct x509_certchain *chain);

size_t x509_certchain_get_ncerts(struct x509_certchain *chain);

int x509_certchain_get_cert(struct x509_certchain *chain, size_t depth,
    struct x509_cert *dst);

int x509_cert_get_subject_name(struct x509_cert *cert, char **strname);

int x509_cert_get_issuer_name(struct x509_cert *cert, char **strname);

/* TODO: Implement serial, valid_from, valid_to accessors */

int x509_cert_get_sans(struct x509_cert *cert, struct x509_sans *sans);

void x509_sans_cleanup(struct x509_sans *sans);

size_t x509_sans_get_nelems(struct x509_sans *sans);

int x509_cert_get_san(struct x509_sans *sans, size_t index,
    struct x509_san *san);

void x509_free_san(struct x509_san *san);

void x509_free_data(void *data);

#endif
