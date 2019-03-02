#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <lib/alloc/linvar.h>

#include <lib/util/objtbl.h>
#include <lib/util/sandbox.h>
#include <lib/util/csv.h>
#include <lib/util/sha1.h>
#include <lib/util/x509.h>
#include <lib/net/tcpproto_types.h>
#include <apps/scan/collate.h>

#define MAX_INOUTS 8
#define MAX_FOPENS 128
#define MAX_MPIDS  4

#define NMEMB_NAME       1024
#define NMEMB_ADDR       4096 /* # address elements per allocation */
#define NMEMB_SVCS       4096
#define NMEMB_CERTCHAINS 4

#define LINVAR_BLKSIZE 1024 * 256 /* # bytes per allocation */

enum collate_type {
  COLLATE_UNKNOWN = 0,
  COLLATE_BANNERS,
  COLLATE_MAX,
};

struct collate_opts {
  enum collate_type type;

  FILE *in_banners[MAX_INOUTS];
  size_t nin_banners;

  FILE *out_services_csv[MAX_INOUTS];
  size_t nout_services_csv;

  FILE *out_certs_csv[MAX_INOUTS];
  size_t nout_certs_csv;

  FILE *closefps[MAX_FOPENS];
  size_t nclosefps;
};

union saddr_t {
  struct sockaddr sa;
  struct sockaddr_in sin;
  struct sockaddr_in6 sin6;
};

#define saddr_len(saddr)                 \
    ((saddr)->sa.sa_family == AF_INET6 ? \
      sizeof(struct sockaddr_in6) :      \
      sizeof(struct sockaddr_in))

struct collate_service_flags {
  unsigned int transport : 2;
  unsigned int fpid : 14;
};

struct collate_certchain {
  char sha1hash[SHA1_DSTLEN];
  struct x509_certchain chain;
  unsigned int id;
};

struct collate_service {
  union saddr_t *addr;
  const char *name;
  unsigned short mpids[MAX_MPIDS];
  struct collate_service_flags eflags;
  struct collate_certchain *fpchain;
  struct collate_certchain *mpchains[MAX_MPIDS];
};

/* allocator used */
struct linvar_ctx varmem_;

/* collation tables */
struct objtbl_ctx nametbl_;
struct objtbl_ctx addrtbl_;
struct objtbl_ctx svctbl_;
struct objtbl_ctx chaintbl_;
unsigned int chainid_ = 1; /* 0 indicates no chain */

/* 32-bit FNV1a constants */
#define FNV1A_OFFSET 0x811c9dc5
#define FNV1A_PRIME   0x1000193

/* NULL sort order in *cmp funcs */
#define NULLCMP(l,r)                       \
  if ((l) == NULL && (r) == NULL) {        \
    return 0;                              \
  } else if ((l) == NULL && (r) != NULL) { \
    return 1;                              \
  } else if ((l) != NULL && (r) == NULL) { \
    return -1;                             \
  }


static objtbl_hash_t namehash(const void *obj, objtbl_hash_t seed) {
  objtbl_hash_t hash = FNV1A_OFFSET;
  const unsigned char *data = obj;
  size_t i;

  if (obj == NULL) {
    return hash;
  }

  for (i = 0; i < sizeof(objtbl_hash_t); i++) {
    hash = (hash ^ (seed & 0xff)) * FNV1A_PRIME;
    seed >>= 8;
  }

  for (i = 0; data[i]; i++) {
    hash = (hash ^ data[i]) * FNV1A_PRIME;
  }

  return hash;
}

static int namecmp(const void *k, const void *e) {

  NULLCMP(k,e);
  return strcmp(k, e);
}

static objtbl_hash_t addrhash(const void *obj, objtbl_hash_t seed) {
  const union saddr_t *addr = obj;
  objtbl_hash_t hash = FNV1A_OFFSET;
  const unsigned char *data = obj;
  size_t len;
  size_t i;

  if (obj == NULL) {
    return hash;
  }

  for (i = 0; i < sizeof(objtbl_hash_t); i++) {
    hash = (hash ^ (seed & 0xff)) * FNV1A_PRIME;
    seed >>= 8;
  }

  len = saddr_len(addr);
  for (i = 0; i < len; i++) {
    hash = (hash ^ data[i]) * FNV1A_PRIME;
  }

  return hash;
}

