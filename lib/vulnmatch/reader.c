#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lib/vulnmatch/vulnmatch.h"

static int vm_getc(struct vulnmatch_reader *r) {
  int ret;

  ret = getc(r->input);
  if (ret == '\n') {
    r->row++;
    r->lastcol = r->col;
    r->col = 0;
  } else if (ret != EOF) {
    r->col++;
  }

  return ret;
}

static int vm_ungetc(struct vulnmatch_reader *r, int ch) {
  int ret;

  ret = ungetc(ch, r->input);
  if (ret != EOF) {
    if (r->col == 0) {
      /* NB: assumes look-ahead of at most one character, otherwise we need
         a stack of lastcol values */
      r->col = r->lastcol;
      r->row--;
    } else {
      r->col--;
    }
  }

  return ret;
}

static enum vulnmatch_token read_number(struct vulnmatch_reader *r,
    int first) {
  long ival = 0;
  long tmp;
  bool negate = false;
  int fpdiv = 0;
  int ch;

  if (first >= '0' && first <= '9') {
    ival = first - '0';
  } else if (first == '-') {
    negate = true;
  } else if (first == '.') {
    fpdiv = 1;
  }

  while ((ch = vm_getc(r)) != EOF) {
    if (ch != '.' && (ch < '0' || ch > '9')) {
      vm_ungetc(r, ch);
      break;
    }

    fpdiv *= 10;
    switch (ch) {
    case '.':
      if (fpdiv != 0) {
        return VULNMATCH_TINVALID;
      }
      fpdiv = 1;
      break;
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
      tmp = ival * 10;
      tmp += ch - '0';
      if (tmp < ival) {
        return VULNMATCH_TINVALID; /* overflow */
      }
      ival = tmp;
      break;
    default:
      /* should be unreachable */
      vm_ungetc(r, ch);
      return VULNMATCH_TINVALID;
    }
  }

  if (negate) {
    ival = -ival;
  }

  if (fpdiv != 0) {
    r->num.dval = (double)ival / fpdiv;
    return VULNMATCH_TDOUBLE;
  }

  r->num.ival = ival;
  return VULNMATCH_TLONG;
}

static enum vulnmatch_token read_string(struct vulnmatch_reader *r) {
  int ch;

  buf_clear(&r->sval);
  /* NB: leading " is already consumed */
  while ((ch = vm_getc(r)) != EOF) {
    if (ch == '"') {
      buf_achar(&r->sval, '\0');
      return VULNMATCH_TSTRING;
    } else if (ch == '\\') {
      ch = vm_getc(r);
      if (ch == EOF) {
        return VULNMATCH_TINVALID;
      }
    }
    buf_achar(&r->sval, ch);
  }

  return VULNMATCH_TINVALID;
}

int vulnmatch_reader_init(struct vulnmatch_reader *r, FILE *input) {
  memset(r, 0, sizeof(*r));
  r->input = input;
  buf_init(&r->sval, 8192);
  return 0;
}

void vulnmatch_reader_cleanup(struct vulnmatch_reader *r) {
  if (r != NULL) {
    buf_cleanup(&r->sval);
  }
}

enum vulnmatch_token vulnmatch_read_token(struct vulnmatch_reader *r) {
  int ch;
  int next;

  while ((ch = vm_getc(r)) != EOF) {
    switch (ch) {
      case ' ':
      case '\t':
      case '\r':
      case '\n':
        /* consume whitespace */
        break;
      case '(':
        return VULNMATCH_TLPAREN;
      case ')':
        return VULNMATCH_TRPAREN;
      case '.':
      case '-':
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
        return read_number(r, ch);
      case '"':
        return read_string(r);
      case 'v':
        return VULNMATCH_TOR;
      case '^':
        return VULNMATCH_TAND;
      case '<':
        next = vm_getc(r);
        if (next == '=') {
          return VULNMATCH_TLE;
        }

        vm_ungetc(r, next);
        return VULNMATCH_TLT;
      case '=':
        return VULNMATCH_TEQ;
      case '>':
        next = vm_getc(r);
        if (next == '=') {
          return VULNMATCH_TGE;
        }

        vm_ungetc(r, next);
        return VULNMATCH_TGT;
      case 'c':
        next = vm_getc(r);
        if (next != 'v') {
          vm_ungetc(r, next);
          return VULNMATCH_TINVALID;
        }

        next = vm_getc(r);
        if (next != 'e') {
          vm_ungetc(r, next);
          return VULNMATCH_TINVALID;
        }

        return VULNMATCH_TCVE;
      default:
        vm_ungetc(r, ch);
        return VULNMATCH_TINVALID;
    }
  }

  return VULNMATCH_TEOF;
}

const char *vulnmatch_token2str(enum vulnmatch_token t) {
  switch(t) {
  case VULNMATCH_TINVALID:
    return "VULNMATCH_TINVALID";
  case VULNMATCH_TEOF:
    return "VULNMATCH_TEOF";
  case VULNMATCH_TLPAREN:
    return "VULNMATCH_TLPAREN";
  case VULNMATCH_TRPAREN:
    return "VULNMATCH_TRPAREN";
  case VULNMATCH_TSTRING:
    return "VULNMATCH_TSTRING";
  case VULNMATCH_TLONG:
    return "VULNMATCH_TLONG";
  case VULNMATCH_TDOUBLE:
    return "VULNMATCH_TDOUBLE";
  case VULNMATCH_TOR:
    return "VULNMATCH_TOR";
  case VULNMATCH_TAND:
    return "VULNMATCH_TAND";
  case VULNMATCH_TLT:
    return "VULNMATCH_TLT";
  case VULNMATCH_TLE:
    return "VULNMATCH_TLE";
  case VULNMATCH_TEQ:
    return "VULNMATCH_TEQ";
  case VULNMATCH_TGE:
    return "VULNMATCH_TGE";
  case VULNMATCH_TGT:
    return "VULNMATCH_TGT";
  case VULNMATCH_TCVE:
    return "VULNMATCH_TCVE";
  default:
    return "???";
  }
}
