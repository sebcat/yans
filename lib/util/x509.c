#include <openssl/bio.h>
#include <openssl/pem.h>

#include <lib/util/x509.h>

int x509_certchain_from_mem(struct x509_certchain *chain, const void *data,
    size_t len) {
  BIO *b;

  if (len > INT_MAX) {
    return X509_ERR;
  }

  b = BIO_new_mem_buf(data, len);
  if (b == NULL) {
    return X509_NOMEM;
  }

  chain->certs = PEM_X509_INFO_read_bio(b, NULL, NULL, NULL);
  BIO_free(b);
  return chain->certs == NULL ? X509_ERR : X509_OK;
}

void x509_certchain_cleanup(struct x509_certchain *chain) {
  if (chain && chain->certs) {
    sk_X509_INFO_pop_free(chain->certs, X509_INFO_free);
    chain->certs = NULL;
  }
}