static int addrcmp(const void *a, const void *b) {
  int ret;
  const union saddr_t *left = a;
  const union saddr_t *right = b;

  NULLCMP(a,b);
  if (left->sa.sa_family == AF_INET && right->sa.sa_family == AF_INET6) {
    return -1;
  } else if (left->sa.sa_family == AF_INET6 &&
      right->sa.sa_family == AF_INET) {
    return 1;
  } else if (left->sa.sa_family == right->sa.sa_family) {
    if (left->sa.sa_family == AF_INET6) {
      ret = memcmp(&left->sin6.sin6_addr, &right->sin6.sin6_addr,
          sizeof(left->sin6.sin6_addr));
      if (ret == 0) {
      ret = memcmp(&left->sin6.sin6_port, &right->sin6.sin6_port,
          sizeof(left->sin6.sin6_port));
      }
    } else {
      ret = memcmp(&left->sin.sin_addr, &right->sin.sin_addr,
          sizeof(left->sin.sin_addr));
      if (ret == 0) {
      ret = memcmp(&left->sin.sin_port, &right->sin.sin_port,
          sizeof(left->sin.sin_port));
      }
    }
    return -ret;
  }

  return 0; /* fallback */
}

static objtbl_hash_t svchash(const void *obj, objtbl_hash_t seed) {
  const struct collate_service *svc = obj;

  if (obj == NULL) {
    return FNV1A_OFFSET;
  }

  return addrhash(svc->addr, seed) ^ namehash(svc->name, seed);
}

static int svccmp(const void *a, const void *b) {
  const struct collate_service *left = a;
  const struct collate_service *right = b;
  int res;

  NULLCMP(a,b);
  res = namecmp(left->name, right->name);
  if (res == 0) {
    res = addrcmp(left->addr, right->addr);
  }

  return res;
}

static objtbl_hash_t chainhash(const void *obj, objtbl_hash_t seed) {
  const struct collate_certchain *chain = obj;
  objtbl_hash_t hash = FNV1A_OFFSET;
  size_t i;

  if (obj == NULL) {
    return hash;
  }

  for (i = 0; i < sizeof(objtbl_hash_t); i++) {
    hash = (hash ^ (seed & 0xff)) * FNV1A_PRIME;
    seed >>= 8;
  }

  for (i = 0; i < sizeof(chain->sha1hash); i++) {
    hash = (hash ^ chain->sha1hash[i]) * FNV1A_PRIME;
  }

  return hash;
}

static int chaincmp(const void *a, const void *b) {
  const struct collate_certchain *left = a;
  const struct collate_certchain *right = b;

  NULLCMP(a,b);
  return memcmp(left->sha1hash, right->sha1hash, sizeof(right->sha1hash));
}

static struct collate_certchain *upsert_chain(const char *hash,
    size_t hashlen, const char *pems, size_t pemslen) {

  int ret;
  void *val = NULL;
  struct collate_certchain *newchain;
  struct collate_certchain key;
  struct x509_certchain chain;

  if (hash == NULL || hashlen != SHA1_DSTLEN || pems == NULL ||
      pemslen == 0) {
    return NULL;
  }

  memcpy(key.sha1hash, hash, sizeof(key.sha1hash));
  ret = objtbl_get(&chaintbl_, &key, &val);
  if (ret == OBJTBL_ENOTFOUND) {
    ret = x509_certchain_from_mem(&chain, pems, pemslen);
    if (ret != X509_OK) {
      return NULL;
    }

    newchain = linvar_alloc(&varmem_, sizeof(struct collate_certchain));
    if (newchain == NULL) {
      x509_certchain_cleanup(&chain);
      return NULL;
    }

    memcpy(newchain->sha1hash, hash, sizeof(newchain->sha1hash));
    newchain->chain = chain;
    newchain->id = chainid_++;
    objtbl_insert(&chaintbl_, newchain);
  } else {
    newchain = val;
  }

  return newchain;
}

