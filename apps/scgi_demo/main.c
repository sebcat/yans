#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <lib/util/sc2mod.h>
#include <lib/net/scgi.h>

SC2MOD_API int sc2_setup(struct sc2mod_ctx *mod) {
  sc2mod_set_data(mod, "icanhasdata");
  return 0;
}

SC2MOD_API int sc2_handler(struct sc2mod_ctx *mod) {
  struct scgi_ctx ctx = {0};
  struct scgi_header hdr = {0};
  int ret;
  int status = EXIT_FAILURE;

  ret = scgi_init(&ctx, fileno(stdin), SCGI_DEFAULT_MAXHDRSZ);
  if (ret != SCGI_OK) {
    fprintf(stderr, "scgi_init: %s\n", scgi_strerror(ret));
    goto end;
  }

  while ((ret = scgi_read_header(&ctx)) == SCGI_AGAIN);
  if (ret != SCGI_OK) {
    fprintf(stderr, "scgi_read_header: %s\n", scgi_strerror(ret));
    goto cleanup_ctx;
  }

  ret = scgi_parse_header(&ctx);
  if (ret != SCGI_OK) {
    fprintf(stderr, "scgi_parse_header: %s\n", scgi_strerror(ret));
    goto cleanup_ctx;
  }

  printf("Status: 200 OK\r\nContent-Type: text/plain\r\n\r\n");
  while ((ret = scgi_get_next_header(&ctx, &hdr)) == SCGI_AGAIN) {
    printf("%s: %s\n", hdr.key, hdr.value);
  }

  if (ret != SCGI_OK) {
    fprintf(stderr, "scgi_get_next_header: %s\n", scgi_strerror(ret));
    goto cleanup_ctx;
  }

  printf("data: %s\n", sc2mod_data(mod));
  status = EXIT_SUCCESS;
cleanup_ctx:
  scgi_cleanup(&ctx);
end:
  return status;
}
