#ifndef YANS_X509_H__
#define YANS_X509_H__

#include <openssl/x509.h>

#define X509_IS_ERR(x) ((x) < 0)
#define X509_OK     0
#define X509_ERR   -1
#define X509_NOMEM -2

struct x509_certchain {
  STACK_OF(X509_INFO) *certs;
};

int x509_certchain_from_mem(struct x509_certchain *chain, const void *data,
    size_t len);

void x509_certchain_cleanup(struct x509_certchain *chain);

#endif
