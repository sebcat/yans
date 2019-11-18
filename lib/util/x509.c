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
#include <string.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/objects.h>

#include <lib/util/macros.h>
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

int x509_certchain_get_cert(struct x509_certchain *chain, size_t depth,
    struct x509_cert *dst) {
  size_t ncerts;
  X509_INFO *info;

  ncerts = x509_certchain_get_ncerts(chain);
  if (depth >= ncerts) {
    return X509_ERR;
  }

  info = sk_X509_INFO_value(chain->certs, depth);
  if (info == NULL || info->x509 == NULL) {
    return X509_ERR;
  }

  if (dst) {
    dst->cert = info->x509;
  }

  return X509_OK;
}

static int convert_time(ASN1_TIME *t, char *out, size_t len) {
  int ret;
  int status = X509_ERR;
  BIO *b;

  b = BIO_new(BIO_s_mem());
  ret = ASN1_TIME_print(b, t);
  if (ret != 1) {
    goto done;
  }

  ret = BIO_gets(b, out, len);
  if (ret <= 0) {
    goto done;
  }

  BIO_free(b);
  status = X509_OK;
done:
  return status;
}

int x509_cert_get_valid_not_before(struct x509_cert *cert, char *out,
    size_t len) {
  ASN1_TIME *t;

  t = X509_get_notBefore(cert->cert);
  return convert_time(t, out, len);
}

int x509_cert_get_valid_not_after(struct x509_cert *cert, char *out,
    size_t len) {
  ASN1_TIME *t;

  t = X509_get_notAfter(cert->cert);
  return convert_time(t, out, len);
}

int x509_cert_get_subject_name(struct x509_cert *cert, char **strname) {
  X509_NAME *name;

  name = X509_get_subject_name(cert->cert);
  if (name == NULL) {
    return X509_ERR;
  }

  if (strname) {
    *strname = X509_NAME_oneline(name, NULL, 0);
  }

  return X509_OK;
}

int x509_cert_get_issuer_name(struct x509_cert *cert, char **strname) {
  X509_NAME *name;

  name = X509_get_issuer_name(cert->cert);
  if (name == NULL) {
    return X509_ERR;
  }

  if (strname) {
    *strname = X509_NAME_oneline(name, NULL, 0);
  }

  return X509_OK;
}

int x509_cert_get_sans(struct x509_cert *cert, struct x509_sans *dst) {
  GENERAL_NAMES *names;

  names = (GENERAL_NAMES*)X509_get_ext_d2i(cert->cert,
      NID_subject_alt_name, NULL, NULL);

  if (dst) {
    dst->names = names;
  }

  return X509_OK;
}

void x509_sans_cleanup(struct x509_sans *sans) {
  if (sans && sans->names) {
    GENERAL_NAMES_free(sans->names);
    sans->names = NULL;
  }
}

size_t x509_sans_get_nelems(struct x509_sans *sans) {
  size_t nelems = 0;
  int nbr_sans;

  if (sans && sans->names) {
    nbr_sans = MAX(sk_GENERAL_NAME_num(sans->names), 0);
    nelems = (size_t)nbr_sans;
  }

  return nelems;
}

int x509_sans_get_san(struct x509_sans *sans, size_t index,
    struct x509_san *san) {
  BIO *b;
  size_t nsans;
  GENERAL_NAME *name;
  char *tmp = NULL;

  nsans = x509_sans_get_nelems(sans);
  if (index >= nsans) {
    return X509_ERR;
  }

  b = BIO_new(BIO_s_mem());
  if (b == NULL) {
    return X509_NOMEM;
  }

  name = sk_GENERAL_NAME_value(sans->names, index);
  if (!name) {
    BIO_free(b);
    return X509_ERR;
  }

  GENERAL_NAME_print(b, name);
  BIO_write(b, "", 1); /* append trailing \0-byte */
/*
  This it what we used to do here:
  BIO_get_mem_ptr(b, &san->str);
  BIO_set_close(b, BIO_NOCLOSE);
  BIO_free(b);

  Using BIO_NOCLOSE in that way is described in the OpenSSL man pages.
  This was (is, at the moment with FreeBSD 13.0-CURRENT) broken for
  multiple later OpenSSL versions[1]. Depending on the version it would
  either cause a memleak or BIO_NOCLOSE was ignored in BIO_free causing us
  to do a use-after-free later in the code.

  Just strdup the SAN for now.

  [1] https://github.com/openssl/openssl/issues/8375
*/
  BIO_get_mem_data(b, &tmp);
  san->str = strdup(tmp);
  BIO_free(b);
  return X509_OK;
}

void x509_free_san(struct x509_san *san) {
  if (san && san->str) {
    free(san->str);
    san->str = NULL;
  }
}

void x509_free_data(void *data) {
  if (data) {
    OPENSSL_free(data);
  }
}
