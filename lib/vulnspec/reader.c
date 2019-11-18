/* Copyright (c) 2019 Sebastian Cato
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
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

static enum vulnspec_token read_symbol(struct vulnspec_reader *r) {
  int ch;
  unsigned int offset = 0;

  for (;;) {
    ch = vm_getc(r);
    switch(ch) {
      case '(':
      case ')':
      case ' ':
      case '\r':
      case '\n':
      case '\t':
        vm_ungetc(r, ch);
        /* fallthrough */
      case EOF:
        r->symbol[offset] = '\0';
        return VULNSPEC_TSYMBOL;
      default:
        r->symbol[offset++] = ch;
        if (offset >= sizeof(r->symbol)-1) {
          return VULNSPEC_TINVALID;
        }
        break;
    }
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
      default:
        vm_ungetc(r, ch);
        return read_symbol(r);
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
  case VULNSPEC_TSYMBOL:
    return "VULNSPEC_TSYMBOL";
  default:
    return "???";
  }
}
