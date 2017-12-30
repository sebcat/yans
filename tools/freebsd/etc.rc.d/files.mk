
GENERATED_RCFILESIN_FreeBSD = \
    tools/freebsd/etc.rc.d/ethd.in \
    tools/freebsd/etc.rc.d/stored.in \
    tools/freebsd/etc.rc.d/clid.in

GENERATED_RCFILES_FreeBSD = ${GENERATED_RCFILESIN_FreeBSD:.in=}

GENERATED_RCFILES += ${GENERATED_RCFILES_${UNAME_S}}
