#ifndef STORED_NULLFD_H__
#define STORED_NULLFD_H__

/* This is used for services that needs a /dev/null. nullfd_init is called
 * before chroot and is inherited by the child processes of the service */

int nullfd_init();
void nullfd_cleanup();
int nullfd_get();

#endif