static const char *upsert_name(const char *name, size_t len) {
  int ret;
  void *val = NULL;
  char *res;

  if (name == NULL || len == 0) {
    return NULL;
  }

  ret = objtbl_get(&nametbl_, name, &val);
  if (ret == OBJTBL_ENOTFOUND) {
    res = linvar_alloc(&varmem_, len + 1);
    if (res == NULL) {
      return NULL;
    }
    memcpy(res, name, len + 1);
    objtbl_insert(&nametbl_, res);
  } else {
    res = val;
  }

  return res;
}

static union saddr_t *upsert_addr(struct sockaddr *saddr,
    socklen_t len) {
  int ret;
  void *val = NULL;
  union saddr_t *res;

  if (saddr == NULL || len <= 0 || len > sizeof(union saddr_t)) {
    return NULL;
  }

  /* validate supported address types and corresponding sizes */
  if (saddr->sa_family != AF_INET && saddr->sa_family != AF_INET6) {
    return NULL;
  } else if ((saddr->sa_family == AF_INET &&
        len != sizeof(struct sockaddr_in)) ||
      (saddr->sa_family == AF_INET6 &&
        len != sizeof(struct sockaddr_in6))) {
    return NULL;
  }

  ret = objtbl_get(&addrtbl_, saddr, &val);
  if (ret == OBJTBL_ENOTFOUND) {
    res = linvar_alloc(&varmem_, sizeof(union saddr_t));
    memcpy(res, saddr, len);
    objtbl_insert(&addrtbl_, res);
  } else {
    res = val;
  }

  return res;
}

static struct collate_service *process_banner(
    struct ycl_msg_banner *banner) {
  int ret;
  unsigned int i;
  struct collate_service key;
  void *val = NULL;
  struct collate_service *res;
  struct collate_certchain *chain = NULL;

  key.name = upsert_name(banner->name.data, banner->name.len);
  key.addr = upsert_addr((struct sockaddr *)banner->addr.data,
      (socklen_t)banner->addr.len);
  if (key.addr == NULL) {
    return NULL;
  }

  if (banner->chash.len == SHA1_DSTLEN && banner->certs.len > 0) {
    chain = upsert_chain(banner->chash.data, SHA1_DSTLEN,
        banner->certs.data, banner->certs.len);
    /* TODO: Validate name for chain? */
  }

  ret = objtbl_get(&svctbl_, &key, &val);
  if (ret == OBJTBL_ENOTFOUND) {
    res = linvar_alloc(&varmem_, sizeof(struct collate_service));
    res->addr = key.addr;
    res->name = key.name;
    res->eflags.fpid = banner->fpid;
    /* TODO: transport */
    objtbl_insert(&svctbl_, res);
  } else {
    res = val;
  }

  /* update mpid, unless mpid already exists or too many mpids are set  */
  /* TODO: Associate the chain with the fpid, or mpid if set */
  if (banner->mpid != TCPPROTO_UNKNOWN) {
    for (i = 0; i < MAX_MPIDS &&
        res->mpids[i] != TCPPROTO_UNKNOWN &&
        res->mpids[i] != banner->mpid;
        i++);
    if (i < MAX_MPIDS) {
      res->mpids[i] = banner->mpid;
      if (chain) {
        res->mpchains[i] = chain;
      }
    }
  } else if (chain != NULL && res->fpchain == NULL) {
    /* banner->mpid is unknown and we have a chain to set as fpid*/
    res->fpchain = chain;
  }

  return res;
}

