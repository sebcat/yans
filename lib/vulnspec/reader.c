#include <stdio.h>
#include <stdbool.h>
#include <string.h>

#include "lib/vulnspec/vulnspec.h"

static int vm_getc(struct vulnspec_reader *r) {
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

static int vm_ungetc(struct vulnspec_reader *r, int ch) {
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

static enum vulnspec_token read_number(struct vulnspec_reader *r,
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
        return VULNSPEC_TINVALID;
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
        return VULNSPEC_TINVALID; /* overflow */
      }
      ival = tmp;
      break;
    default:
      /* should be unreachable */
      vm_ungetc(r, ch);
      return VULNSPEC_TINVALID;
    }
  }

  if (negate) {
    ival = -ival;
  }

  if (fpdiv != 0) {
    r->num.dval = (double)ival / fpdiv;
    return VULNSPEC_TDOUBLE;
  }

  r->num.ival = ival;
  return VULNSPEC_TLONG;
}

static enum vulnspec_token read_string(struct vulnspec_reader *r) {
  int ch;

  buf_clear(&r->sval);
  /* NB: leading " is already consumed */
  while ((ch = vm_getc(r)) != EOF) {
    if (ch == '"') {
      buf_achar(&r->sval, '\0');
      return VULNSPEC_TSTRING;
    } else if (ch == '\\') {
      ch = vm_getc(r);
      if (ch == EOF) {
        return VULNSPEC_TINVALID;
      }
    }
    buf_achar(&r->sval, ch);
  }

  return VULNSPEC_TINVALID;
}

int vulnspec_reader_init(struct vulnspec_reader *r, FILE *input) {
  memset(r, 0, sizeof(*r));
  r->input = input;
  buf_init(&r->sval, 8192);
  return 0;
}

void vulnspec_reader_cleanup(struct vulnspec_reader *r) {
  if (r != NULL) {
    buf_cleanup(&r->sval);
  }
}

enum vulnspec_token vulnspec_read_token(struct vulnspec_reader *r) {
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
        return VULNSPEC_TLPAREN;
      case ')':
        return VULNSPEC_TRPAREN;
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
        return VULNSPEC_TOR;
      case '^':
        return VULNSPEC_TAND;
      case '<':
        next = vm_getc(r);
        if (next == '=') {
          return VULNSPEC_TLE;
        }

        vm_ungetc(r, next);
        return VULNSPEC_TLT;
      case '=':
        return VULNSPEC_TEQ;
      case '>':
        next = vm_getc(r);
        if (next == '=') {
          return VULNSPEC_TGE;
        }

        vm_ungetc(r, next);
        return VULNSPEC_TGT;
      case 'c':
        next = vm_getc(r);
        if (next != 'v') {
          vm_ungetc(r, next);
          return VULNSPEC_TINVALID;
        }

        next = vm_getc(r);
        if (next != 'e') {
          vm_ungetc(r, next);
          return VULNSPEC_TINVALID;
        }

        return VULNSPEC_TCVE;
      default:
        vm_ungetc(r, ch);
        return VULNSPEC_TINVALID;
    }
  }

  return VULNSPEC_TEOF;
}

const char *vulnspec_token2str(enum vulnspec_token t) {
  switch(t) {
  case VULNSPEC_TINVALID:
    return "VULNSPEC_TINVALID";
  case VULNSPEC_TEOF:
    return "VULNSPEC_TEOF";
  case VULNSPEC_TLPAREN:
    return "VULNSPEC_TLPAREN";
  case VULNSPEC_TRPAREN:
    return "VULNSPEC_TRPAREN";
  case VULNSPEC_TSTRING:
    return "VULNSPEC_TSTRING";
  case VULNSPEC_TLONG:
    return "VULNSPEC_TLONG";
  case VULNSPEC_TDOUBLE:
    return "VULNSPEC_TDOUBLE";
  case VULNSPEC_TOR:
    return "VULNSPEC_TOR";
  case VULNSPEC_TAND:
    return "VULNSPEC_TAND";
  case VULNSPEC_TLT:
    return "VULNSPEC_TLT";
  case VULNSPEC_TLE:
    return "VULNSPEC_TLE";
  case VULNSPEC_TEQ:
    return "VULNSPEC_TEQ";
  case VULNSPEC_TGE:
    return "VULNSPEC_TGE";
  case VULNSPEC_TGT:
    return "VULNSPEC_TGT";
  case VULNSPEC_TCVE:
    return "VULNSPEC_TCVE";
  default:
    return "???";
  }
}
