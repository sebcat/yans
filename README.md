# Yet Another Network Scanner

This code base grew slowly over time as a side-project for exploring
various concepts, methods and algorithms such as process level
sandboxing, domain-specific languages, hash tables, netstring based IPC, &c

While most of the code is intended to be POSIX-compatible, some
FreeBSD-only functionality exists and the complete solution currently only
works on FreeBSD.

The main build artifact is an .ISO which can be run by most amd64
hypervisors and most(?) amd64 hardware. See tools/freebsd/vm/YANS for the
kernel configuration. The .ISO is mounted as the read-only root filesystem
at boot. /var and /tmp are mounted as memory-backed file systems. There's
a cron-job restarting the machine once a day. No data is saved by the
image between reboots. At least 1GB of RAM is recommended due to the md(4)
filesystems.

The virtual machine has a password-less user that anyone with console
access can use: scan-user. Otherwise all access should be done over
HTTP/HTTPS.

## Try it out

An instance of the image is running at
[disco.sajber.se](https://disco.sajber.se/). It's one $10 qemu-backed
instance so don't expect it to be available all the time.

yans-1.0.0.iso.xz is available [here](#TODO).
The released .ISO only listens with http on port 80.

## The code

### lib/net

- dnstres: threaded getaddrinfo(3) resolver
- ip,dsts,ports: code for handling ranges of IP addresses and ports
- fcgi,scgi: fcgi and scgi protocol implementations
- punycode: RFC3492 implementation of unknown conformance
- reaplan: file descriptor event-loop implementation
- tcpsrc: connect(2) over /dev/tcpsrc
- url: routines for normalizing URLs, incomplete (see url\_test.c)
- urlquery: parsing of URL query parameters

### lib/util

- buf: a simple buffer implementation
- csv: CSV reader, RFC4180 implementation of unknown conformance
- deduptbl: file-backed SHA1-based hash table for deduplication
- eds: lib for implementing event-driven, non-blocking local services
- flagset: code for mapping a string to a set of binary flags
- hexdump: mainly for tests and debugging output
- idset: binary flag implementation
- idtbl: map 32 bit integers (ids) to void *
- io: file descriptor utilities
- lines: line buffering abstraction
- nalphaver: numeric-alpha version comparison
- netstring: an implementation of djb netstrings
- nullfd: manages a file descriptor to /dev/null in static storage
- objtbl: table for indexing objects
- os: OS utility functions
- prng: mersenne twister with a 32-bit seed
- reorder: reorders a sequence of integers in a range using an LCG
- sandbox: very basic Capsicum or seccomp-bpf process-level sandbox
- sha1: wrapper for OpenSSL SHA1
- sindex: file-backed singe-reader/writer index
- str: string utilities
- sysinfo: get block, inode capacity of a disk and load average
- u8: UTF-8 to/from unicode code point conversion
- vaguever: version parser with vague semver-like abilities
- x509: wrapper around OpenSSL X.509 APIs
- ylog: logging utility for system services
- zfile: zlib wrapper for FILE\*

idtbl, deduptbl, objtbl are power of two hash tables with open
addressing, using robin-hood hashing and backwards shift deletion.

### ycl

ycl is a mechanism for generating code that builds netstring-formatted
messages used for non-blocking IPC over local sockets as well as data
serialization to the file system.

The code generation starts with data structures specified in .ycl files
of which there is only one: lib/ycl/ycl\_msg.ycl. From this file
lib/ycl/ycl\_msg.c is then generated using apps/yclgen. apps/yclgen
is a flex/bison tokenizer/parser.

### vulnspec

vulnspec is an S-expression based DSL with a simple byte code. The
code for parsing and evaluation of vulnspec expressions can be found in
lib/vulnspec. Examples of the vulnspec expressions themselves can be found
in data/vulnspec/components.vsi. e.g.,

```
(cve "CVE-2007-0626" 7.60 -1.00 "The comment_form_add_preview function in comment.module in Drupal before 4.7.6, and 5.x before 5.1, and vbDrupal, allows remote attackers with \"post comments\" privileges and access to multiple input filters to execute arbitrary code by previewing comments, which are not processed by \"normal form validation routines.\""
  (v
    (^
      (> "drupal/drupal" "4.0.0")
      (< "drupal/drupal" "4.7.6"))
    (^
      (>= "drupal/drupal" "5.0")
      (< "drupal/drupal" "5.1"))))

```

I feel that the problem space is suitable for a DSL. It's a basic
implementation. The execution time of CVE matching in vulnspec is
currently proportional to the number of components to test multiplied with
the number of CVEs in the data set.

misc/vulnspec contains POSIX shell and Python code for getting NVD CVE
listings and generating vulnspec statements from that data.

### a2, sc2

sc2 executes functions provided by a shared object file and dups a client
file descriptor to the standard I/O of the new process. It acts as a daemon
forking processes for CGI use.

The execution is done in a forked process of the parent with no subsequent
execve call. It trades security for speed by reusing its own address space.

a2 is an implementation of the HTTP API used by the Yans image. It expects
its I/O to follow the SCGI protocol. sc2 executes the functions in a2.

### deduptbl

apps/deduptbl is a demo utility for lib/util/deduptbl for potentially
file-backed deduplication of line-oriented data. It could e.g., be used
to remove previously seen URLs from a set of URLs.

```
$ deduptbl create -n 10 lel
$ printf 'aa\nbb\naa\n' | deduptbl lines -f lel
aa
bb
$ printf 'aa\nbb\naa\n' | deduptbl lines -f lel
$ printf 'cc\n' | deduptbl lines -f lel
cc
```

### expand-dst

apps/expand-dst is a demo utility for lib/net/dsts.

```
$ expand-dst 127.0.0.1-127.0.0.2 22-23
127.0.0.1 22
127.0.0.1 23
127.0.0.2 22
127.0.0.2 23
```

### kneg, knegd

knegd(1) is a daemon whose purpose is to queue and execute programs.
It interacts with stored(2) for allocation of stores. kneg(1) is the CLI
for the daemon. kneg(1) interacts with knegd(1) with ycl messages sent
over non-blocking UNIX domain sockets.

### store, stored

stored(1) is a daemon whose purpose is to provide a store of data for
sandboxed processes with no access to the file system. store(1) is the CLI
for the daemon. store(1) interacts with stored(1) with ycl messages sent
over non-blocking UNIX domain sockets.

### tcpctl, tcpsrc

tcpsrc(4) is a kernel module providing an interface for sandboxed processes
to create connect(2)-ed non-blocking TCP sockets. It is possible to set up
constraints based on destination addresses. tcpctl(4) is the
configuration endpoint. They live in devfs as /dev/tcpsrc, /dev/tcpctl.
tcpctl(1) is its CLI.

### scan, webfetch

scan(1) provides a way to grab banners and match regular expressions
against data. webfetch fetches URLs using libcurl. These are the scanner
applications. They execute in an application sandbox.

### lib/alloc

Contains allocators for specific allocation patterns. Not proven to improve
anything over malloc(3).

### lib/match

re2 wrapper and utility code for regular expression matching used in some
parts of the code.