static int print_services_csv(struct objtbl_ctx *svctbl,
    struct collate_opts *opts) {
  size_t i;
  size_t j;
  size_t k;
  size_t tbllen;
  buf_t buf;
  const char *fields[6];
  char addrbuf[128];
  char portbuf[8];
  struct collate_service *svc;
  socklen_t salen;
  int ret;
  int status = -1;
  char certid[24];
  struct collate_certchain *chain;

  if (!buf_init(&buf, 2048)) {
    return -1;
  }

  tbllen = objtbl_size(svctbl);
  for (i = 0; i < opts->nout_services_csv; i++) {
    for (j = 0; j < tbllen; j++) {
      svc = objtbl_val(svctbl, j);
      assert(svc != NULL);

      salen = saddr_len(svc->addr);
      ret = getnameinfo(&svc->addr->sa, salen, addrbuf, sizeof(addrbuf),
          portbuf, sizeof(portbuf), NI_NUMERICHOST | NI_NUMERICSERV);
      if (ret != 0) {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(ret));
        continue;
      }

      /* If no mpid exists, set the fpid as the mpid. FIXME:
       * There could be a cert in mpids[0] though, should step up index if
       * so*/
      if (svc->mpids[0] == TCPPROTO_UNKNOWN) {
        svc->mpids[0] = svc->eflags.fpid;
        svc->mpchains[0] = svc->fpchain;
      }

      for (k = 0; k < MAX_MPIDS && svc->mpids[k] != TCPPROTO_UNKNOWN; k++) {
        chain = svc->mpchains[k];
        if (chain != NULL && chain->id > 0) {
          snprintf(certid, sizeof(certid), "%u", chain->id);
        } else {
          certid[0] = '\0';
        }

        buf_clear(&buf);
        fields[0] = svc->name;
        fields[1] = addrbuf;
        fields[2] = "tcp";
        fields[3] = portbuf;
        fields[4] = tcpproto_type_to_string(svc->mpids[k]);
        fields[5] = certid;
        ret = csv_encode(&buf, fields, sizeof(fields) / sizeof(*fields));
        if (ret != 0) {
          goto end;
        }

        if (fwrite(buf.data, buf.len, 1, opts->out_services_csv[i]) != 1) {
          goto end;
        }
      }
    }
  }

  status = 0;
end:
  buf_cleanup(&buf);
  return status;
}

static int print_chain_csv(struct collate_certchain *certchain,
    struct collate_opts *opts) {
  size_t ncerts;
  size_t i;
  char chain_id[24];
  char depth[24];
  const char *fields[3];
  int ret;
  struct x509_certchain *chain;
  char *subject_name;
  int result = -1;
  buf_t buf;
  size_t nouts;
  size_t j;

  nouts = opts->nout_certs_csv;
  if (nouts == 0) {
    return 0;
  }

  if (buf_init(&buf, 2048) == NULL) {
    return -1;
  }

  snprintf(chain_id, sizeof(chain_id), "%u", certchain->id);
  chain = &certchain->chain;
  ncerts = x509_certchain_get_ncerts(chain);
  for (i = 0; i < ncerts; i++) {
    snprintf(depth, sizeof(depth), "%zu", i);
    subject_name = NULL;
    x509_certchain_get_subject_name(chain, i, &subject_name);

    fields[0] = chain_id;
    fields[1] = depth;
    fields[2] = subject_name;
    buf_clear(&buf);
    ret = csv_encode(&buf, fields, sizeof(fields) / sizeof(*fields));

    /* free per-iteration allocations before checking return status of
     * csv_encode */
    x509_certchain_free_data(chain, subject_name);

    if (ret != 0) {
      goto end;
    }

    for (j = 0; j < nouts; j++) {
      if (fwrite(buf.data, buf.len, 1, opts->out_certs_csv[j]) != 1) {
        goto end;
      }
    }
  }

  result = 0;
end:
  buf_cleanup(&buf);
  return result;

}

