#include <lib/match/reset.h>

/* This is its own file because it doesn't depend on anything else and
 * matchgen needs it but does not depend on the re2 stuff */

const char *reset_type2str(enum reset_match_type t) {
  /* The strings in here must correspond to the actual enum string */
  switch (t) {
  case RESET_MATCH_COMPONENT:
    return "RESET_MATCH_COMPONENT";
  case RESET_MATCH_UNKNOWN:
  default:
    return "RESET_MATCH_UNKNOWN";
  }
}

