GENERATED_RCFILESIN_Linux = \
    tools/linux/systemd/stored.service.in

GENERATED_RCFILES_Linux = ${GENERATED_RCFILESIN_Linux:.in=}

GENERATED_RCFILES += ${GENERATED_RCFILES_${UNAME_S}}
