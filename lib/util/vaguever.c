#include <string.h>
#include <stdio.h>
#include <lib/util/macros.h>
#include <lib/util/vaguever.h>

void vaguever_init(struct vaguever_version *v, const char *str) {
  int i = 0;
  int ch;
  const char *curr = str;
  int val = 0;
  int hasval = 0;

  memset(v, 0, sizeof(*v)); /* always clear: unset fields == 0 */
  do {
    ch = *curr++;
    switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      /* NB: ignore integer overflow */
      val *= 10;
      val += ch - '0';
      hasval = 1;
      break;
    case '.':
    default:
      if (hasval) {
        v->fields[i] = val;
        i++;
      }
      val = 0;
      hasval = 0;
      if (ch != '.' || i >= VAGUEVER_NFIELDS) {
        goto done;
      }
      break;
    }
  } while (ch != '\0');

done:
  v->nused = MAX(i, 1);
}

void vaguever_str(struct vaguever_version *v, char *out, size_t len) {
  switch(v->nused) {
  case 1:
    snprintf(out, len, "%u", v->fields[VAGUEVER_MAJOR]);
    break;
  case 2:
    snprintf(out, len, "%u.%u", v->fields[VAGUEVER_MAJOR],
        v->fields[VAGUEVER_MINOR]);
    break;
  case 3:
    snprintf(out, len, "%u.%u.%u", v->fields[VAGUEVER_MAJOR],
        v->fields[VAGUEVER_MINOR], v->fields[VAGUEVER_PATCH]);
    break;
  case 4:
  default:
    snprintf(out, len, "%u.%u.%u.%u", v->fields[VAGUEVER_MAJOR],
        v->fields[VAGUEVER_MINOR], v->fields[VAGUEVER_PATCH],
        v->fields[VAGUEVER_BUILD]);
    break;
  }
}

int vaguever_cmp(struct vaguever_version *v1,
    struct vaguever_version *v2) {
  int i;
  int diff;

  for (i = 0; i < VAGUEVER_NFIELDS; i++) {
    diff = v1->fields[i] - v2->fields[i];
    if (diff != 0) {
      return diff;
    }
  }

  return 0;
}
