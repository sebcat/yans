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
#ifndef TCPPROTO_TYPES_H__
#define TCPPROTO_TYPES_H__

/* When adding tcpprotos to this enum: add them at the end, right
 * before NBR_OF_TCPPROTOS. This keeps the values consistent. */
enum tcpproto_type {
  TCPPROTO_UNKNOWN = 0,
  TCPPROTO_SMTP,
  TCPPROTO_SMTPS,
  TCPPROTO_DNS,
  TCPPROTO_HTTP,
  TCPPROTO_HTTPS,
  TCPPROTO_POP3,
  TCPPROTO_POP3S,
  TCPPROTO_IMAP,
  TCPPROTO_IMAPS,
  TCPPROTO_IRC,
  TCPPROTO_IRCS,
  TCPPROTO_FTP,
  TCPPROTO_FTPS,
  TCPPROTO_SSH,
  NBR_OF_TCPPROTOS,
};

const char *tcpproto_type_to_string(enum tcpproto_type t);
enum tcpproto_type tcpproto_type_from_port(unsigned short port);

#endif

