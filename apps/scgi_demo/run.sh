#!/bin/sh

# example nginx config
# location /trololo {
#     include   scgi_params;
#     scgi_pass 127.0.0.1:9000;
# }

if [ -z "$USE_SOCAT" ]; then
  printf '17:FOO\0BAR\0BAZ\0WIIE\0,' | ./scgi_demo
else
  socat tcp4-listen:9000,bind=127.0.0.1,reuseaddr,fork EXEC:./scgi_demo
fi