static int print_chains_csv(struct objtbl_ctx *chains,
    struct collate_opts *opts) {
  size_t tbllen;
  size_t i;
  size_t j;
  struct collate_certchain *chain;
  int result = 0;
  int ret;

  tbllen = objtbl_size(chains);
  for (i = 0; i < opts->nout_services_csv; i++) {
    for (j = 0; j < tbllen; j++) {
      chain = objtbl_val(chains, j);
      assert(chain != NULL);

      /* If printing the element(s) of one chain fails, we must still
       * iterate over the rest in order to free the memory associated with
       * them. Therefore, we only call print_chain_csv if we still have 
       * a zero (OK) result */
      if (result == 0) {
        ret = print_chain_csv(chain, opts);
        if (ret < 0) {
          result = -1;
        }
      }

      x509_certchain_cleanup(&chain->chain);
    }
  }

  return result;
}

static int banners(struct scan_ctx *ctx, struct collate_opts *opts) {
  size_t ibindex = 0;
  struct ycl_ctx ycl;
  struct ycl_msg msg;
  int status = EXIT_FAILURE;
  int ret;
  struct ycl_msg_banner banner;
  static struct objtbl_opts nametblopts = {
    .hashfunc = namehash,
    .cmpfunc = namecmp,
  };
  static struct objtbl_opts addrtblopts = {
    .hashfunc = addrhash,
    .cmpfunc = addrcmp,
  };
  static struct objtbl_opts svctblopts = {
    .hashfunc = svchash,
    .cmpfunc = svccmp,
  };
  static struct objtbl_opts chaintblopts = {
    .hashfunc = chainhash,
    .cmpfunc = chaincmp,
  };

  /* TODO: do better w/ regards to randomness */
  srand(time(NULL));
  addrtblopts.hashseed = nametblopts.hashseed = svctblopts.hashseed =
      rand();

  linvar_init(&varmem_, LINVAR_BLKSIZE);
  objtbl_init(&addrtbl_, &addrtblopts, NMEMB_ADDR);
  objtbl_init(&nametbl_, &nametblopts, NMEMB_NAME);
  objtbl_init(&svctbl_, &svctblopts, NMEMB_SVCS);
  objtbl_init(&chaintbl_, &chaintblopts, NMEMB_CERTCHAINS);
  ycl_init(&ycl, YCL_NOFD);
  ycl_msg_init(&msg);

  for (ibindex = 0; ibindex < opts->nin_banners; ibindex++) {
    while (ycl_readmsg(&ycl, &msg, opts->in_banners[ibindex]) == YCL_OK) {
      ret = ycl_msg_parse_banner(&msg, &banner);
      if (ret != YCL_OK) {
        goto end;
      }

      process_banner(&banner);
    }
  }

  objtbl_sort(&svctbl_); /* XXX: Destructive operation */
  ret = print_services_csv(&svctbl_, opts);
  if (ret < 0) {
    goto end;
  }

  objtbl_sort(&chaintbl_); /* FIXME: Currently, this sorts by sha1hash
                            *        which is always set, if unused  too */
  ret = print_chains_csv(&chaintbl_, opts);
  if (ret < 0) {
    goto end;
  }

  status = EXIT_SUCCESS;
end:
  ycl_msg_cleanup(&msg);
  ycl_close(&ycl);
  objtbl_cleanup(&svctbl_);
  objtbl_cleanup(&nametbl_);
  objtbl_cleanup(&addrtbl_);
  linvar_cleanup(&varmem_);
  return status;
}

enum collate_type collate_type_from_str(const char *str) {
  static const struct {
    char *name;
    enum collate_type type;
  } map[] = {
    {"banners", COLLATE_BANNERS},
  };
  size_t i;

  for (i = 0; i < sizeof(map) / sizeof(*map); i++) {
    if (strcmp(map[i].name, str) == 0) {
      return map[i].type;
    }
  }

  return COLLATE_UNKNOWN;
}

static int open_io(struct collate_opts *opts, struct opener_ctx *opener,
    const char *path, const char *mode, FILE **dst, size_t *ndst) {

  FILE *fp;
  int ret;

  if (*ndst >= MAX_INOUTS || opts->nclosefps >= MAX_FOPENS) {
    fprintf(stderr, "too many inputs\n");
    return -1;
  }

  ret = opener_fopen(opener, path, mode, &fp);
  if (ret < 0) {
    fprintf(stderr, "%s: %s\n", optarg, opener_strerr(opener));
    return -1;
  }

  dst[*ndst] = fp;
  opts->closefps[opts->nclosefps++] = fp;
  *ndst = *ndst + 1;
  return 0;
}

