(module punycode (punycode-encode)
  (import chicken scheme foreign srfi-13)
  (foreign-declare "#include <lib/net/punycode.h>")

  (define (punycode-encode str)
    ((foreign-lambda c-string* punycode_encode (const nonnull-c-string) size_t)
        str (string-length str))))
