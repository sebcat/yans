#include <cstddef>
#include <climits>
#include <new>
#include <algorithm>
#include <vector>

#include <re2/set.h>

#include <lib/match/reset.h>

using namespace re2;

class Pattern;

struct reset_t {
  RE2::Set *set;
  std::vector<Pattern> patterns;
  std::vector<int> matches;
  std::size_t curr_match_index;
  std::string errstr;
  std::string substring;
};

class Pattern {
public:
  Pattern(const char *re, enum reset_match_type mtype, const char *name) :
      pstr_(re), mtype_(mtype), name_(name), matcher_(nullptr) {};
  ~Pattern();
  const char *name() { return name_.c_str(); }
  enum reset_match_type type() { return mtype_; }
  bool substring(const char *data, size_t len, string *s);

private:
  std::string pstr_;
  enum reset_match_type mtype_;
  std::string name_;
  re2::RE2 *matcher_;
};

static void init_options(RE2::Options& opts) {
  opts.set_dot_nl(true);        /* Allow '.' to match newline */
  opts.set_max_mem(INT_MAX);    /* INT_MAX sounds high enough */
  opts.set_posix_syntax(true);  /* enforce POSIX ERE syntax */
}

Pattern::~Pattern() {
  if (matcher_) {
    delete matcher_;
    matcher_ = nullptr;
  }
}

bool Pattern::substring(const char *data, size_t len, string *s) {
  int nsubgroups;
  bool matched;
  re2::StringPiece sub[2];

  if (matcher_ == nullptr) {
    RE2::Options opts;
    init_options(opts);
    matcher_ = new(std::nothrow) re2::RE2(pstr_, opts);
    if (matcher_ == nullptr) {
      return false;
    }
  }

  nsubgroups = matcher_->NumberOfCapturingGroups();
  if (nsubgroups < 1) {
    return false;
  }

  matched = matcher_->Match(data, 0, len, RE2::UNANCHORED, sub, 2);
  if (!matched) {
    return false;
  }

  s->assign(sub[1].data(), sub[1].length());
  return true;
}

const char *reset_strerror(reset_t *reset) {
  return reset->errstr.c_str();
}

int reset_error(struct reset_t *reset, const char *errstr) {
  reset->errstr.assign(errstr);
  return RESET_ERR;
}

reset_t *reset_new() {
  reset_t *reset;
  RE2::Set *set;
  RE2::Options opts;

  init_options(opts);
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

int reset_add_type_name_pattern(reset_t *reset, enum reset_match_type mtype,
    const char *name, const char *re) {
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

  if (name == NULL) {
    name = "";
  }

  reset->patterns.push_back(Pattern(re, mtype, name));
  return res;
}

int reset_add_name_pattern(reset_t *reset, const char *name,
    const char *re) {
  return reset_add_type_name_pattern(reset, RESET_MATCH_UNKNOWN,
      name, re);
}

int reset_add_pattern(reset_t *reset, const char *re) {
  return reset_add_type_name_pattern(reset, RESET_MATCH_UNKNOWN,
      NULL, re);
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

const char *reset_get_name(reset_t *reset, int id) {
  if (id < 0 || id >= (int)reset->patterns.size()) {
    return "";
  }

  return reset->patterns[id].name();
}

enum reset_match_type reset_get_type(reset_t *reset, int id) {
  if (id < 0 || id >= (int)reset->patterns.size()) {
    return RESET_MATCH_UNKNOWN;
  }

  return reset->patterns[id].type();
}

const char *reset_get_substring(reset_t *reset, int id, const char *data,
    size_t len, size_t *ol) {
  bool matched;

  if (id < 0 || id >= (int)reset->patterns.size()) {
    return NULL;
  }

  matched = reset->patterns[id].substring(data, len, &reset->substring);
  if (matched) {
    if (ol) {
      *ol = reset->substring.length();
    }
    return reset->substring.c_str();
  }

  return NULL;
}
