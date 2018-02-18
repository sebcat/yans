#include <lib/util/ylog.h>

#include <lib/net/route.h>
#include <lib/net/iface.h>
#include <lib/net/neigh.h>
#include <apps/clid/routes.h>

#define CLIFLAG_HASMSGBUF (1 << 0)

#define ROUTES_CLIENT(cli__) \
  ((struct routes_client *)((cli__)->udata))

void on_read_req(struct eds_client *cli, int fd) {
  struct ycl_msg_route_req req;
  struct ycl_msg_route_resp resp = {0};
  struct routes_client *ecli = ROUTES_CLIENT(cli);
  struct route_table rt = {0};
  struct iface_entries ifs = {0};
  struct neigh_ip4_entry *ip4_neigh = NULL;
  struct neigh_ip6_entry *ip6_neigh = NULL;
  size_t nip4_neigh = 0;
  size_t nip6_neigh = 0;
  char errbuf[128];
  int ret;

  ret = ycl_recvmsg(&ecli->ycl, &ecli->msgbuf);
  if (ret == YCL_AGAIN) {
    return;
  } else if (ret != YCL_OK) {
    ylog_error("routescli%d: ycl_recvmsg: %s", fd, ycl_strerror(&ecli->ycl));
    goto done;
  }

  ret = ycl_msg_parse_route_req(&ecli->msgbuf, &req);
  if (ret != YCL_OK) {
    ylog_error("routescli%d: invalid request message", fd);
    goto done;
  }

  ret = route_table_init(&rt);
  if (ret < 0) {
    route_table_strerror(&rt, errbuf, sizeof(errbuf));
    resp.err = errbuf;
    goto write_resp;
  }

  ret = iface_init(&ifs);
  if (ret < 0) {
    resp.err = iface_strerror(&ifs);
    goto write_resp;
  }

  ip4_neigh = neigh_get_ip4_entries(&nip4_neigh, NULL);
  ip6_neigh = neigh_get_ip6_entries(&nip6_neigh, NULL);

write_resp:
  resp.ip4_routes.data = (const char *)rt.entries_ip4;
  resp.ip4_routes.len = sizeof(*rt.entries_ip4) * rt.nentries_ip4;
  resp.ip4_srcs.data = (const char *)ifs.ip4srcs;
  resp.ip4_srcs.len = sizeof(*ifs.ip4srcs) * ifs.nip4srcs;
  resp.ip6_routes.data = (const char *)rt.entries_ip6;
  resp.ip6_routes.len = sizeof(*rt.entries_ip6) * rt.nentries_ip6;
  resp.ip6_srcs.data = (const char *)ifs.ip6srcs;
  resp.ip6_srcs.len = sizeof(*ifs.ip6srcs) * ifs.nip6srcs;
  resp.ifaces.data = (const char *)ifs.ifaces;
  resp.ifaces.len = sizeof(*ifs.ifaces) * ifs.nifaces;
  if (nip4_neigh > 0) {
    resp.ip4_neigh.data = (const char *)ip4_neigh;
    resp.ip4_neigh.len = nip4_neigh * sizeof(struct neigh_ip4_entry);
  }
  if (nip6_neigh > 0) {
    resp.ip6_neigh.data = (const char *)ip6_neigh;
    resp.ip6_neigh.len = nip6_neigh * sizeof(struct neigh_ip6_entry);
  }
  ret = ycl_msg_create_route_resp(&ecli->msgbuf, &resp);
  iface_cleanup(&ifs);
  route_table_cleanup(&rt);
  if (ip4_neigh != NULL) {
    neigh_free_ip4_entries(ip4_neigh);
  }
  if (ip6_neigh != NULL) {
    neigh_free_ip6_entries(ip6_neigh);
  }
  if (ret != YCL_OK) {
    ylog_error("routescli%d: response message creation failure", fd);
    goto done;
  }

  /* success */
  eds_client_clear_actions(cli);
  eds_client_send(cli, ycl_msg_bytes(&ecli->msgbuf),
      ycl_msg_nbytes(&ecli->msgbuf), NULL);
  return;
done:
  eds_client_clear_actions(cli);
}

void routes_on_readable(struct eds_client *cli, int fd) {
  int ret;
  struct routes_client *ecli = ROUTES_CLIENT(cli);

  ycl_init(&ecli->ycl, fd);
  if (!(ecli->flags & CLIFLAG_HASMSGBUF)) {
    ret = ycl_msg_init(&ecli->msgbuf);
    if (ret != YCL_OK) {
      ylog_error("routescli%d: ycl_msg_init failure", fd);
      goto fail;
    }
    ecli->flags |= CLIFLAG_HASMSGBUF;
  } else {
    ycl_msg_reset(&ecli->msgbuf);
  }

  eds_client_set_on_readable(cli, on_read_req, 0);
  return;
fail:
  eds_client_clear_actions(cli);
}

void routes_on_done(struct eds_client *cli, int fd) {
  ylog_info("routescli%d: done", fd);
}

void routes_on_finalize(struct eds_client *cli) {
  struct routes_client *ecli = ROUTES_CLIENT(cli);
  if (ecli->flags & CLIFLAG_HASMSGBUF) {
    ycl_msg_cleanup(&ecli->msgbuf);
    ecli->flags &= ~(CLIFLAG_HASMSGBUF);
  }
}
