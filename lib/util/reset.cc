#include <cstddef>
#include <climits>
#include <new>
#include <algorithm>
#include <vector>

#include <re2/set.h>

#include <lib/util/reset.h>

using namespace re2;

class Pattern {
public:
  Pattern(std::string &re) : pstr_(re) {};
  Pattern(const char *resp) : pstr_(resp) {};

private:
  std::string pstr_;
};

struct reset_t {
  RE2::Set *set;
  std::vector<Pattern> patterns;
  std::vector<int> matches;
  std::size_t curr_match_index;
  std::string errstr;
};

reset_t *reset_new() {
  reset_t *reset;
  RE2::Set *set;
  RE2::Options opts;

  opts.set_dot_nl(true);       /* Allow '.' to match newline */
  opts.set_max_mem(INT_MAX);   /* INT_MAX sounds high enough */
  opts.set_posix_syntax(true); /* enforce POSIX ERE syntax */
  set = new(std::nothrow) RE2::Set(opts, RE2::Anchor::UNANCHORED);
  if (set == NULL) {
    return NULL;
  }

  reset = new(std::nothrow) reset_t;
  if (reset == NULL) {
    delete set;
    return NULL;
  }

  reset->set = set;
  return reset;
}

void reset_free(reset_t *reset) {

  if (reset != NULL) {
    delete reset->set;
    reset->set = NULL;
    delete reset;
  }
}

const char *reset_strerror(reset_t *reset) {
  return reset->errstr.c_str();
}

int reset_add(reset_t *reset, const char *re) {
  string err;
  int res;
  StringPiece resp(re);

  /* Add a regular expression to the RE2::Set, as well as to the
   * pattern vector. The ID allocation for the RE2::Set and the pattern
   * vector indices should be consistent. */

  res = reset->set->Add(resp, &reset->errstr);
  if (res < 0) {
    return RESET_ERR;
  }

  reset->patterns.push_back(Pattern(re));
  return res;
}

int reset_compile(reset_t *reset) {
  if (!reset->set->Compile()) {
    reset->errstr = "memory limit exceeded";
    return RESET_ERR;
  }

  return RESET_OK;
}

int reset_match(reset_t *reset, const char *data, size_t datalen) {
  StringPiece resp(data, datalen);

  reset->matches.clear();
  reset->curr_match_index = 0;
  if (!reset->set->Match(resp, &reset->matches)) {
    reset->errstr = "no matches";
    return RESET_ERR;
  }

  return RESET_OK;
}

int reset_get_next_match(reset_t *reset) {
  int res;

  if (reset->curr_match_index >= reset->matches.size() ||
      reset->curr_match_index > INT_MAX) {
    return -1;
  }

  res = reset->matches[reset->curr_match_index];
  reset->curr_match_index++;
  return res;
}