typedef int (*collate_func_t)(struct scan_ctx *, struct collate_opts *);

static void usage(const char *argv0) {
  fprintf(stderr, "usage: %s collate <opts>\n"
      "opts:\n"
      "  -t|--type <name>\n"
      "      Collation type\n"
      "  -B|--in-banners <path>\n"
      "      Banner input\n"
      "  -s|--out-services-csv <path>\n"
      "      Services CSV output\n"
      "  -X|--no-sandbox\n"
      "      Disable sandbox\n"
      ,argv0);
}

int collate_main(struct scan_ctx *scan, int argc, char **argv) {
  const char *tmpstr;
  const char *argv0;
  const char *optstr = "t:B:s:c:X";
  static struct collate_opts opts;
  collate_func_t func;
  collate_func_t funcs[COLLATE_MAX] = {
    [COLLATE_BANNERS]  = banners,
  };
  /* short opts: uppercase letters for inputs, if sane to do so */
  static const struct option lopts[] = {
    {"type",             required_argument, NULL, 't'},
    {"in-banners",       required_argument, NULL, 'B'},
    {"out-services-csv", required_argument, NULL, 's'},
    {"out-certs-csv",    required_argument, NULL, 'c'},
    {"no-sandbox",       no_argument,       NULL, 'X'},
    {NULL, 0, NULL, 0}};
  int ch;
  int status = EXIT_FAILURE;
  int ret;
  int i;
  int sandbox = 1;

  argv0 = argv[0];
  argc--;
  argv++;

  while ((ch = getopt_long(argc, argv, optstr, lopts, NULL)) != -1) {
    switch (ch) {
    case 't': /* type */
      opts.type = collate_type_from_str(optarg);
      break;
    case 'B': /* in-banners */
      ret = open_io(&opts, &scan->opener, optarg, "rb",
          opts.in_banners, &opts.nin_banners);
      if (ret < 0) {
        goto end;
      }
      break;
    case 's': /* out-services-csv */
      ret = open_io(&opts, &scan->opener, optarg, "wb",
          opts.out_services_csv, &opts.nout_services_csv);
      if (ret < 0) {
        goto end;
      }

      tmpstr = "Name,Address,Transport,Port,Service,Certificate Chain\r\n";
      fwrite(tmpstr, 1, strlen(tmpstr),
          opts.out_services_csv[opts.nout_services_csv-1]);
      break;
    case 'c': /* out-certs-csv */
      ret = open_io(&opts, &scan->opener, optarg, "wb",
          opts.out_certs_csv, &opts.nout_certs_csv);
      if (ret < 0) {
        goto end;
      }

      tmpstr = "Chain,Depth,Subject Name\r\n";
      fwrite(tmpstr, 1, strlen(tmpstr),
          opts.out_certs_csv[opts.nout_certs_csv-1]);
      break;
    case 'X':
      sandbox = 0;
      break;
    default:
      usage(argv0);
      goto end;
    }
  }

  if (opts.type <= COLLATE_UNKNOWN || opts.type >= COLLATE_MAX) {
    fprintf(stderr, "missing or invalid collation\n");
    usage(argv0);
    goto end;
  }

  func = funcs[opts.type];
  if (func == NULL) {
    fprintf(stderr, "collation %d has no callback\n", opts.type);
    goto end;
  }

  if (sandbox) {
    ret = sandbox_enter();
    if (ret != 0) {
      fprintf(stderr, "failed to enter sandbox mode\n");
      goto end;
    }
  } else {
    fprintf(stderr, "warning: sandbox disabled\n");
  }

  status = func(scan, &opts);
end:
  for (i = 0; i < opts.nclosefps; i++) {
    fclose(opts.closefps[i]);
  }

  return status;
}
