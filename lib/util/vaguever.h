#ifndef YANS_VAGUEVER_H__
#define YANS_VAGUEVER_H__

/* a lot of different versioning schemes exists, and they differ in subtle
 * ways. This code is an attempt to provide a version parser that can be
 * used for semver-like versioning schemes as a fallback. */

enum vaguever_fields {
  VAGUEVER_MAJOR = 0,
  VAGUEVER_MINOR,
  VAGUEVER_PATCH,
  VAGUEVER_BUILD,
  VAGUEVER_NFIELDS
};

struct vaguever_version {
  int nused;
  int fields[VAGUEVER_NFIELDS];
};

void vaguever_init(struct vaguever_version *v, const char *str);
void vaguever_str(struct vaguever_version *v, char *out, size_t len);
int vaguever_cmp(struct vaguever_version *v1, struct vaguever_version *v2);

#endif
