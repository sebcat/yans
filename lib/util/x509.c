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

size_t x509_certchain_get_ncerts(struct x509_certchain *chain) {
  int ret = 0;

  if (chain && chain->certs) {
    ret = sk_X509_INFO_num(chain->certs);
    if (ret < 0) {
      ret = 0;
    }
  }

  return (size_t)ret;
}

int x509_certchain_get_subject_name(struct x509_certchain *chain, size_t cert,
    char **strname) {
  size_t ncerts;
  X509_INFO *info;
  X509_NAME *name;

  ncerts = x509_certchain_get_ncerts(chain);
  if (cert >= ncerts) {
    return X509_ERR;
  }

  info = sk_X509_INFO_value(chain->certs, cert);
  if (info == NULL || info->x509 == NULL) {
    return X509_ERR;
  }

  name = X509_get_subject_name(info->x509);
  if (name == NULL) {
    return X509_ERR;
  }

  if (strname) {
    *strname = X509_NAME_oneline(name, NULL, 0);
  }

  return 0;
}

int x509_certchain_get_issuer_name(struct x509_certchain *chain, size_t cert,
    char **strname) {
  size_t ncerts;
  X509_INFO *info;
  X509_NAME *name;

  ncerts = x509_certchain_get_ncerts(chain);
  if (cert >= ncerts) {
    return X509_ERR;
  }

  info = sk_X509_INFO_value(chain->certs, cert);
  if (info == NULL || info->x509 == NULL) {
    return X509_ERR;
  }

  name = X509_get_issuer_name(info->x509);
  if (name == NULL) {
    return X509_ERR;
  }

  if (strname) {
    *strname = X509_NAME_oneline(name, NULL, 0);
  }

  return 0;
}

void x509_certchain_free_data(struct x509_certchain *chain, void *data) {
  (void)chain;

  if (data) {
    OPENSSL_free(data);
  }
}
