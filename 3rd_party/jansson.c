#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <sched.h>
#include <sys/time.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <stdint.h>
#include <locale.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "jansson.h"

#if defined(__FreeBSD__)
#include <sys/param.h>  /* attempt to define endianness */
#define RANDOMDEV "/dev/random"
#else
#include <endian.h>    /* attempt to define endianness */
#define RANDOMDEV "/dev/urandom"
#endif

/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef HASHTABLE_H
#define HASHTABLE_H

struct hashtable_list {
    struct hashtable_list *prev;
    struct hashtable_list *next;
};

/* "pair" may be a bit confusing a name, but think of it as a
   key-value pair. In this case, it just encodes some extra data,
   too */
struct hashtable_pair {
    struct hashtable_list list;
    struct hashtable_list ordered_list;
    size_t hash;
    json_t *value;
    char key[1];
};

struct hashtable_bucket {
    struct hashtable_list *first;
    struct hashtable_list *last;
};

typedef struct hashtable {
    size_t size;
    struct hashtable_bucket *buckets;
    size_t order;  /* hashtable has pow(2, order) buckets */
    struct hashtable_list list;
    struct hashtable_list ordered_list;
} hashtable_t;

#define hashtable_key_to_iter(key_) \
    (&(container_of(key_, struct hashtable_pair, key)->ordered_list))

/**
 * hashtable_init - Initialize a hashtable object
 *
 * @hashtable: The (statically allocated) hashtable object
 *
 * Initializes a statically allocated hashtable object. The object
 * should be cleared with hashtable_close when it's no longer used.
 *
 * Returns 0 on success, -1 on error (out of memory).
 */
static int hashtable_init(hashtable_t *hashtable);

/**
 * hashtable_close - Release all resources used by a hashtable object
 *
 * @hashtable: The hashtable
 *
 * Destroys a statically allocated hashtable object.
 */
static void hashtable_close(hashtable_t *hashtable);

/**
 * hashtable_set - Add/modify value in hashtable
 *
 * @hashtable: The hashtable object
 * @key: The key
 * @serial: For addition order of keys
 * @value: The value
 *
 * If a value with the given key already exists, its value is replaced
 * with the new value. Value is "stealed" in the sense that hashtable
 * doesn't increment its refcount but decreases the refcount when the
 * value is no longer needed.
 *
 * Returns 0 on success, -1 on failure (out of memory).
 */
static int hashtable_set(hashtable_t *hashtable, const char *key,
    json_t *value);

/**
 * hashtable_get - Get a value associated with a key
 *
 * @hashtable: The hashtable object
 * @key: The key
 *
 * Returns value if it is found, or NULL otherwise.
 */
static void *hashtable_get(hashtable_t *hashtable, const char *key);

/**
 * hashtable_del - Remove a value from the hashtable
 *
 * @hashtable: The hashtable object
 * @key: The key
 *
 * Returns 0 on success, or -1 if the key was not found.
 */
static int hashtable_del(hashtable_t *hashtable, const char *key);

/**
 * hashtable_clear - Clear hashtable
 *
 * @hashtable: The hashtable object
 *
 * Removes all items from the hashtable.
 */
static void hashtable_clear(hashtable_t *hashtable);

/**
 * hashtable_iter - Iterate over hashtable
 *
 * @hashtable: The hashtable object
 *
 * Returns an opaque iterator to the first element in the hashtable.
 * The iterator should be passed to hashtable_iter_* functions.
 * The hashtable items are not iterated over in any particular order.
 *
 * There's no need to free the iterator in any way. The iterator is
 * valid as long as the item that is referenced by the iterator is not
 * deleted. Other values may be added or deleted. In particular,
 * hashtable_iter_next() may be called on an iterator, and after that
 * the key/value pair pointed by the old iterator may be deleted.
 */
static void *hashtable_iter(hashtable_t *hashtable);

/**
 * hashtable_iter_at - Return an iterator at a specific key
 *
 * @hashtable: The hashtable object
 * @key: The key that the iterator should point to
 *
 * Like hashtable_iter() but returns an iterator pointing to a
 * specific key.
 */
static void *hashtable_iter_at(hashtable_t *hashtable, const char *key);

/**
 * hashtable_iter_next - Advance an iterator
 *
 * @hashtable: The hashtable object
 * @iter: The iterator
 *
 * Returns a new iterator pointing to the next element in the
 * hashtable or NULL if the whole hastable has been iterated over.
 */
static void *hashtable_iter_next(hashtable_t *hashtable, void *iter);

/**
 * hashtable_iter_key - Retrieve the key pointed by an iterator
 *
 * @iter: The iterator
 */
static void *hashtable_iter_key(void *iter);

/**
 * hashtable_iter_value - Retrieve the value pointed by an iterator
 *
 * @iter: The iterator
 */
static void *hashtable_iter_value(void *iter);

/**
 * hashtable_iter_set - Set the value pointed by an iterator
 *
 * @iter: The iterator
 * @value: The value to set
 */
static void hashtable_iter_set(void *iter, json_t *value);

#endif
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef STRBUFFER_H
#define STRBUFFER_H

typedef struct {
    char *value;
    size_t length;   /* bytes used */
    size_t size;     /* bytes allocated */
} strbuffer_t;

static int strbuffer_init(strbuffer_t *strbuff);
static void strbuffer_close(strbuffer_t *strbuff);

static void strbuffer_clear(strbuffer_t *strbuff);

static const char *strbuffer_value(const strbuffer_t *strbuff);

/* Steal the value and close the strbuffer */
static char *strbuffer_steal_value(strbuffer_t *strbuff);

static int strbuffer_append_byte(strbuffer_t *strbuff, char byte);
static int strbuffer_append_bytes(strbuffer_t *strbuff, const char *data,
    size_t size);

static char strbuffer_pop(strbuffer_t *strbuff);

#endif
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef JANSSON_PRIVATE_H
#define JANSSON_PRIVATE_H

#define container_of(ptr_, type_, member_)  \
    ((type_ *)((char *)ptr_ - offsetof(type_, member_)))

/* On some platforms, max() may already be defined */
#ifndef max
#define max(a, b)  ((a) > (b) ? (a) : (b))
#endif

/* va_copy is a C99 feature. In C89 implementations, it's sometimes
   available as __va_copy. If not, memcpy() should do the trick. */
#ifndef va_copy
#ifdef __va_copy
#define va_copy __va_copy
#else
#define va_copy(a, b)  memcpy(&(a), &(b), sizeof(va_list))
#endif
#endif

typedef struct {
    json_t json;
    hashtable_t hashtable;
    int visited;
} json_object_t;

typedef struct {
    json_t json;
    size_t size;
    size_t entries;
    json_t **table;
    int visited;
} json_array_t;

typedef struct {
    json_t json;
    char *value;
    size_t length;
} json_string_t;

typedef struct {
    json_t json;
    double value;
} json_real_t;

typedef struct {
    json_t json;
    json_int_t value;
} json_integer_t;

#define json_to_object(json_)  container_of(json_, json_object_t, json)
#define json_to_array(json_)   container_of(json_, json_array_t, json)
#define json_to_string(json_)  container_of(json_, json_string_t, json)
#define json_to_real(json_)    container_of(json_, json_real_t, json)
#define json_to_integer(json_) container_of(json_, json_integer_t, json)

/* Create a string by taking ownership of an existing buffer */
static json_t *jsonp_stringn_nocheck_own(const char *value, size_t len);

/* Error message formatting */
static void jsonp_error_init(json_error_t *error, const char *source);
static void jsonp_error_set_source(json_error_t *error, const char *source);
static void jsonp_error_set(json_error_t *error, int line, int column,
                     size_t position, const char *msg, ...);
static void jsonp_error_vset(json_error_t *error, int line, int column,
                      size_t position, const char *msg, va_list ap);

/* Locale independent string<->double conversions */
static int jsonp_strtod(strbuffer_t *strbuffer, double *out);
static int jsonp_dtostr(char *buffer, size_t size, double value, int prec);

/* Wrappers for custom memory functions */
static void* jsonp_malloc(size_t size);
static void jsonp_free(void *ptr);
static char *jsonp_strndup(const char *str, size_t length);
static char *jsonp_strdup(const char *str);
static char *jsonp_strndup(const char *str, size_t len);

#endif
/*
-------------------------------------------------------------------------------
lookup3.c, by Bob Jenkins, May 2006, Public Domain.

These are functions for producing 32-bit hashes for hash table lookup.
hashword(), hashlittle(), hashlittle2(), hashbig(), mix(), and final()
are externally useful functions.  Routines to test the hash are included
if SELF_TEST is defined.  You can use this free for any purpose.  It's in
the public domain.  It has no warranty.

You probably want to use hashlittle().  hashlittle() and hashbig()
hash byte arrays.  hashlittle() is is faster than hashbig() on
little-endian machines.  Intel and AMD are little-endian machines.
On second thought, you probably want hashlittle2(), which is identical to
hashlittle() except it returns two 32-bit hashes for the price of one.
You could implement hashbig2() if you wanted but I haven't bothered here.

If you want to find a hash of, say, exactly 7 integers, do
  a = i1;  b = i2;  c = i3;
  mix(a,b,c);
  a += i4; b += i5; c += i6;
  mix(a,b,c);
  a += i7;
  final(a,b,c);
then use c as the hash value.  If you have a variable length array of
4-byte integers to hash, use hashword().  If you have a byte array (like
a character string), use hashlittle().  If you have several byte arrays, or
a mix of things, see the comments above hashlittle().

Why is this so big?  I read 12 bytes at a time into 3 4-byte integers,
then mix those integers.  This is fast (you can do a lot more thorough
mixing with 12*3 instructions on 3 integers than you can with 3 instructions
on 1 byte), but shoehorning those bytes into integers efficiently is messy.
-------------------------------------------------------------------------------
*/

#include <stdlib.h>

/*
 * My best guess at if you are big-endian or little-endian.  This may
 * need adjustment.
 */
#if (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && \
     __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && \
       __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 0
#endif

#define hashsize(n) ((uint32_t)1<<(n))
#define hashmask(n) (hashsize(n)-1)
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

/*
-------------------------------------------------------------------------------
mix -- mix 3 32-bit values reversibly.

This is reversible, so any information in (a,b,c) before mix() is
still in (a,b,c) after mix().

If four pairs of (a,b,c) inputs are run through mix(), or through
mix() in reverse, there are at least 32 bits of the output that
are sometimes the same for one pair and different for another pair.
This was tested for:
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

Some k values for my "a-=c; a^=rot(c,k); c+=b;" arrangement that
satisfy this are
    4  6  8 16 19  4
    9 15  3 18 27 15
   14  9  3  7 17  3
Well, "9 15 3 18 27 15" didn't quite get 32 bits diffing
for "differ" defined as + with a one-bit base and a two-bit delta.  I
used http://burtleburtle.net/bob/hash/avalanche.html to choose 
the operations, constants, and arrangements of the variables.

This does not achieve avalanche.  There are input bits of (a,b,c)
that fail to affect some output bits of (a,b,c), especially of a.  The
most thoroughly mixed value is c, but it doesn't really even achieve
avalanche in c.

This allows some parallelism.  Read-after-writes are good at doubling
the number of bits affected, so the goal of mixing pulls in the opposite
direction as the goal of parallelism.  I did what I could.  Rotates
seem to cost as much as shifts on every machine I could lay my hands
on, and rotates are much kinder to the top and bottom bits, so I used
rotates.
-------------------------------------------------------------------------------
*/
#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

/*
-------------------------------------------------------------------------------
final -- final mixing of 3 32-bit values (a,b,c) into c

Pairs of (a,b,c) values differing in only a few bits will usually
produce values of c that look totally different.  This was tested for
* pairs that differed by one bit, by two bits, in any combination
  of top bits of (a,b,c), or in any combination of bottom bits of
  (a,b,c).
* "differ" is defined as +, -, ^, or ~^.  For + and -, I transformed
  the output delta to a Gray code (a^(a>>1)) so a string of 1's (as
  is commonly produced by subtraction) look like a single 1-bit
  difference.
* the base values were pseudorandom, all zero but one bit set, or 
  all zero plus a counter that starts at zero.

These constants passed:
 14 11 25 16 4 14 24
 12 14 25 16 4 14 24
and these came close:
  4  8 15 26 3 22 24
 10  8 15 26 3 22 24
 11  8 15 26 3 22 24
-------------------------------------------------------------------------------
*/
#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

/*
-------------------------------------------------------------------------------
hashlittle() -- hash a variable-length key into a 32-bit value
  k       : the key (the unaligned variable-length array of bytes)
  length  : the length of the key, counting by bytes
  initval : can be any 4-byte value
Returns a 32-bit value.  Every bit of the key affects every bit of
the return value.  Two keys differing by one or two bits will have
totally different hash values.

The best hash table sizes are powers of 2.  There is no need to do
mod a prime (mod is sooo slow!).  If you need less than 32 bits,
use a bitmask.  For example, if you need only 10 bits, do
  h = (h & hashmask(10));
In which case, the hash table should have hashsize(10) elements.

If you are hashing n strings (uint8_t **)k, do it like this:
  for (i=0, h=0; i<n; ++i) h = hashlittle( k[i], len[i], h);

By Bob Jenkins, 2006.  bob_jenkins@burtleburtle.net.  You may use this
code any way you wish, private, educational, or commercial.  It's free.

Use for hash table lookup, or anything where one collision in 2^^32 is
acceptable.  Do NOT use for cryptographic purposes.
-------------------------------------------------------------------------------
*/

static uint32_t hashlittle(const void *key, size_t length, uint32_t initval)
{
  uint32_t a,b,c;                                          /* internal state */
  union { const void *ptr; size_t i; } u;     /* needed for Mac Powerbook G4 */

  /* Set up the internal state */
  a = b = c = 0xdeadbeef + ((uint32_t)length) + initval;

  u.ptr = key;
  if (HASH_LITTLE_ENDIAN && ((u.i & 0x3) == 0)) {
    const uint32_t *k = (const uint32_t *)key;         /* read 32-bit chunks */

/* Detect Valgrind or AddressSanitizer */
#ifdef VALGRIND
# define NO_MASKING_TRICK 1
#else
# if defined(__has_feature)  /* Clang */
#  if __has_feature(address_sanitizer)  /* is ASAN enabled? */
#   define NO_MASKING_TRICK 1
#  endif
# else
#  if defined(__SANITIZE_ADDRESS__)  /* GCC 4.8.x, is ASAN enabled? */
#   define NO_MASKING_TRICK 1
#  endif
# endif
#endif

#ifdef NO_MASKING_TRICK
    const uint8_t  *k8;
#endif

    /*------ all but last block: aligned reads and affect 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      b += k[1];
      c += k[2];
      mix(a,b,c);
      length -= 12;
      k += 3;
    }

    /*----------------------------- handle the last (probably partial) block */
    /*
     * "k[2]&0xffffff" actually reads beyond the end of the string, but
     * then masks off the part it's not allowed to read.  Because the
     * string is aligned, the masked-off tail is in the same word as the
     * rest of the string.  Every machine with memory protection I've seen
     * does it on word boundaries, so is OK with this.  But VALGRIND will
     * still catch it and complain.  The masking trick does make the hash
     * noticably faster for short strings (like English words).
     */
#ifndef NO_MASKING_TRICK

    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=k[2]&0xffffff; b+=k[1]; a+=k[0]; break;
    case 10: c+=k[2]&0xffff; b+=k[1]; a+=k[0]; break;
    case 9 : c+=k[2]&0xff; b+=k[1]; a+=k[0]; break;
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=k[1]&0xffffff; a+=k[0]; break;
    case 6 : b+=k[1]&0xffff; a+=k[0]; break;
    case 5 : b+=k[1]&0xff; a+=k[0]; break;
    case 4 : a+=k[0]; break;
    case 3 : a+=k[0]&0xffffff; break;
    case 2 : a+=k[0]&0xffff; break;
    case 1 : a+=k[0]&0xff; break;
    case 0 : return c;              /* zero length strings require no mixing */
    }

#else /* make valgrind happy */

    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[2]; b+=k[1]; a+=k[0]; break;
    case 11: c+=((uint32_t)k8[10])<<16;  /* fall through */
    case 10: c+=((uint32_t)k8[9])<<8;    /* fall through */
    case 9 : c+=k8[8];                   /* fall through */
    case 8 : b+=k[1]; a+=k[0]; break;
    case 7 : b+=((uint32_t)k8[6])<<16;   /* fall through */
    case 6 : b+=((uint32_t)k8[5])<<8;    /* fall through */
    case 5 : b+=k8[4];                   /* fall through */
    case 4 : a+=k[0]; break;
    case 3 : a+=((uint32_t)k8[2])<<16;   /* fall through */
    case 2 : a+=((uint32_t)k8[1])<<8;    /* fall through */
    case 1 : a+=k8[0]; break;
    case 0 : return c;
    }

#endif /* !valgrind */

  } else if (HASH_LITTLE_ENDIAN && ((u.i & 0x1) == 0)) {
    const uint16_t *k = (const uint16_t *)key;         /* read 16-bit chunks */
    const uint8_t  *k8;

    /*--------------- all but last block: aligned reads and different mixing */
    while (length > 12)
    {
      a += k[0] + (((uint32_t)k[1])<<16);
      b += k[2] + (((uint32_t)k[3])<<16);
      c += k[4] + (((uint32_t)k[5])<<16);
      mix(a,b,c);
      length -= 12;
      k += 6;
    }

    /*----------------------------- handle the last (probably partial) block */
    k8 = (const uint8_t *)k;
    switch(length)
    {
    case 12: c+=k[4]+(((uint32_t)k[5])<<16);
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 11: c+=((uint32_t)k8[10])<<16;     /* fall through */
    case 10: c+=k[4];
             b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 9 : c+=k8[8];                      /* fall through */
    case 8 : b+=k[2]+(((uint32_t)k[3])<<16);
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 7 : b+=((uint32_t)k8[6])<<16;      /* fall through */
    case 6 : b+=k[2];
             a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 5 : b+=k8[4];                      /* fall through */
    case 4 : a+=k[0]+(((uint32_t)k[1])<<16);
             break;
    case 3 : a+=((uint32_t)k8[2])<<16;      /* fall through */
    case 2 : a+=k[0];
             break;
    case 1 : a+=k8[0];
             break;
    case 0 : return c;                     /* zero length requires no mixing */
    }

  } else {                        /* need to read the key one byte at a time */
    const uint8_t *k = (const uint8_t *)key;

    /*--------------- all but the last block: affect some 32 bits of (a,b,c) */
    while (length > 12)
    {
      a += k[0];
      a += ((uint32_t)k[1])<<8;
      a += ((uint32_t)k[2])<<16;
      a += ((uint32_t)k[3])<<24;
      b += k[4];
      b += ((uint32_t)k[5])<<8;
      b += ((uint32_t)k[6])<<16;
      b += ((uint32_t)k[7])<<24;
      c += k[8];
      c += ((uint32_t)k[9])<<8;
      c += ((uint32_t)k[10])<<16;
      c += ((uint32_t)k[11])<<24;
      mix(a,b,c);
      length -= 12;
      k += 12;
    }

    /*-------------------------------- last block: affect all 32 bits of (c) */
    switch(length)                   /* all the case statements fall through */
    {
    case 12: c+=((uint32_t)k[11])<<24;
    case 11: c+=((uint32_t)k[10])<<16;
    case 10: c+=((uint32_t)k[9])<<8;
    case 9 : c+=k[8];
    case 8 : b+=((uint32_t)k[7])<<24;
    case 7 : b+=((uint32_t)k[6])<<16;
    case 6 : b+=((uint32_t)k[5])<<8;
    case 5 : b+=k[4];
    case 4 : a+=((uint32_t)k[3])<<24;
    case 3 : a+=((uint32_t)k[2])<<16;
    case 2 : a+=((uint32_t)k[1])<<8;
    case 1 : a+=k[0];
             break;
    case 0 : return c;
    }
  }

  final(a,b,c);
  return c;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

static int utf8_encode(int32_t codepoint, char *buffer, size_t *size)
{
    if(codepoint < 0)
        return -1;
    else if(codepoint < 0x80)
    {
        buffer[0] = (char)codepoint;
        *size = 1;
    }
    else if(codepoint < 0x800)
    {
        buffer[0] = 0xC0 + ((codepoint & 0x7C0) >> 6);
        buffer[1] = 0x80 + ((codepoint & 0x03F));
        *size = 2;
    }
    else if(codepoint < 0x10000)
    {
        buffer[0] = 0xE0 + ((codepoint & 0xF000) >> 12);
        buffer[1] = 0x80 + ((codepoint & 0x0FC0) >> 6);
        buffer[2] = 0x80 + ((codepoint & 0x003F));
        *size = 3;
    }
    else if(codepoint <= 0x10FFFF)
    {
        buffer[0] = 0xF0 + ((codepoint & 0x1C0000) >> 18);
        buffer[1] = 0x80 + ((codepoint & 0x03F000) >> 12);
        buffer[2] = 0x80 + ((codepoint & 0x000FC0) >> 6);
        buffer[3] = 0x80 + ((codepoint & 0x00003F));
        *size = 4;
    }
    else
        return -1;

    return 0;
}

static size_t utf8_check_first(char byte)
{
    unsigned char u = (unsigned char)byte;

    if(u < 0x80)
        return 1;

    if(0x80 <= u && u <= 0xBF) {
        /* second, third or fourth byte of a multi-byte
           sequence, i.e. a "continuation byte" */
        return 0;
    }
    else if(u == 0xC0 || u == 0xC1) {
        /* overlong encoding of an ASCII byte */
        return 0;
    }
    else if(0xC2 <= u && u <= 0xDF) {
        /* 2-byte sequence */
        return 2;
    }

    else if(0xE0 <= u && u <= 0xEF) {
        /* 3-byte sequence */
        return 3;
    }
    else if(0xF0 <= u && u <= 0xF4) {
        /* 4-byte sequence */
        return 4;
    }
    else { /* u >= 0xF5 */
        /* Restricted (start of 4-, 5- or 6-byte sequence) or invalid
           UTF-8 */
        return 0;
    }
}

static size_t utf8_check_full(const char *buffer, size_t size,
    int32_t *codepoint)
{
    size_t i;
    int32_t value = 0;
    unsigned char u = (unsigned char)buffer[0];

    if(size == 2)
    {
        value = u & 0x1F;
    }
    else if(size == 3)
    {
        value = u & 0xF;
    }
    else if(size == 4)
    {
        value = u & 0x7;
    }
    else
        return 0;

    for(i = 1; i < size; i++)
    {
        u = (unsigned char)buffer[i];

        if(u < 0x80 || u > 0xBF) {
            /* not a continuation byte */
            return 0;
        }

        value = (value << 6) + (u & 0x3F);
    }

    if(value > 0x10FFFF) {
        /* not in Unicode range */
        return 0;
    }

    else if(0xD800 <= value && value <= 0xDFFF) {
        /* invalid code point (UTF-16 surrogate halves) */
        return 0;
    }

    else if((size == 2 && value < 0x80) ||
            (size == 3 && value < 0x800) ||
            (size == 4 && value < 0x10000)) {
        /* overlong encoding */
        return 0;
    }

    if(codepoint)
        *codepoint = value;

    return 1;
}

static const char *utf8_iterate(const char *buffer, size_t bufsize,
    int32_t *codepoint)
{
    size_t count;
    int32_t value;

    if(!bufsize)
        return buffer;

    count = utf8_check_first(buffer[0]);
    if(count <= 0)
        return NULL;

    if(count == 1)
        value = (unsigned char)buffer[0];
    else
    {
        if(count > bufsize || !utf8_check_full(buffer, count, &value))
            return NULL;
    }

    if(codepoint)
        *codepoint = value;

    return buffer + count;
}

static int utf8_check_string(const char *string, size_t length)
{
    size_t i;

    for(i = 0; i < length; i++)
    {
        size_t count = utf8_check_first(string[i]);
        if(count == 0)
            return 0;
        else if(count > 1)
        {
            if(count > length - i)
                return 0;

            if(!utf8_check_full(&string[i], count, NULL))
                return 0;

            i += count - 1;
        }
    }

    return 1;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#define MAX_INTEGER_STR_LENGTH  100
#define MAX_REAL_STR_LENGTH     100

#define FLAGS_TO_INDENT(f)      ((f) & 0x1F)
#define FLAGS_TO_PRECISION(f)   (((f) >> 11) & 0x1F)

struct buffer {
    const size_t size;
    size_t used;
    char *data;
};

static int dump_to_strbuffer(const char *buffer, size_t size, void *data)
{
    return strbuffer_append_bytes((strbuffer_t *)data, buffer, size);
}

static int dump_to_buffer(const char *buffer, size_t size, void *data)
{
    struct buffer *buf = (struct buffer *)data;

    if(buf->used + size <= buf->size)
        memcpy(&buf->data[buf->used], buffer, size);

    buf->used += size;
    return 0;
}

static int dump_to_file(const char *buffer, size_t size, void *data)
{
    FILE *dest = (FILE *)data;
    if(fwrite(buffer, size, 1, dest) != 1)
        return -1;
    return 0;
}

static int dump_to_fd(const char *buffer, size_t size, void *data)
{
    int *dest = (int *)data;
    if(write(*dest, buffer, size) == (ssize_t)size)
        return 0;
    return -1;
}

/* 32 spaces (the maximum indentation size) */
static const char whitespace[] = "                                ";

static int dump_indent(size_t flags, int depth, int space,
    json_dump_callback_t dump, void *data)
{
    if(FLAGS_TO_INDENT(flags) > 0)
    {
        unsigned int ws_count = FLAGS_TO_INDENT(flags);
        unsigned int n_spaces = depth * ws_count;

        if(dump("\n", 1, data))
            return -1;

        while(n_spaces > 0)
        {
            int cur_n;
            if (n_spaces < sizeof(whitespace) - 1) {
              cur_n = n_spaces;
            } else {
              cur_n = sizeof(whitespace) - 1;
            }

            if(dump(whitespace, cur_n, data))
                return -1;

            n_spaces -= cur_n;
        }
    }
    else if(space && !(flags & JSON_COMPACT))
    {
        return dump(" ", 1, data);
    }
    return 0;
}

static int dump_string(const char *str, size_t len, json_dump_callback_t dump,
    void *data, size_t flags)
{
    const char *pos, *end, *lim;
    int32_t codepoint = 0;

    if(dump("\"", 1, data))
        return -1;

    end = pos = str;
    lim = str + len;
    while(1)
    {
        const char *text;
        char seq[13];
        int length;

        while(end < lim)
        {
            end = utf8_iterate(pos, lim - pos, &codepoint);
            if(!end)
                return -1;

            /* mandatory escape or control char */
            if(codepoint == '\\' || codepoint == '"' || codepoint < 0x20)
                break;

            /* slash */
            if((flags & JSON_ESCAPE_SLASH) && codepoint == '/')
                break;

            /* non-ASCII */
            if((flags & JSON_ENSURE_ASCII) && codepoint > 0x7F)
                break;

            pos = end;
        }

        if(pos != str) {
            if(dump(str, pos - str, data))
                return -1;
        }

        if(end == pos)
            break;

        /* handle \, /, ", and control codes */
        length = 2;
        switch(codepoint)
        {
            case '\\': text = "\\\\"; break;
            case '\"': text = "\\\""; break;
            case '\b': text = "\\b"; break;
            case '\f': text = "\\f"; break;
            case '\n': text = "\\n"; break;
            case '\r': text = "\\r"; break;
            case '\t': text = "\\t"; break;
            case '/':  text = "\\/"; break;
            default:
            {
                /* codepoint is in BMP */
                if(codepoint < 0x10000)
                {
                    snprintf(seq, sizeof(seq), "\\u%04X",
                        (unsigned int)codepoint);
                    length = 6;
                }

                /* not in BMP -> construct a UTF-16 surrogate pair */
                else
                {
                    int32_t first, last;

                    codepoint -= 0x10000;
                    first = 0xD800 | ((codepoint & 0xffc00) >> 10);
                    last = 0xDC00 | (codepoint & 0x003ff);

                    snprintf(seq, sizeof(seq), "\\u%04X\\u%04X",
                        (unsigned int)first, (unsigned int)last);
                    length = 12;
                }

                text = seq;
                break;
            }
        }

        if(dump(text, length, data))
            return -1;

        str = pos = end;
    }

    return dump("\"", 1, data);
}

static int compare_keys(const void *key1, const void *key2)
{
    return strcmp(*(const char **)key1, *(const char **)key2);
}

static int do_dump(const json_t *json, size_t flags, int depth,
                   json_dump_callback_t dump, void *data)
{
    int embed = flags & JSON_EMBED;

    flags &= ~JSON_EMBED;

    if(!json)
        return -1;

    switch(json_typeof(json)) {
        case JSON_NULL:
            return dump("null", 4, data);

        case JSON_TRUE:
            return dump("true", 4, data);

        case JSON_FALSE:
            return dump("false", 5, data);

        case JSON_INTEGER:
        {
            char buffer[MAX_INTEGER_STR_LENGTH];
            int size;

            size = snprintf(buffer, MAX_INTEGER_STR_LENGTH,
                            "%" JSON_INTEGER_FORMAT,
                            json_integer_value(json));
            if(size < 0 || size >= MAX_INTEGER_STR_LENGTH)
                return -1;

            return dump(buffer, size, data);
        }

        case JSON_REAL:
        {
            char buffer[MAX_REAL_STR_LENGTH];
            int size;
            double value = json_real_value(json);

            size = jsonp_dtostr(buffer, MAX_REAL_STR_LENGTH, value,
                                FLAGS_TO_PRECISION(flags));
            if(size < 0)
                return -1;

            return dump(buffer, size, data);
        }

        case JSON_STRING:
            return dump_string(json_string_value(json),
                json_string_length(json), dump, data, flags);

        case JSON_ARRAY:
        {
            size_t n;
            size_t i;

            json_array_t *array;

            /* detect circular references */
            array = json_to_array(json);
            if(array->visited)
                goto array_error;
            array->visited = 1;

            n = json_array_size(json);

            if(!embed && dump("[", 1, data))
                goto array_error;
            if(n == 0) {
                array->visited = 0;
                return embed ? 0 : dump("]", 1, data);
            }
            if(dump_indent(flags, depth + 1, 0, dump, data))
                goto array_error;

            for(i = 0; i < n; ++i) {
                if(do_dump(json_array_get(json, i), flags, depth + 1,
                           dump, data))
                    goto array_error;

                if(i < n - 1)
                {
                    if(dump(",", 1, data) ||
                       dump_indent(flags, depth + 1, 1, dump, data))
                        goto array_error;
                }
                else
                {
                    if(dump_indent(flags, depth, 0, dump, data))
                        goto array_error;
                }
            }

            array->visited = 0;
            return embed ? 0 : dump("]", 1, data);

        array_error:
            array->visited = 0;
            return -1;
        }

        case JSON_OBJECT:
        {
            json_object_t *object;
            void *iter;
            const char *separator;
            int separator_length;

            if(flags & JSON_COMPACT) {
                separator = ":";
                separator_length = 1;
            }
            else {
                separator = ": ";
                separator_length = 2;
            }

            /* detect circular references */
            object = json_to_object(json);
            if(object->visited)
                goto object_error;
            object->visited = 1;

            iter = json_object_iter((json_t *)json);

            if(!embed && dump("{", 1, data))
                goto object_error;
            if(!iter) {
                object->visited = 0;
                return embed ? 0 : dump("}", 1, data);
            }
            if(dump_indent(flags, depth + 1, 0, dump, data))
                goto object_error;

            if(flags & JSON_SORT_KEYS)
            {
                const char **keys;
                size_t size, i;

                size = json_object_size(json);
                keys = jsonp_malloc(size * sizeof(const char *));
                if(!keys)
                    goto object_error;

                i = 0;
                while(iter)
                {
                    keys[i] = json_object_iter_key(iter);
                    iter = json_object_iter_next((json_t *)json, iter);
                    i++;
                }
                assert(i == size);

                qsort(keys, size, sizeof(const char *), compare_keys);

                for(i = 0; i < size; i++)
                {
                    const char *key;
                    json_t *value;

                    key = keys[i];
                    value = json_object_get(json, key);
                    assert(value);

                    dump_string(key, strlen(key), dump, data, flags);
                    if(dump(separator, separator_length, data) ||
                       do_dump(value, flags, depth + 1, dump, data))
                    {
                        jsonp_free(keys);
                        goto object_error;
                    }

                    if(i < size - 1)
                    {
                        if(dump(",", 1, data) ||
                           dump_indent(flags, depth + 1, 1, dump, data))
                        {
                            jsonp_free(keys);
                            goto object_error;
                        }
                    }
                    else
                    {
                        if(dump_indent(flags, depth, 0, dump, data))
                        {
                            jsonp_free(keys);
                            goto object_error;
                        }
                    }
                }

                jsonp_free(keys);
            }
            else
            {
                /* Don't sort keys */

                while(iter)
                {
                    void *next = json_object_iter_next((json_t *)json, iter);
                    const char *key = json_object_iter_key(iter);

                    dump_string(key, strlen(key), dump, data, flags);
                    if(dump(separator, separator_length, data) ||
                       do_dump(json_object_iter_value(iter), flags, depth + 1,
                               dump, data))
                        goto object_error;

                    if(next)
                    {
                        if(dump(",", 1, data) ||
                           dump_indent(flags, depth + 1, 1, dump, data))
                            goto object_error;
                    }
                    else
                    {
                        if(dump_indent(flags, depth, 0, dump, data))
                            goto object_error;
                    }

                    iter = next;
                }
            }

            object->visited = 0;
            return embed ? 0 : dump("}", 1, data);

        object_error:
            object->visited = 0;
            return -1;
        }

        default:
            /* not reached */
            return -1;
    }
}

char *json_dumps(const json_t *json, size_t flags)
{
    strbuffer_t strbuff;
    char *result;

    if(strbuffer_init(&strbuff))
        return NULL;

    if(json_dump_callback(json, dump_to_strbuffer, (void *)&strbuff, flags))
        result = NULL;
    else
        result = jsonp_strdup(strbuffer_value(&strbuff));

    strbuffer_close(&strbuff);
    return result;
}

size_t json_dumpb(const json_t *json, char *buffer, size_t size, size_t flags)
{
    struct buffer buf = { size, 0, buffer };

    if(json_dump_callback(json, dump_to_buffer, (void *)&buf, flags))
        return 0;

    return buf.used;
}

int json_dumpf(const json_t *json, FILE *output, size_t flags)
{
    return json_dump_callback(json, dump_to_file, (void *)output, flags);
}

int json_dumpfd(const json_t *json, int output, size_t flags)
{
    return json_dump_callback(json, dump_to_fd, (void *)&output, flags);
}

int json_dump_file(const json_t *json, const char *path, size_t flags)
{
    int result;

    FILE *output = fopen(path, "w");
    if(!output)
        return -1;

    result = json_dumpf(json, output, flags);

    fclose(output);
    return result;
}

int json_dump_callback(const json_t *json, json_dump_callback_t callback,
    void *data, size_t flags)
{
    if(!(flags & JSON_ENCODE_ANY)) {
        if(!json_is_array(json) && !json_is_object(json))
           return -1;
    }

    return do_dump(json, flags, 0, callback, data);
}

void jsonp_error_init(json_error_t *error, const char *source)
{
    if(error)
    {
        error->text[0] = '\0';
        error->line = -1;
        error->column = -1;
        error->position = 0;
        if(source)
            jsonp_error_set_source(error, source);
        else
            error->source[0] = '\0';
    }
}

void jsonp_error_set_source(json_error_t *error, const char *source)
{
    size_t length;

    if(!error || !source)
        return;

    length = strlen(source);
    if(length < JSON_ERROR_SOURCE_LENGTH)
        strncpy(error->source, source, length + 1);
    else {
        size_t extra = length - JSON_ERROR_SOURCE_LENGTH + 4;
        strncpy(error->source, "...", 4);
        strncpy(error->source + 3, source + extra, length - extra + 1);
    }
}

void jsonp_error_set(json_error_t *error, int line, int column,
                     size_t position, const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    jsonp_error_vset(error, line, column, position, msg, ap);
    va_end(ap);
}

void jsonp_error_vset(json_error_t *error, int line, int column,
                      size_t position, const char *msg, va_list ap)
{
    if(!error)
        return;

    if(error->text[0] != '\0') {
        /* error already set */
        return;
    }

    error->line = line;
    error->column = column;
    error->position = (int)position;

    vsnprintf(error->text, JSON_ERROR_TEXT_LENGTH, msg, ap);
    error->text[JSON_ERROR_TEXT_LENGTH - 1] = '\0';
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#ifndef INITIAL_HASHTABLE_ORDER
#define INITIAL_HASHTABLE_ORDER 3
#endif

typedef struct hashtable_list list_t;
typedef struct hashtable_pair pair_t;
typedef struct hashtable_bucket bucket_t;

/* Implementation of the hash function */

volatile uint32_t hashtable_seed = 0;

#define list_to_pair(list_)  container_of(list_, pair_t, list)
#define ordered_list_to_pair(list_)  container_of(list_, pair_t, ordered_list)
#define hash_str(key) ((size_t)hashlittle((key), strlen(key), hashtable_seed))

static inline void list_init(list_t *list)
{
    list->next = list;
    list->prev = list;
}

static inline void list_insert(list_t *list, list_t *node)
{
    node->next = list;
    node->prev = list->prev;
    list->prev->next = node;
    list->prev = node;
}

static inline void list_remove(list_t *list)
{
    list->prev->next = list->next;
    list->next->prev = list->prev;
}

static inline int bucket_is_empty(hashtable_t *hashtable, bucket_t *bucket)
{
    return bucket->first == &hashtable->list && bucket->first == bucket->last;
}

static void insert_to_bucket(hashtable_t *hashtable, bucket_t *bucket,
                             list_t *list)
{
    if(bucket_is_empty(hashtable, bucket))
    {
        list_insert(&hashtable->list, list);
        bucket->first = bucket->last = list;
    }
    else
    {
        list_insert(bucket->first, list);
        bucket->first = list;
    }
}

static pair_t *hashtable_find_pair(hashtable_t *hashtable, bucket_t *bucket,
                                   const char *key, size_t hash)
{
    list_t *list;
    pair_t *pair;

    if(bucket_is_empty(hashtable, bucket))
        return NULL;

    list = bucket->first;
    while(1)
    {
        pair = list_to_pair(list);
        if(pair->hash == hash && strcmp(pair->key, key) == 0)
            return pair;

        if(list == bucket->last)
            break;

        list = list->next;
    }

    return NULL;
}

/* returns 0 on success, -1 if key was not found */
static int hashtable_do_del(hashtable_t *hashtable,
                            const char *key, size_t hash)
{
    pair_t *pair;
    bucket_t *bucket;
    size_t index;

    index = hash & hashmask(hashtable->order);
    bucket = &hashtable->buckets[index];

    pair = hashtable_find_pair(hashtable, bucket, key, hash);
    if(!pair)
        return -1;

    if(&pair->list == bucket->first && &pair->list == bucket->last)
        bucket->first = bucket->last = &hashtable->list;

    else if(&pair->list == bucket->first)
        bucket->first = pair->list.next;

    else if(&pair->list == bucket->last)
        bucket->last = pair->list.prev;

    list_remove(&pair->list);
    list_remove(&pair->ordered_list);
    json_decref(pair->value);

    jsonp_free(pair);
    hashtable->size--;

    return 0;
}

static void hashtable_do_clear(hashtable_t *hashtable)
{
    list_t *list, *next;
    pair_t *pair;

    for(list = hashtable->list.next; list != &hashtable->list; list = next)
    {
        next = list->next;
        pair = list_to_pair(list);
        json_decref(pair->value);
        jsonp_free(pair);
    }
}

static int hashtable_do_rehash(hashtable_t *hashtable)
{
    list_t *list, *next;
    pair_t *pair;
    size_t i, index, new_size, new_order;
    struct hashtable_bucket *new_buckets;

    new_order = hashtable->order + 1;
    new_size = hashsize(new_order);

    new_buckets = jsonp_malloc(new_size * sizeof(bucket_t));
    if(!new_buckets)
        return -1;

    jsonp_free(hashtable->buckets);
    hashtable->buckets = new_buckets;
    hashtable->order = new_order;

    for(i = 0; i < hashsize(hashtable->order); i++)
    {
        hashtable->buckets[i].first = hashtable->buckets[i].last =
            &hashtable->list;
    }

    list = hashtable->list.next;
    list_init(&hashtable->list);

    for(; list != &hashtable->list; list = next) {
        next = list->next;
        pair = list_to_pair(list);
        index = pair->hash % new_size;
        insert_to_bucket(hashtable, &hashtable->buckets[index], &pair->list);
    }

    return 0;
}

int hashtable_init(hashtable_t *hashtable)
{
    size_t i;

    hashtable->size = 0;
    hashtable->order = INITIAL_HASHTABLE_ORDER;
    hashtable->buckets = jsonp_malloc(hashsize(hashtable->order) * sizeof(bucket_t));
    if(!hashtable->buckets)
        return -1;

    list_init(&hashtable->list);
    list_init(&hashtable->ordered_list);

    for(i = 0; i < hashsize(hashtable->order); i++)
    {
        hashtable->buckets[i].first = hashtable->buckets[i].last =
            &hashtable->list;
    }

    return 0;
}

void hashtable_close(hashtable_t *hashtable)
{
    hashtable_do_clear(hashtable);
    jsonp_free(hashtable->buckets);
}

int hashtable_set(hashtable_t *hashtable, const char *key, json_t *value)
{
    pair_t *pair;
    bucket_t *bucket;
    size_t hash, index;

    /* rehash if the load ratio exceeds 1 */
    if(hashtable->size >= hashsize(hashtable->order))
        if(hashtable_do_rehash(hashtable))
            return -1;

    hash = hash_str(key);
    index = hash & hashmask(hashtable->order);
    bucket = &hashtable->buckets[index];
    pair = hashtable_find_pair(hashtable, bucket, key, hash);

    if(pair)
    {
        json_decref(pair->value);
        pair->value = value;
    }
    else
    {
        /* offsetof(...) returns the size of pair_t without the last,
           flexible member. This way, the correct amount is
           allocated. */

        size_t len = strlen(key);
        if(len >= (size_t)-1 - offsetof(pair_t, key)) {
            /* Avoid an overflow if the key is very long */
            return -1;
        }

        pair = jsonp_malloc(offsetof(pair_t, key) + len + 1);
        if(!pair)
            return -1;

        pair->hash = hash;
        strncpy(pair->key, key, len + 1);
        pair->value = value;
        list_init(&pair->list);
        list_init(&pair->ordered_list);

        insert_to_bucket(hashtable, bucket, &pair->list);
        list_insert(&hashtable->ordered_list, &pair->ordered_list);

        hashtable->size++;
    }
    return 0;
}

void *hashtable_get(hashtable_t *hashtable, const char *key)
{
    pair_t *pair;
    size_t hash;
    bucket_t *bucket;

    hash = hash_str(key);
    bucket = &hashtable->buckets[hash & hashmask(hashtable->order)];

    pair = hashtable_find_pair(hashtable, bucket, key, hash);
    if(!pair)
        return NULL;

    return pair->value;
}

int hashtable_del(hashtable_t *hashtable, const char *key)
{
    size_t hash = hash_str(key);
    return hashtable_do_del(hashtable, key, hash);
}

void hashtable_clear(hashtable_t *hashtable)
{
    size_t i;

    hashtable_do_clear(hashtable);

    for(i = 0; i < hashsize(hashtable->order); i++)
    {
        hashtable->buckets[i].first = hashtable->buckets[i].last =
            &hashtable->list;
    }

    list_init(&hashtable->list);
    list_init(&hashtable->ordered_list);
    hashtable->size = 0;
}

void *hashtable_iter(hashtable_t *hashtable)
{
    return hashtable_iter_next(hashtable, &hashtable->ordered_list);
}

void *hashtable_iter_at(hashtable_t *hashtable, const char *key)
{
    pair_t *pair;
    size_t hash;
    bucket_t *bucket;

    hash = hash_str(key);
    bucket = &hashtable->buckets[hash & hashmask(hashtable->order)];

    pair = hashtable_find_pair(hashtable, bucket, key, hash);
    if(!pair)
        return NULL;

    return &pair->ordered_list;
}

void *hashtable_iter_next(hashtable_t *hashtable, void *iter)
{
    list_t *list = (list_t *)iter;
    if(list->next == &hashtable->ordered_list)
        return NULL;
    return list->next;
}

void *hashtable_iter_key(void *iter)
{
    pair_t *pair = ordered_list_to_pair((list_t *)iter);
    return pair->key;
}

void *hashtable_iter_value(void *iter)
{
    pair_t *pair = ordered_list_to_pair((list_t *)iter);
    return pair->value;
}

void hashtable_iter_set(void *iter, json_t *value)
{
    pair_t *pair = ordered_list_to_pair((list_t *)iter);

    json_decref(pair->value);
    pair->value = value;
}
/* Generate sizeof(uint32_t) bytes of as random data as possible to seed
   the hash function.
*/

static uint32_t buf_to_uint32(char *data) {
    size_t i;
    uint32_t result = 0;

    for (i = 0; i < sizeof(uint32_t); i++)
        result = (result << 8) | (unsigned char)data[i];

    return result;
}

static int seed_from_randomdev(uint32_t *seed) {
    char data[sizeof(uint32_t)];
    int ok;

    int urandom;
    urandom = open(RANDOMDEV, O_RDONLY);
    if (urandom == -1)
        return 1;

    ok = read(urandom, data, sizeof(uint32_t)) == sizeof(uint32_t);
    close(urandom);

    if (!ok)
        return 1;

    *seed = buf_to_uint32(data);
    return 0;
}

/* gettimeofday() and getpid() */
static int seed_from_timestamp_and_pid(uint32_t *seed) {
    /* XOR of seconds and microseconds */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    *seed = (uint32_t)tv.tv_sec ^ (uint32_t)tv.tv_usec;
    /* XOR with PID for more randomness */
    *seed ^= (uint32_t)getpid();
    return 0;
}

static uint32_t generate_seed() {
    uint32_t seed;
    int done = 0;

    if (seed_from_randomdev(&seed) == 0)
        done = 1;

    if (!done) {
        /* Fall back to timestamp and PID if no better randomness is
           available */
        seed_from_timestamp_and_pid(&seed);
    }

    /* Make sure the seed is never zero */
    if (seed == 0)
        seed = 1;

    return seed;
}

/* Fall back to a thread-unsafe version */
void json_object_seed(size_t seed) {
    uint32_t new_seed = (uint32_t)seed;

    if (hashtable_seed == 0) {
        if (new_seed == 0)
            new_seed = generate_seed();

        hashtable_seed = new_seed;
    }
}

/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#define STREAM_STATE_OK        0
#define STREAM_STATE_EOF      -1
#define STREAM_STATE_ERROR    -2

#define TOKEN_INVALID         -1
#define TOKEN_EOF              0
#define TOKEN_STRING         256
#define TOKEN_INTEGER        257
#define TOKEN_REAL           258
#define TOKEN_TRUE           259
#define TOKEN_FALSE          260
#define TOKEN_NULL           261

/* Locale independent versions of isxxx() functions */
#define l_isupper(c)  ('A' <= (c) && (c) <= 'Z')
#define l_islower(c)  ('a' <= (c) && (c) <= 'z')
#define l_isalpha(c)  (l_isupper(c) || l_islower(c))
#define l_isdigit(c)  ('0' <= (c) && (c) <= '9')
#define l_isxdigit(c) \
    (l_isdigit(c) || ('A' <= (c) && (c) <= 'F') || ('a' <= (c) && (c) <= 'f'))

/* Read one byte from stream, convert to unsigned char, then int, and
   return. return EOF on end of file. This corresponds to the
   behaviour of fgetc(). */
typedef int (*get_func)(void *data);

typedef struct {
    get_func get;
    void *data;
    char buffer[5];
    size_t buffer_pos;
    int state;
    int line;
    int column, last_column;
    size_t position;
} stream_t;

typedef struct {
    stream_t stream;
    strbuffer_t saved_text;
    size_t flags;
    size_t depth;
    int token;
    union {
        struct {
            char *val;
            size_t len;
        } string;
        json_int_t integer;
        double real;
    } value;
} lex_t;

#define stream_to_lex(stream) container_of(stream, lex_t, stream)

/*** error reporting ***/

static void error_set(json_error_t *error, const lex_t *lex,
                      const char *msg, ...)
{
    va_list ap;
    char msg_text[JSON_ERROR_TEXT_LENGTH];
    char msg_with_context[JSON_ERROR_TEXT_LENGTH + 32];

    int line = -1, col = -1;
    size_t pos = 0;
    const char *result = msg_text;

    if(!error)
        return;

    va_start(ap, msg);
    vsnprintf(msg_text, JSON_ERROR_TEXT_LENGTH, msg, ap);
    msg_text[JSON_ERROR_TEXT_LENGTH - 1] = '\0';
    va_end(ap);

    if(lex)
    {
        const char *saved_text = strbuffer_value(&lex->saved_text);

        line = lex->stream.line;
        col = lex->stream.column;
        pos = lex->stream.position;

        if(saved_text && saved_text[0])
        {
            if(lex->saved_text.length <= 20) {
                snprintf(msg_with_context, JSON_ERROR_TEXT_LENGTH + 32,
                         "%s near '%s'", msg_text, saved_text);
                msg_with_context[JSON_ERROR_TEXT_LENGTH - 1] = '\0';
                result = msg_with_context;
            }
        }
        else
        {
            if(lex->stream.state == STREAM_STATE_ERROR) {
                /* No context for UTF-8 decoding errors */
                result = msg_text;
            }
            else {
                snprintf(msg_with_context, JSON_ERROR_TEXT_LENGTH + 32,
                         "%s near end of file", msg_text);
                msg_with_context[JSON_ERROR_TEXT_LENGTH - 1] = '\0';
                result = msg_with_context;
            }
        }
    }

    jsonp_error_set(error, line, col, pos, "%s", result);
}

/*** lexical analyzer ***/

static void
stream_init(stream_t *stream, get_func get, void *data)
{
    stream->get = get;
    stream->data = data;
    stream->buffer[0] = '\0';
    stream->buffer_pos = 0;

    stream->state = STREAM_STATE_OK;
    stream->line = 1;
    stream->column = 0;
    stream->position = 0;
}

static int stream_get(stream_t *stream, json_error_t *error)
{
    int c;

    if(stream->state != STREAM_STATE_OK)
        return stream->state;

    if(!stream->buffer[stream->buffer_pos])
    {
        c = stream->get(stream->data);
        if(c == EOF) {
            stream->state = STREAM_STATE_EOF;
            return STREAM_STATE_EOF;
        }

        stream->buffer[0] = c;
        stream->buffer_pos = 0;

        if(0x80 <= c && c <= 0xFF)
        {
            /* multi-byte UTF-8 sequence */
            size_t i, count;

            count = utf8_check_first(c);
            if(!count)
                goto out;

            assert(count >= 2);

            for(i = 1; i < count; i++)
                stream->buffer[i] = stream->get(stream->data);

            if(!utf8_check_full(stream->buffer, count, NULL))
                goto out;

            stream->buffer[count] = '\0';
        }
        else
            stream->buffer[1] = '\0';
    }

    c = stream->buffer[stream->buffer_pos++];

    stream->position++;
    if(c == '\n') {
        stream->line++;
        stream->last_column = stream->column;
        stream->column = 0;
    }
    else if(utf8_check_first(c)) {
        /* track the Unicode character column, so increment only if
           this is the first character of a UTF-8 sequence */
        stream->column++;
    }

    return c;

out:
    stream->state = STREAM_STATE_ERROR;
    error_set(error, stream_to_lex(stream), "unable to decode byte 0x%x", c);
    return STREAM_STATE_ERROR;
}

static void stream_unget(stream_t *stream, int c)
{
    if(c == STREAM_STATE_EOF || c == STREAM_STATE_ERROR)
        return;

    stream->position--;
    if(c == '\n') {
        stream->line--;
        stream->column = stream->last_column;
    }
    else if(utf8_check_first(c))
        stream->column--;

    assert(stream->buffer_pos > 0);
    stream->buffer_pos--;
    assert(stream->buffer[stream->buffer_pos] == c);
}

static int lex_get(lex_t *lex, json_error_t *error)
{
    return stream_get(&lex->stream, error);
}

static void lex_save(lex_t *lex, int c)
{
    strbuffer_append_byte(&lex->saved_text, c);
}

static int lex_get_save(lex_t *lex, json_error_t *error)
{
    int c = stream_get(&lex->stream, error);
    if(c != STREAM_STATE_EOF && c != STREAM_STATE_ERROR)
        lex_save(lex, c);
    return c;
}

static void lex_unget(lex_t *lex, int c)
{
    stream_unget(&lex->stream, c);
}

static void lex_unget_unsave(lex_t *lex, int c)
{
    if(c != STREAM_STATE_EOF && c != STREAM_STATE_ERROR) {
        /* Since we treat warnings as errors, when assertions are turned
         * off the "d" variable would be set but never used. Which is
         * treated as an error by GCC.
         */
        #ifndef NDEBUG
        char d;
        #endif
        stream_unget(&lex->stream, c);
        #ifndef NDEBUG
        d =
        #endif
            strbuffer_pop(&lex->saved_text);
        assert(c == d);
    }
}

static void lex_save_cached(lex_t *lex)
{
    while(lex->stream.buffer[lex->stream.buffer_pos] != '\0')
    {
        lex_save(lex, lex->stream.buffer[lex->stream.buffer_pos]);
        lex->stream.buffer_pos++;
        lex->stream.position++;
    }
}

static void lex_free_string(lex_t *lex)
{
    jsonp_free(lex->value.string.val);
    lex->value.string.val = NULL;
    lex->value.string.len = 0;
}

/* assumes that str points to 'u' plus at least 4 valid hex digits */
static int32_t decode_unicode_escape(const char *str)
{
    int i;
    int32_t value = 0;

    assert(str[0] == 'u');

    for(i = 1; i <= 4; i++) {
        char c = str[i];
        value <<= 4;
        if(l_isdigit(c))
            value += c - '0';
        else if(l_islower(c))
            value += c - 'a' + 10;
        else if(l_isupper(c))
            value += c - 'A' + 10;
        else
            return -1;
    }

    return value;
}

static void lex_scan_string(lex_t *lex, json_error_t *error)
{
    int c;
    const char *p;
    char *t;
    int i;

    lex->value.string.val = NULL;
    lex->token = TOKEN_INVALID;

    c = lex_get_save(lex, error);

    while(c != '"') {
        if(c == STREAM_STATE_ERROR)
            goto out;

        else if(c == STREAM_STATE_EOF) {
            error_set(error, lex, "premature end of input");
            goto out;
        }

        else if(0 <= c && c <= 0x1F) {
            /* control character */
            lex_unget_unsave(lex, c);
            if(c == '\n')
                error_set(error, lex, "unexpected newline");
            else
                error_set(error, lex, "control character 0x%x", c);
            goto out;
        }

        else if(c == '\\') {
            c = lex_get_save(lex, error);
            if(c == 'u') {
                c = lex_get_save(lex, error);
                for(i = 0; i < 4; i++) {
                    if(!l_isxdigit(c)) {
                        error_set(error, lex, "invalid escape");
                        goto out;
                    }
                    c = lex_get_save(lex, error);
                }
            }
            else if(c == '"' || c == '\\' || c == '/' || c == 'b' ||
                    c == 'f' || c == 'n' || c == 'r' || c == 't')
                c = lex_get_save(lex, error);
            else {
                error_set(error, lex, "invalid escape");
                goto out;
            }
        }
        else
            c = lex_get_save(lex, error);
    }

    /* the actual value is at most of the same length as the source
       string, because:
         - shortcut escapes (e.g. "\t") (length 2) are converted to 1 byte
         - a single \uXXXX escape (length 6) is converted to at most 3 bytes
         - two \uXXXX escapes (length 12) forming an UTF-16 surrogate pair
           are converted to 4 bytes
    */
    t = jsonp_malloc(lex->saved_text.length + 1);
    if(!t) {
        /* this is not very nice, since TOKEN_INVALID is returned */
        goto out;
    }
    lex->value.string.val = t;

    /* + 1 to skip the " */
    p = strbuffer_value(&lex->saved_text) + 1;

    while(*p != '"') {
        if(*p == '\\') {
            p++;
            if(*p == 'u') {
                size_t length = 0;
                int32_t value;

                value = decode_unicode_escape(p);
                if(value < 0) {
                    error_set(error, lex,
                        "invalid Unicode escape '%.6s'", p - 1);
                    goto out;
                }
                p += 5;

                if(0xD800 <= value && value <= 0xDBFF) {
                    /* surrogate pair */
                    if(*p == '\\' && *(p + 1) == 'u') {
                        int32_t value2 = decode_unicode_escape(++p);
                        if(value2 < 0) {
                            error_set(error, lex,
                                "invalid Unicode escape '%.6s'", p - 1);
                            goto out;
                        }
                        p += 5;

                        if(0xDC00 <= value2 && value2 <= 0xDFFF) {
                            /* valid second surrogate */
                            value =
                                ((value - 0xD800) << 10) +
                                (value2 - 0xDC00) +
                                0x10000;
                        }
                        else {
                            /* invalid second surrogate */
                            error_set(error, lex,
                                      "invalid Unicode '\\u%04X\\u%04X'",
                                      value, value2);
                            goto out;
                        }
                    }
                    else {
                        /* no second surrogate */
                        error_set(error, lex, "invalid Unicode '\\u%04X'",
                                  value);
                        goto out;
                    }
                }
                else if(0xDC00 <= value && value <= 0xDFFF) {
                    error_set(error, lex, "invalid Unicode '\\u%04X'", value);
                    goto out;
                }

                if(utf8_encode(value, t, &length))
                    assert(0);
                t += length;
            }
            else {
                switch(*p) {
                    case '"': case '\\': case '/':
                        *t = *p; break;
                    case 'b': *t = '\b'; break;
                    case 'f': *t = '\f'; break;
                    case 'n': *t = '\n'; break;
                    case 'r': *t = '\r'; break;
                    case 't': *t = '\t'; break;
                    default: assert(0);
                }
                t++;
                p++;
            }
        }
        else
            *(t++) = *(p++);
    }
    *t = '\0';
    lex->value.string.len = t - lex->value.string.val;
    lex->token = TOKEN_STRING;
    return;

out:
    lex_free_string(lex);
}

#define json_strtoint     strtoll

static int lex_scan_number(lex_t *lex, int c, json_error_t *error)
{
    const char *saved_text;
    char *end;
    double doubleval;

    lex->token = TOKEN_INVALID;

    if(c == '-')
        c = lex_get_save(lex, error);

    if(c == '0') {
        c = lex_get_save(lex, error);
        if(l_isdigit(c)) {
            lex_unget_unsave(lex, c);
            goto out;
        }
    }
    else if(l_isdigit(c)) {
        do
            c = lex_get_save(lex, error);
        while(l_isdigit(c));
    }
    else {
        lex_unget_unsave(lex, c);
        goto out;
    }

    if(!(lex->flags & JSON_DECODE_INT_AS_REAL) &&
       c != '.' && c != 'E' && c != 'e')
    {
        json_int_t intval;

        lex_unget_unsave(lex, c);

        saved_text = strbuffer_value(&lex->saved_text);

        errno = 0;
        intval = json_strtoint(saved_text, &end, 10);
        if(errno == ERANGE) {
            if(intval < 0)
                error_set(error, lex, "too big negative integer");
            else
                error_set(error, lex, "too big integer");
            goto out;
        }

        assert(end == saved_text + lex->saved_text.length);

        lex->token = TOKEN_INTEGER;
        lex->value.integer = intval;
        return 0;
    }

    if(c == '.') {
        c = lex_get(lex, error);
        if(!l_isdigit(c)) {
            lex_unget(lex, c);
            goto out;
        }
        lex_save(lex, c);

        do
            c = lex_get_save(lex, error);
        while(l_isdigit(c));
    }

    if(c == 'E' || c == 'e') {
        c = lex_get_save(lex, error);
        if(c == '+' || c == '-')
            c = lex_get_save(lex, error);

        if(!l_isdigit(c)) {
            lex_unget_unsave(lex, c);
            goto out;
        }

        do
            c = lex_get_save(lex, error);
        while(l_isdigit(c));
    }

    lex_unget_unsave(lex, c);

    if(jsonp_strtod(&lex->saved_text, &doubleval)) {
        error_set(error, lex, "real number overflow");
        goto out;
    }

    lex->token = TOKEN_REAL;
    lex->value.real = doubleval;
    return 0;

out:
    return -1;
}

static int lex_scan(lex_t *lex, json_error_t *error)
{
    int c;

    strbuffer_clear(&lex->saved_text);

    if(lex->token == TOKEN_STRING)
        lex_free_string(lex);

    do
        c = lex_get(lex, error);
    while(c == ' ' || c == '\t' || c == '\n' || c == '\r');

    if(c == STREAM_STATE_EOF) {
        lex->token = TOKEN_EOF;
        goto out;
    }

    if(c == STREAM_STATE_ERROR) {
        lex->token = TOKEN_INVALID;
        goto out;
    }

    lex_save(lex, c);

    if(c == '{' || c == '}' || c == '[' || c == ']' || c == ':' || c == ',')
        lex->token = c;

    else if(c == '"')
        lex_scan_string(lex, error);

    else if(l_isdigit(c) || c == '-') {
        if(lex_scan_number(lex, c, error))
            goto out;
    }

    else if(l_isalpha(c)) {
        /* eat up the whole identifier for clearer error messages */
        const char *saved_text;

        do
            c = lex_get_save(lex, error);
        while(l_isalpha(c));
        lex_unget_unsave(lex, c);

        saved_text = strbuffer_value(&lex->saved_text);

        if(strcmp(saved_text, "true") == 0)
            lex->token = TOKEN_TRUE;
        else if(strcmp(saved_text, "false") == 0)
            lex->token = TOKEN_FALSE;
        else if(strcmp(saved_text, "null") == 0)
            lex->token = TOKEN_NULL;
        else
            lex->token = TOKEN_INVALID;
    }

    else {
        /* save the rest of the input UTF-8 sequence to get an error
           message of valid UTF-8 */
        lex_save_cached(lex);
        lex->token = TOKEN_INVALID;
    }

out:
    return lex->token;
}

static char *lex_steal_string(lex_t *lex, size_t *out_len)
{
    char *result = NULL;
    if(lex->token == TOKEN_STRING) {
        result = lex->value.string.val;
        *out_len = lex->value.string.len;
        lex->value.string.val = NULL;
        lex->value.string.len = 0;
    }
    return result;
}

static int lex_init(lex_t *lex, get_func get, size_t flags, void *data)
{
    stream_init(&lex->stream, get, data);
    if(strbuffer_init(&lex->saved_text))
        return -1;

    lex->flags = flags;
    lex->token = TOKEN_INVALID;
    return 0;
}

static void lex_close(lex_t *lex)
{
    if(lex->token == TOKEN_STRING)
        lex_free_string(lex);
    strbuffer_close(&lex->saved_text);
}

/*** parser ***/

static json_t *parse_value(lex_t *lex, size_t flags, json_error_t *error);

static json_t *parse_object(lex_t *lex, size_t flags, json_error_t *error)
{
    json_t *object = json_object();
    if(!object)
        return NULL;

    lex_scan(lex, error);
    if(lex->token == '}')
        return object;

    while(1) {
        char *key;
        size_t len;
        json_t *value;

        if(lex->token != TOKEN_STRING) {
            error_set(error, lex, "string or '}' expected");
            goto error;
        }

        key = lex_steal_string(lex, &len);
        if(!key)
            return NULL;
        if (memchr(key, '\0', len)) {
            jsonp_free(key);
            error_set(error, lex, "NUL byte in object key not supported");
            goto error;
        }

        if(flags & JSON_REJECT_DUPLICATES) {
            if(json_object_get(object, key)) {
                jsonp_free(key);
                error_set(error, lex, "duplicate object key");
                goto error;
            }
        }

        lex_scan(lex, error);
        if(lex->token != ':') {
            jsonp_free(key);
            error_set(error, lex, "':' expected");
            goto error;
        }

        lex_scan(lex, error);
        value = parse_value(lex, flags, error);
        if(!value) {
            jsonp_free(key);
            goto error;
        }

        if(json_object_set_nocheck(object, key, value)) {
            jsonp_free(key);
            json_decref(value);
            goto error;
        }

        json_decref(value);
        jsonp_free(key);

        lex_scan(lex, error);
        if(lex->token != ',')
            break;

        lex_scan(lex, error);
    }

    if(lex->token != '}') {
        error_set(error, lex, "'}' expected");
        goto error;
    }

    return object;

error:
    json_decref(object);
    return NULL;
}

static json_t *parse_array(lex_t *lex, size_t flags, json_error_t *error)
{
    json_t *array = json_array();
    if(!array)
        return NULL;

    lex_scan(lex, error);
    if(lex->token == ']')
        return array;

    while(lex->token) {
        json_t *elem = parse_value(lex, flags, error);
        if(!elem)
            goto error;

        if(json_array_append(array, elem)) {
            json_decref(elem);
            goto error;
        }
        json_decref(elem);

        lex_scan(lex, error);
        if(lex->token != ',')
            break;

        lex_scan(lex, error);
    }

    if(lex->token != ']') {
        error_set(error, lex, "']' expected");
        goto error;
    }

    return array;

error:
    json_decref(array);
    return NULL;
}

static json_t *parse_value(lex_t *lex, size_t flags, json_error_t *error)
{
    json_t *json;

    lex->depth++;
    if(lex->depth > JSON_PARSER_MAX_DEPTH) {
        error_set(error, lex, "maximum parsing depth reached");
        return NULL;
    }

    switch(lex->token) {
        case TOKEN_STRING: {
            const char *value = lex->value.string.val;
            size_t len = lex->value.string.len;

            if(!(flags & JSON_ALLOW_NUL)) {
                if(memchr(value, '\0', len)) {
                    error_set(error, lex,
                        "\\u0000 is not allowed without JSON_ALLOW_NUL");
                    return NULL;
                }
            }

            json = jsonp_stringn_nocheck_own(value, len);
            if(json) {
                lex->value.string.val = NULL;
                lex->value.string.len = 0;
            }
            break;
        }

        case TOKEN_INTEGER: {
            json = json_integer(lex->value.integer);
            break;
        }

        case TOKEN_REAL: {
            json = json_real(lex->value.real);
            break;
        }

        case TOKEN_TRUE:
            json = json_true();
            break;

        case TOKEN_FALSE:
            json = json_false();
            break;

        case TOKEN_NULL:
            json = json_null();
            break;

        case '{':
            json = parse_object(lex, flags, error);
            break;

        case '[':
            json = parse_array(lex, flags, error);
            break;

        case TOKEN_INVALID:
            error_set(error, lex, "invalid token");
            return NULL;

        default:
            error_set(error, lex, "unexpected token");
            return NULL;
    }

    if(!json)
        return NULL;

    lex->depth--;
    return json;
}

static json_t *parse_json(lex_t *lex, size_t flags, json_error_t *error)
{
    json_t *result;

    lex->depth = 0;

    lex_scan(lex, error);
    if(!(flags & JSON_DECODE_ANY)) {
        if(lex->token != '[' && lex->token != '{') {
            error_set(error, lex, "'[' or '{' expected");
            return NULL;
        }
    }

    result = parse_value(lex, flags, error);
    if(!result)
        return NULL;

    if(!(flags & JSON_DISABLE_EOF_CHECK)) {
        lex_scan(lex, error);
        if(lex->token != TOKEN_EOF) {
            error_set(error, lex, "end of file expected");
            json_decref(result);
            return NULL;
        }
    }

    if(error) {
        /* Save the position even though there was no error */
        error->position = (int)lex->stream.position;
    }

    return result;
}

typedef struct
{
    const char *data;
    size_t pos;
} string_data_t;

static int string_get(void *data)
{
    char c;
    string_data_t *stream = (string_data_t *)data;
    c = stream->data[stream->pos];
    if(c == '\0')
        return EOF;
    else
    {
        stream->pos++;
        return (unsigned char)c;
    }
}

json_t *json_loads(const char *string, size_t flags, json_error_t *error)
{
    lex_t lex;
    json_t *result;
    string_data_t stream_data;

    jsonp_error_init(error, "<string>");

    if (string == NULL) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    stream_data.data = string;
    stream_data.pos = 0;

    if(lex_init(&lex, string_get, flags, (void *)&stream_data))
        return NULL;

    result = parse_json(&lex, flags, error);

    lex_close(&lex);
    return result;
}

typedef struct
{
    const char *data;
    size_t len;
    size_t pos;
} buffer_data_t;

static int buffer_get(void *data)
{
    char c;
    buffer_data_t *stream = data;
    if(stream->pos >= stream->len)
      return EOF;

    c = stream->data[stream->pos];
    stream->pos++;
    return (unsigned char)c;
}

json_t *json_loadb(const char *buffer, size_t buflen, size_t flags,
    json_error_t *error)
{
    lex_t lex;
    json_t *result;
    buffer_data_t stream_data;

    jsonp_error_init(error, "<buffer>");

    if (buffer == NULL) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    stream_data.data = buffer;
    stream_data.pos = 0;
    stream_data.len = buflen;

    if(lex_init(&lex, buffer_get, flags, (void *)&stream_data))
        return NULL;

    result = parse_json(&lex, flags, error);

    lex_close(&lex);
    return result;
}

json_t *json_loadf(FILE *input, size_t flags, json_error_t *error)
{
    lex_t lex;
    const char *source;
    json_t *result;

    if(input == stdin)
        source = "<stdin>";
    else
        source = "<stream>";

    jsonp_error_init(error, source);

    if (input == NULL) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    if(lex_init(&lex, (get_func)fgetc, flags, input))
        return NULL;

    result = parse_json(&lex, flags, error);

    lex_close(&lex);
    return result;
}

static int fd_get_func(int *fd)
{
    uint8_t c;
    if (read(*fd, &c, 1) == 1)
        return c;
    return EOF;
}

json_t *json_loadfd(int input, size_t flags, json_error_t *error)
{
    lex_t lex;
    const char *source;
    json_t *result;

    if(input == STDIN_FILENO)
        source = "<stdin>";
    else
        source = "<stream>";

    jsonp_error_init(error, source);

    if (input < 0) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    if(lex_init(&lex, (get_func)fd_get_func, flags, &input))
        return NULL;

    result = parse_json(&lex, flags, error);

    lex_close(&lex);
    return result;
}

json_t *json_load_file(const char *path, size_t flags, json_error_t *error)
{
    json_t *result;
    FILE *fp;

    jsonp_error_init(error, path);

    if (path == NULL) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    fp = fopen(path, "rb");
    if(!fp)
    {
        error_set(error, NULL, "unable to open %s: %s",
                  path, strerror(errno));
        return NULL;
    }

    result = json_loadf(fp, flags, error);

    fclose(fp);
    return result;
}

#define MAX_BUF_LEN 1024

typedef struct
{
    char data[MAX_BUF_LEN];
    size_t len;
    size_t pos;
    json_load_callback_t callback;
    void *arg;
} callback_data_t;

static int callback_get(void *data)
{
    char c;
    callback_data_t *stream = data;

    if(stream->pos >= stream->len) {
        stream->pos = 0;
        stream->len = stream->callback(stream->data, MAX_BUF_LEN, stream->arg);
        if(stream->len == 0 || stream->len == (size_t)-1)
            return EOF;
    }

    c = stream->data[stream->pos];
    stream->pos++;
    return (unsigned char)c;
}

json_t *json_load_callback(json_load_callback_t callback, void *arg,
    size_t flags, json_error_t *error)
{
    lex_t lex;
    json_t *result;

    callback_data_t stream_data;

    memset(&stream_data, 0, sizeof(stream_data));
    stream_data.callback = callback;
    stream_data.arg = arg;

    jsonp_error_init(error, "<callback>");

    if (callback == NULL) {
        error_set(error, NULL, "wrong arguments");
        return NULL;
    }

    if(lex_init(&lex, (get_func)callback_get, flags, &stream_data))
        return NULL;

    result = parse_json(&lex, flags, error);

    lex_close(&lex);
    return result;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 * Copyright (c) 2011-2012 Basile Starynkevitch <basile@starynkevitch.net>
 *
 * Jansson is free software; you can redistribute it and/or modify it
 * under the terms of the MIT license. See LICENSE for details.
 */

/* C89 allows these to be macros */
#undef malloc
#undef free

/* memory function pointers */
static json_malloc_t do_malloc = malloc;
static json_free_t do_free = free;

void *jsonp_malloc(size_t size)
{
    if(!size)
        return NULL;

    return (*do_malloc)(size);
}

void jsonp_free(void *ptr)
{
    if(!ptr)
        return;

    (*do_free)(ptr);
}

char *jsonp_strdup(const char *str)
{
    return jsonp_strndup(str, strlen(str));
}

char *jsonp_strndup(const char *str, size_t len)
{
    char *new_str;

    new_str = jsonp_malloc(len + 1);
    if(!new_str)
        return NULL;

    memcpy(new_str, str, len);
    new_str[len] = '\0';
    return new_str;
}

void json_set_alloc_funcs(json_malloc_t malloc_fn, json_free_t free_fn)
{
    do_malloc = malloc_fn;
    do_free = free_fn;
}

void json_get_alloc_funcs(json_malloc_t *malloc_fn, json_free_t *free_fn)
{
    if (malloc_fn)
        *malloc_fn = do_malloc;
    if (free_fn)
        *free_fn = do_free;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 * Copyright (c) 2011-2012 Graeme Smecher <graeme.smecher@mail.mcgill.ca>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

typedef struct {
    int line;
    int column;
    size_t pos;
    char token;
} token_t;

typedef struct {
    const char *start;
    const char *fmt;
    token_t prev_token;
    token_t token;
    token_t next_token;
    json_error_t *error;
    size_t flags;
    int line;
    int column;
    size_t pos;
} scanner_t;

#define token(scanner) ((scanner)->token.token)

static const char * const type_names[] = {
    "object",
    "array",
    "string",
    "integer",
    "real",
    "true",
    "false",
    "null"
};

#define type_name(x) type_names[json_typeof(x)]

static const char unpack_value_starters[] = "{[siIbfFOon";

static void scanner_init(scanner_t *s, json_error_t *error,
                         size_t flags, const char *fmt)
{
    s->error = error;
    s->flags = flags;
    s->fmt = s->start = fmt;
    memset(&s->prev_token, 0, sizeof(token_t));
    memset(&s->token, 0, sizeof(token_t));
    memset(&s->next_token, 0, sizeof(token_t));
    s->line = 1;
    s->column = 0;
    s->pos = 0;
}

static void next_token(scanner_t *s)
{
    const char *t;
    s->prev_token = s->token;

    if(s->next_token.line) {
        s->token = s->next_token;
        s->next_token.line = 0;
        return;
    }

    t = s->fmt;
    s->column++;
    s->pos++;

    /* skip space and ignored chars */
    while(*t == ' ' || *t == '\t' || *t == '\n' || *t == ',' || *t == ':') {
        if(*t == '\n') {
            s->line++;
            s->column = 1;
        }
        else
            s->column++;

        s->pos++;
        t++;
    }

    s->token.token = *t;
    s->token.line = s->line;
    s->token.column = s->column;
    s->token.pos = s->pos;

    t++;
    s->fmt = t;
}

static void prev_token(scanner_t *s)
{
    s->next_token = s->token;
    s->token = s->prev_token;
}

static void set_error(scanner_t *s, const char *source, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    jsonp_error_vset(s->error, s->token.line, s->token.column, s->token.pos,
                     fmt, ap);

    jsonp_error_set_source(s->error, source);

    va_end(ap);
}

static json_t *pack(scanner_t *s, va_list *ap);

/* ours will be set to 1 if jsonp_free() must be called for the result
   afterwards */
static char *read_string(scanner_t *s, va_list *ap,
                         const char *purpose, size_t *out_len, int *ours)
{
    char t;
    strbuffer_t strbuff;
    const char *str;
    size_t length;

    next_token(s);
    t = token(s);
    prev_token(s);

    if(t != '#' && t != '%' && t != '+') {
        /* Optimize the simple case */
        str = va_arg(*ap, const char *);

        if(!str) {
            set_error(s, "<args>", "NULL string argument");
            return NULL;
        }

        length = strlen(str);

        if(!utf8_check_string(str, length)) {
            set_error(s, "<args>", "Invalid UTF-8 %s", purpose);
            return NULL;
        }

        *out_len = length;
        *ours = 0;
        return (char *)str;
    }

    strbuffer_init(&strbuff);

    while(1) {
        str = va_arg(*ap, const char *);
        if(!str) {
            set_error(s, "<args>", "NULL string argument");
            strbuffer_close(&strbuff);
            return NULL;
        }

        next_token(s);

        if(token(s) == '#') {
            length = va_arg(*ap, int);
        }
        else if(token(s) == '%') {
            length = va_arg(*ap, size_t);
        }
        else {
            prev_token(s);
            length = strlen(str);
        }

        if(strbuffer_append_bytes(&strbuff, str, length) == -1) {
            set_error(s, "<internal>", "Out of memory");
            strbuffer_close(&strbuff);
            return NULL;
        }

        next_token(s);
        if(token(s) != '+') {
            prev_token(s);
            break;
        }
    }

    if(!utf8_check_string(strbuff.value, strbuff.length)) {
        set_error(s, "<args>", "Invalid UTF-8 %s", purpose);
        strbuffer_close(&strbuff);
        return NULL;
    }

    *out_len = strbuff.length;
    *ours = 1;
    return strbuffer_steal_value(&strbuff);
}

static json_t *pack_object(scanner_t *s, va_list *ap)
{
    json_t *object = json_object();
    next_token(s);

    while(token(s) != '}') {
        char *key;
        size_t len;
        int ours;
        json_t *value;

        if(!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            goto error;
        }

        if(token(s) != 's') {
            set_error(s, "<format>", "Expected format 's', got '%c'", token(s));
            goto error;
        }

        key = read_string(s, ap, "object key", &len, &ours);
        if(!key)
            goto error;

        next_token(s);

        value = pack(s, ap);
        if(!value) {
            if(ours)
                jsonp_free(key);

            goto error;
        }

        if(json_object_set_new_nocheck(object, key, value)) {
            set_error(s, "<internal>", "Unable to add key \"%s\"", key);
            if(ours)
                jsonp_free(key);

            goto error;
        }

        if(ours)
            jsonp_free(key);

        next_token(s);
    }

    return object;

error:
    json_decref(object);
    return NULL;
}

static json_t *pack_array(scanner_t *s, va_list *ap)
{
    json_t *array = json_array();
    next_token(s);

    while(token(s) != ']') {
        json_t *value;

        if(!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            goto error;
        }

        value = pack(s, ap);
        if(!value)
            goto error;

        if(json_array_append_new(array, value)) {
            set_error(s, "<internal>", "Unable to append to array");
            goto error;
        }

        next_token(s);
    }
    return array;

error:
    json_decref(array);
    return NULL;
}

static json_t *pack_string(scanner_t *s, va_list *ap)
{
    char *str;
    size_t len;
    int ours;
    int nullable;

    next_token(s);
    nullable = token(s) == '?';
    if (!nullable)
        prev_token(s);

    str = read_string(s, ap, "string", &len, &ours);
    if (!str) {
        return nullable ? json_null() : NULL;
    } else if (ours) {
        return jsonp_stringn_nocheck_own(str, len);
    } else {
        return json_stringn_nocheck(str, len);
    }
}

static json_t *pack(scanner_t *s, va_list *ap)
{
    switch(token(s)) {
        case '{':
            return pack_object(s, ap);

        case '[':
            return pack_array(s, ap);

        case 's': /* string */
            return pack_string(s, ap);

        case 'n': /* null */
            return json_null();

        case 'b': /* boolean */
            return va_arg(*ap, int) ? json_true() : json_false();

        case 'i': /* integer from int */
            return json_integer(va_arg(*ap, int));

        case 'I': /* integer from json_int_t */
            return json_integer(va_arg(*ap, json_int_t));

        case 'f': /* real */
            return json_real(va_arg(*ap, double));

        case 'O': /* a json_t object; increments refcount */
        {
            int nullable;
            json_t *json;

            next_token(s);
            nullable = token(s) == '?';
            if (!nullable)
                prev_token(s);

            json = va_arg(*ap, json_t *);
            if (!json && nullable) {
                return json_null();
            } else {
                return json_incref(json);
            }
        }

        case 'o': /* a json_t object; doesn't increment refcount */
        {
            int nullable;
            json_t *json;

            next_token(s);
            nullable = token(s) == '?';
            if (!nullable)
                prev_token(s);

            json = va_arg(*ap, json_t *);
            if (!json && nullable) {
                return json_null();
            } else {
                return json;
            }
        }

        default:
            set_error(s, "<format>", "Unexpected format character '%c'",
                      token(s));
            return NULL;
    }
}

static int unpack(scanner_t *s, json_t *root, va_list *ap);

static int unpack_object(scanner_t *s, json_t *root, va_list *ap)
{
    int ret = -1;
    int strict = 0;
    int gotopt = 0;

    /* Use a set (emulated by a hashtable) to check that all object
       keys are accessed. Checking that the correct number of keys
       were accessed is not enough, as the same key can be unpacked
       multiple times.
    */
    hashtable_t key_set;

    if(hashtable_init(&key_set)) {
        set_error(s, "<internal>", "Out of memory");
        return -1;
    }

    if(root && !json_is_object(root)) {
        set_error(s, "<validation>", "Expected object, got %s",
                  type_name(root));
        goto out;
    }
    next_token(s);

    while(token(s) != '}') {
        const char *key;
        json_t *value;
        int opt = 0;

        if(strict != 0) {
            set_error(s, "<format>", "Expected '}' after '%c', got '%c'",
                      (strict == 1 ? '!' : '*'), token(s));
            goto out;
        }

        if(!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            goto out;
        }

        if(token(s) == '!' || token(s) == '*') {
            strict = (token(s) == '!' ? 1 : -1);
            next_token(s);
            continue;
        }

        if(token(s) != 's') {
            set_error(s, "<format>", "Expected format 's', got '%c'",
                token(s));
            goto out;
        }

        key = va_arg(*ap, const char *);
        if(!key) {
            set_error(s, "<args>", "NULL object key");
            goto out;
        }

        next_token(s);

        if(token(s) == '?') {
            opt = gotopt = 1;
            next_token(s);
        }

        if(!root) {
            /* skipping */
            value = NULL;
        }
        else {
            value = json_object_get(root, key);
            if(!value && !opt) {
                set_error(s, "<validation>", "Object item not found: %s", key);
                goto out;
            }
        }

        if(unpack(s, value, ap))
            goto out;

        hashtable_set(&key_set, key, json_null());
        next_token(s);
    }

    if(strict == 0 && (s->flags & JSON_STRICT))
        strict = 1;

    if(root && strict == 1) {
        /* We need to check that all non optional items have been parsed */
        const char *key;
        int have_unrecognized_keys = 0;
        strbuffer_t unrecognized_keys;
        json_t *value;
        long unpacked = 0;
        if (gotopt) {
            /* We have optional keys, we need to iter on each key */
            json_object_foreach(root, key, value) {
                if(!hashtable_get(&key_set, key)) {
                    unpacked++;

                    /* Save unrecognized keys for the error message */
                    if (!have_unrecognized_keys) {
                        strbuffer_init(&unrecognized_keys);
                        have_unrecognized_keys = 1;
                    } else {
                        strbuffer_append_bytes(&unrecognized_keys, ", ", 2);
                    }
                    strbuffer_append_bytes(&unrecognized_keys, key,
                        strlen(key));
                }
            }
        } else {
            /* No optional keys, we can just compare the number of items */
            unpacked = (long)json_object_size(root) - (long)key_set.size;
        }
        if (unpacked) {
            if (!gotopt) {
                /* Save unrecognized keys for the error message */
                json_object_foreach(root, key, value) {
                    if(!hashtable_get(&key_set, key)) {
                        if (!have_unrecognized_keys) {
                            strbuffer_init(&unrecognized_keys);
                            have_unrecognized_keys = 1;
                        } else {
                            strbuffer_append_bytes(&unrecognized_keys, ", ",
                                2);
                        }
                        strbuffer_append_bytes(&unrecognized_keys, key,
                            strlen(key));
                    }
                }
            }
            set_error(s, "<validation>",
                      "%li object item(s) left unpacked: %s",
                      unpacked, strbuffer_value(&unrecognized_keys));
            strbuffer_close(&unrecognized_keys);
            goto out;
        }
    }

    ret = 0;

out:
    hashtable_close(&key_set);
    return ret;
}

static int unpack_array(scanner_t *s, json_t *root, va_list *ap)
{
    size_t i = 0;
    int strict = 0;

    if(root && !json_is_array(root)) {
        set_error(s, "<validation>", "Expected array, got %s",
            type_name(root));
        return -1;
    }
    next_token(s);

    while(token(s) != ']') {
        json_t *value;

        if(strict != 0) {
            set_error(s, "<format>", "Expected ']' after '%c', got '%c'",
                      (strict == 1 ? '!' : '*'),
                      token(s));
            return -1;
        }

        if(!token(s)) {
            set_error(s, "<format>", "Unexpected end of format string");
            return -1;
        }

        if(token(s) == '!' || token(s) == '*') {
            strict = (token(s) == '!' ? 1 : -1);
            next_token(s);
            continue;
        }

        if(!strchr(unpack_value_starters, token(s))) {
            set_error(s, "<format>", "Unexpected format character '%c'",
                      token(s));
            return -1;
        }

        if(!root) {
            /* skipping */
            value = NULL;
        }
        else {
            value = json_array_get(root, i);
            if(!value) {
                set_error(s, "<validation>", "Array index %lu out of range",
                          (unsigned long)i);
                return -1;
            }
        }

        if(unpack(s, value, ap))
            return -1;

        next_token(s);
        i++;
    }

    if(strict == 0 && (s->flags & JSON_STRICT))
        strict = 1;

    if(root && strict == 1 && i != json_array_size(root)) {
        long diff = (long)json_array_size(root) - (long)i;
        set_error(s, "<validation>", "%li array item(s) left unpacked", diff);
        return -1;
    }

    return 0;
}

static int unpack(scanner_t *s, json_t *root, va_list *ap)
{
    switch(token(s))
    {
        case '{':
            return unpack_object(s, root, ap);

        case '[':
            return unpack_array(s, root, ap);

        case 's':
            if(root && !json_is_string(root)) {
                set_error(s, "<validation>", "Expected string, got %s",
                          type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                const char **str_target;
                size_t *len_target = NULL;

                str_target = va_arg(*ap, const char **);
                if(!str_target) {
                    set_error(s, "<args>", "NULL string argument");
                    return -1;
                }

                next_token(s);

                if(token(s) == '%') {
                    len_target = va_arg(*ap, size_t *);
                    if(!len_target) {
                        set_error(s, "<args>", "NULL string length argument");
                        return -1;
                    }
                }
                else
                    prev_token(s);

                if(root) {
                    *str_target = json_string_value(root);
                    if(len_target)
                        *len_target = json_string_length(root);
                }
            }
            return 0;

        case 'i':
            if(root && !json_is_integer(root)) {
                set_error(s, "<validation>", "Expected integer, got %s",
                          type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                int *target = va_arg(*ap, int*);
                if(root)
                    *target = (int)json_integer_value(root);
            }

            return 0;

        case 'I':
            if(root && !json_is_integer(root)) {
                set_error(s, "<validation>", "Expected integer, got %s",
                          type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                json_int_t *target = va_arg(*ap, json_int_t*);
                if(root)
                    *target = json_integer_value(root);
            }

            return 0;

        case 'b':
            if(root && !json_is_boolean(root)) {
                set_error(s, "<validation>", "Expected true or false, got %s",
                          type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                int *target = va_arg(*ap, int*);
                if(root)
                    *target = json_is_true(root);
            }

            return 0;

        case 'f':
            if(root && !json_is_real(root)) {
                set_error(s, "<validation>", "Expected real, got %s",
                          type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                double *target = va_arg(*ap, double*);
                if(root)
                    *target = json_real_value(root);
            }

            return 0;

        case 'F':
            if(root && !json_is_number(root)) {
                set_error(s, "<validation>",
                          "Expected real or integer, got %s", type_name(root));
                return -1;
            }

            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                double *target = va_arg(*ap, double*);
                if(root)
                    *target = json_number_value(root);
            }

            return 0;

        case 'O':
            if(root && !(s->flags & JSON_VALIDATE_ONLY))
                json_incref(root);
            /* Fall through */

        case 'o':
            if(!(s->flags & JSON_VALIDATE_ONLY)) {
                json_t **target = va_arg(*ap, json_t**);
                if(root)
                    *target = root;
            }

            return 0;

        case 'n':
            /* Never assign, just validate */
            if(root && !json_is_null(root)) {
                set_error(s, "<validation>", "Expected null, got %s",
                          type_name(root));
                return -1;
            }
            return 0;

        default:
            set_error(s, "<format>", "Unexpected format character '%c'",
                      token(s));
            return -1;
    }
}

json_t *json_vpack_ex(json_error_t *error, size_t flags,
                      const char *fmt, va_list ap)
{
    scanner_t s;
    va_list ap_copy;
    json_t *value;

    if(!fmt || !*fmt) {
        jsonp_error_init(error, "<format>");
        jsonp_error_set(error, -1, -1, 0, "NULL or empty format string");
        return NULL;
    }
    jsonp_error_init(error, NULL);

    scanner_init(&s, error, flags, fmt);
    next_token(&s);

    va_copy(ap_copy, ap);
    value = pack(&s, &ap_copy);
    va_end(ap_copy);

    if(!value)
        return NULL;

    next_token(&s);
    if(token(&s)) {
        json_decref(value);
        set_error(&s, "<format>", "Garbage after format string");
        return NULL;
    }

    return value;
}

json_t *json_pack_ex(json_error_t *error, size_t flags, const char *fmt, ...)
{
    json_t *value;
    va_list ap;

    va_start(ap, fmt);
    value = json_vpack_ex(error, flags, fmt, ap);
    va_end(ap);

    return value;
}

json_t *json_pack(const char *fmt, ...)
{
    json_t *value;
    va_list ap;

    va_start(ap, fmt);
    value = json_vpack_ex(NULL, 0, fmt, ap);
    va_end(ap);

    return value;
}

int json_vunpack_ex(json_t *root, json_error_t *error, size_t flags,
                    const char *fmt, va_list ap)
{
    scanner_t s;
    va_list ap_copy;

    if(!root) {
        jsonp_error_init(error, "<root>");
        jsonp_error_set(error, -1, -1, 0, "NULL root value");
        return -1;
    }

    if(!fmt || !*fmt) {
        jsonp_error_init(error, "<format>");
        jsonp_error_set(error, -1, -1, 0, "NULL or empty format string");
        return -1;
    }
    jsonp_error_init(error, NULL);

    scanner_init(&s, error, flags, fmt);
    next_token(&s);

    va_copy(ap_copy, ap);
    if(unpack(&s, root, &ap_copy)) {
        va_end(ap_copy);
        return -1;
    }
    va_end(ap_copy);

    next_token(&s);
    if(token(&s)) {
        set_error(&s, "<format>", "Garbage after format string");
        return -1;
    }

    return 0;
}

int json_unpack_ex(json_t *root, json_error_t *error, size_t flags,
    const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = json_vunpack_ex(root, error, flags, fmt, ap);
    va_end(ap);

    return ret;
}

int json_unpack(json_t *root, const char *fmt, ...)
{
    int ret;
    va_list ap;

    va_start(ap, fmt);
    ret = json_vunpack_ex(root, NULL, 0, fmt, ap);
    va_end(ap);

    return ret;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

#define STRBUFFER_MIN_SIZE  16
#define STRBUFFER_FACTOR    2
#define STRBUFFER_SIZE_MAX  ((size_t)-1)

int strbuffer_init(strbuffer_t *strbuff)
{
    strbuff->size = STRBUFFER_MIN_SIZE;
    strbuff->length = 0;

    strbuff->value = jsonp_malloc(strbuff->size);
    if(!strbuff->value)
        return -1;

    /* initialize to empty */
    strbuff->value[0] = '\0';
    return 0;
}

void strbuffer_close(strbuffer_t *strbuff)
{
    if(strbuff->value)
        jsonp_free(strbuff->value);

    strbuff->size = 0;
    strbuff->length = 0;
    strbuff->value = NULL;
}

void strbuffer_clear(strbuffer_t *strbuff)
{
    strbuff->length = 0;
    strbuff->value[0] = '\0';
}

const char *strbuffer_value(const strbuffer_t *strbuff)
{
    return strbuff->value;
}

char *strbuffer_steal_value(strbuffer_t *strbuff)
{
    char *result = strbuff->value;
    strbuff->value = NULL;
    return result;
}

int strbuffer_append_byte(strbuffer_t *strbuff, char byte)
{
    return strbuffer_append_bytes(strbuff, &byte, 1);
}

int strbuffer_append_bytes(strbuffer_t *strbuff, const char *data, size_t size)
{
    if(size >= strbuff->size - strbuff->length)
    {
        size_t new_size;
        char *new_value;

        /* avoid integer overflow */
        if (strbuff->size > STRBUFFER_SIZE_MAX / STRBUFFER_FACTOR
            || size > STRBUFFER_SIZE_MAX - 1
            || strbuff->length > STRBUFFER_SIZE_MAX - 1 - size)
            return -1;

        new_size = max(strbuff->size * STRBUFFER_FACTOR,
                       strbuff->length + size + 1);

        new_value = jsonp_malloc(new_size);
        if(!new_value)
            return -1;

        memcpy(new_value, strbuff->value, strbuff->length);

        jsonp_free(strbuff->value);
        strbuff->value = new_value;
        strbuff->size = new_size;
    }

    memcpy(strbuff->value + strbuff->length, data, size);
    strbuff->length += size;
    strbuff->value[strbuff->length] = '\0';

    return 0;
}

char strbuffer_pop(strbuffer_t *strbuff)
{
    if(strbuff->length > 0) {
        char c = strbuff->value[--strbuff->length];
        strbuff->value[strbuff->length] = '\0';
        return c;
    }
    else
        return '\0';
}

int jsonp_strtod(strbuffer_t *strbuffer, double *out)
{
    double value;
    char *end;

    errno = 0;
    value = strtod(strbuffer->value, &end);
    assert(end == strbuffer->value + strbuffer->length);

    if((value == HUGE_VAL || value == -HUGE_VAL) && errno == ERANGE) {
        /* Overflow */
        return -1;
    }

    *out = value;
    return 0;
}

int jsonp_dtostr(char *buffer, size_t size, double value, int precision)
{
    int ret;
    char *start, *end;
    size_t length;

    if (precision == 0)
        precision = 17;

    ret = snprintf(buffer, size, "%.*g", precision, value);
    if(ret < 0)
        return -1;

    length = (size_t)ret;
    if(length >= size)
        return -1;

    /* Make sure there's a dot or 'e' in the output. Otherwise
       a real is converted to an integer when decoding */
    if(strchr(buffer, '.') == NULL &&
       strchr(buffer, 'e') == NULL)
    {
        if(length + 3 >= size) {
            /* No space to append ".0" */
            return -1;
        }
        buffer[length] = '.';
        buffer[length + 1] = '0';
        buffer[length + 2] = '\0';
        length += 2;
    }

    /* Remove leading '+' from positive exponent. Also remove leading
       zeros from exponents (added by some printf() implementations) */
    start = strchr(buffer, 'e');
    if(start) {
        start++;
        end = start + 1;

        if(*start == '-')
            start++;

        while(*end == '0')
            end++;

        if(end != start) {
            memmove(start, end, length - (size_t)(end - buffer));
            length -= (size_t)(end - start);
        }
    }

    return (int)length;
}
/*
 * Copyright (c) 2009-2016 Petri Lehtinen <petri@digip.org>
 *
 * Jansson is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See LICENSE for details.
 */

/* Work around nonstandard isnan() and isinf() implementations */
#ifndef isnan
#ifndef __sun
static inline int isnan(double x) { return x != x; }
#endif
#endif
#ifndef isinf
static inline int isinf(double x) { return !isnan(x) && isnan(x - x); }
#endif

static inline void json_init(json_t *json, json_type type)
{
    json->type = type;
    json->refcount = 1;
}

/*** object ***/

extern volatile uint32_t hashtable_seed;

json_t *json_object(void)
{
    json_object_t *object = jsonp_malloc(sizeof(json_object_t));
    if(!object)
        return NULL;

    if (!hashtable_seed) {
        /* Autoseed */
        json_object_seed(0);
    }

    json_init(&object->json, JSON_OBJECT);

    if(hashtable_init(&object->hashtable))
    {
        jsonp_free(object);
        return NULL;
    }

    object->visited = 0;

    return &object->json;
}

static void json_delete_object(json_object_t *object)
{
    hashtable_close(&object->hashtable);
    jsonp_free(object);
}

size_t json_object_size(const json_t *json)
{
    json_object_t *object;

    if(!json_is_object(json))
        return 0;

    object = json_to_object(json);
    return object->hashtable.size;
}

json_t *json_object_get(const json_t *json, const char *key)
{
    json_object_t *object;

    if(!key || !json_is_object(json))
        return NULL;

    object = json_to_object(json);
    return hashtable_get(&object->hashtable, key);
}

int json_object_set_new_nocheck(json_t *json, const char *key, json_t *value)
{
    json_object_t *object;

    if(!value)
        return -1;

    if(!key || !json_is_object(json) || json == value)
    {
        json_decref(value);
        return -1;
    }
    object = json_to_object(json);

    if(hashtable_set(&object->hashtable, key, value))
    {
        json_decref(value);
        return -1;
    }

    return 0;
}

int json_object_set_new(json_t *json, const char *key, json_t *value)
{
    if(!key || !utf8_check_string(key, strlen(key)))
    {
        json_decref(value);
        return -1;
    }

    return json_object_set_new_nocheck(json, key, value);
}

int json_object_del(json_t *json, const char *key)
{
    json_object_t *object;

    if(!key || !json_is_object(json))
        return -1;

    object = json_to_object(json);
    return hashtable_del(&object->hashtable, key);
}

int json_object_clear(json_t *json)
{
    json_object_t *object;

    if(!json_is_object(json))
        return -1;

    object = json_to_object(json);
    hashtable_clear(&object->hashtable);

    return 0;
}

int json_object_update(json_t *object, json_t *other)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    json_object_foreach(other, key, value) {
        if(json_object_set_nocheck(object, key, value))
            return -1;
    }

    return 0;
}

int json_object_update_existing(json_t *object, json_t *other)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    json_object_foreach(other, key, value) {
        if(json_object_get(object, key))
            json_object_set_nocheck(object, key, value);
    }

    return 0;
}

int json_object_update_missing(json_t *object, json_t *other)
{
    const char *key;
    json_t *value;

    if(!json_is_object(object) || !json_is_object(other))
        return -1;

    json_object_foreach(other, key, value) {
        if(!json_object_get(object, key))
            json_object_set_nocheck(object, key, value);
    }

    return 0;
}

void *json_object_iter(json_t *json)
{
    json_object_t *object;

    if(!json_is_object(json))
        return NULL;

    object = json_to_object(json);
    return hashtable_iter(&object->hashtable);
}

void *json_object_iter_at(json_t *json, const char *key)
{
    json_object_t *object;

    if(!key || !json_is_object(json))
        return NULL;

    object = json_to_object(json);
    return hashtable_iter_at(&object->hashtable, key);
}

void *json_object_iter_next(json_t *json, void *iter)
{
    json_object_t *object;

    if(!json_is_object(json) || iter == NULL)
        return NULL;

    object = json_to_object(json);
    return hashtable_iter_next(&object->hashtable, iter);
}

const char *json_object_iter_key(void *iter)
{
    if(!iter)
        return NULL;

    return hashtable_iter_key(iter);
}

json_t *json_object_iter_value(void *iter)
{
    if(!iter)
        return NULL;

    return (json_t *)hashtable_iter_value(iter);
}

int json_object_iter_set_new(json_t *json, void *iter, json_t *value)
{
    if(!json_is_object(json) || !iter || !value)
        return -1;

    hashtable_iter_set(iter, value);
    return 0;
}

void *json_object_key_to_iter(const char *key)
{
    if(!key)
        return NULL;

    return hashtable_key_to_iter(key);
}

static int json_object_equal(json_t *object1, json_t *object2)
{
    const char *key;
    json_t *value1, *value2;

    if(json_object_size(object1) != json_object_size(object2))
        return 0;

    json_object_foreach(object1, key, value1) {
        value2 = json_object_get(object2, key);

        if(!json_equal(value1, value2))
            return 0;
    }

    return 1;
}

static json_t *json_object_copy(json_t *object)
{
    json_t *result;

    const char *key;
    json_t *value;

    result = json_object();
    if(!result)
        return NULL;

    json_object_foreach(object, key, value)
        json_object_set_nocheck(result, key, value);

    return result;
}

static json_t *json_object_deep_copy(const json_t *object)
{
    json_t *result;
    void *iter;

    result = json_object();
    if(!result)
        return NULL;

    /* Cannot use json_object_foreach because object has to be cast
       non-const */
    iter = json_object_iter((json_t *)object);
    while(iter) {
        const char *key;
        const json_t *value;
        key = json_object_iter_key(iter);
        value = json_object_iter_value(iter);

        json_object_set_new_nocheck(result, key, json_deep_copy(value));
        iter = json_object_iter_next((json_t *)object, iter);
    }

    return result;
}

/*** array ***/

json_t *json_array(void)
{
    json_array_t *array = jsonp_malloc(sizeof(json_array_t));
    if(!array)
        return NULL;
    json_init(&array->json, JSON_ARRAY);

    array->entries = 0;
    array->size = 8;

    array->table = jsonp_malloc(array->size * sizeof(json_t *));
    if(!array->table) {
        jsonp_free(array);
        return NULL;
    }

    array->visited = 0;

    return &array->json;
}

static void json_delete_array(json_array_t *array)
{
    size_t i;

    for(i = 0; i < array->entries; i++)
        json_decref(array->table[i]);

    jsonp_free(array->table);
    jsonp_free(array);
}

size_t json_array_size(const json_t *json)
{
    if(!json_is_array(json))
        return 0;

    return json_to_array(json)->entries;
}

json_t *json_array_get(const json_t *json, size_t index)
{
    json_array_t *array;
    if(!json_is_array(json))
        return NULL;
    array = json_to_array(json);

    if(index >= array->entries)
        return NULL;

    return array->table[index];
}

int json_array_set_new(json_t *json, size_t index, json_t *value)
{
    json_array_t *array;

    if(!value)
        return -1;

    if(!json_is_array(json) || json == value)
    {
        json_decref(value);
        return -1;
    }
    array = json_to_array(json);

    if(index >= array->entries)
    {
        json_decref(value);
        return -1;
    }

    json_decref(array->table[index]);
    array->table[index] = value;

    return 0;
}

static void array_move(json_array_t *array, size_t dest,
                       size_t src, size_t count)
{
    memmove(&array->table[dest], &array->table[src], count * sizeof(json_t *));
}

static void array_copy(json_t **dest, size_t dpos,
                       json_t **src, size_t spos,
                       size_t count)
{
    memcpy(&dest[dpos], &src[spos], count * sizeof(json_t *));
}

static json_t **json_array_grow(json_array_t *array,
                                size_t amount,
                                int copy)
{
    size_t new_size;
    json_t **old_table, **new_table;

    if(array->entries + amount <= array->size)
        return array->table;

    old_table = array->table;

    new_size = max(array->size + amount, array->size * 2);
    new_table = jsonp_malloc(new_size * sizeof(json_t *));
    if(!new_table)
        return NULL;

    array->size = new_size;
    array->table = new_table;

    if(copy) {
        array_copy(array->table, 0, old_table, 0, array->entries);
        jsonp_free(old_table);
        return array->table;
    }

    return old_table;
}

int json_array_append_new(json_t *json, json_t *value)
{
    json_array_t *array;

    if(!value)
        return -1;

    if(!json_is_array(json) || json == value)
    {
        json_decref(value);
        return -1;
    }
    array = json_to_array(json);

    if(!json_array_grow(array, 1, 1)) {
        json_decref(value);
        return -1;
    }

    array->table[array->entries] = value;
    array->entries++;

    return 0;
}

int json_array_insert_new(json_t *json, size_t index, json_t *value)
{
    json_array_t *array;
    json_t **old_table;

    if(!value)
        return -1;

    if(!json_is_array(json) || json == value) {
        json_decref(value);
        return -1;
    }
    array = json_to_array(json);

    if(index > array->entries) {
        json_decref(value);
        return -1;
    }

    old_table = json_array_grow(array, 1, 0);
    if(!old_table) {
        json_decref(value);
        return -1;
    }

    if(old_table != array->table) {
        array_copy(array->table, 0, old_table, 0, index);
        array_copy(array->table, index + 1, old_table, index,
                   array->entries - index);
        jsonp_free(old_table);
    }
    else
        array_move(array, index + 1, index, array->entries - index);

    array->table[index] = value;
    array->entries++;

    return 0;
}

int json_array_remove(json_t *json, size_t index)
{
    json_array_t *array;

    if(!json_is_array(json))
        return -1;
    array = json_to_array(json);

    if(index >= array->entries)
        return -1;

    json_decref(array->table[index]);

    /* If we're removing the last element, nothing has to be moved */
    if(index < array->entries - 1)
        array_move(array, index, index + 1, array->entries - index - 1);

    array->entries--;

    return 0;
}

int json_array_clear(json_t *json)
{
    json_array_t *array;
    size_t i;

    if(!json_is_array(json))
        return -1;
    array = json_to_array(json);

    for(i = 0; i < array->entries; i++)
        json_decref(array->table[i]);

    array->entries = 0;
    return 0;
}

int json_array_extend(json_t *json, json_t *other_json)
{
    json_array_t *array, *other;
    size_t i;

    if(!json_is_array(json) || !json_is_array(other_json))
        return -1;
    array = json_to_array(json);
    other = json_to_array(other_json);

    if(!json_array_grow(array, other->entries, 1))
        return -1;

    for(i = 0; i < other->entries; i++)
        json_incref(other->table[i]);

    array_copy(array->table, array->entries, other->table, 0, other->entries);

    array->entries += other->entries;
    return 0;
}

static int json_array_equal(json_t *array1, json_t *array2)
{
    size_t i, size;

    size = json_array_size(array1);
    if(size != json_array_size(array2))
        return 0;

    for(i = 0; i < size; i++)
    {
        json_t *value1, *value2;

        value1 = json_array_get(array1, i);
        value2 = json_array_get(array2, i);

        if(!json_equal(value1, value2))
            return 0;
    }

    return 1;
}

static json_t *json_array_copy(json_t *array)
{
    json_t *result;
    size_t i;

    result = json_array();
    if(!result)
        return NULL;

    for(i = 0; i < json_array_size(array); i++)
        json_array_append(result, json_array_get(array, i));

    return result;
}

static json_t *json_array_deep_copy(const json_t *array)
{
    json_t *result;
    size_t i;

    result = json_array();
    if(!result)
        return NULL;

    for(i = 0; i < json_array_size(array); i++)
        json_array_append_new(result, json_deep_copy(json_array_get(array, i)));

    return result;
}

/*** string ***/

static json_t *string_create(const char *value, size_t len, int own)
{
    char *v;
    json_string_t *string;

    if(!value)
        return NULL;

    if(own)
        v = (char *)value;
    else {
        v = jsonp_strndup(value, len);
        if(!v)
            return NULL;
    }

    string = jsonp_malloc(sizeof(json_string_t));
    if(!string) {
        if(!own)
            jsonp_free(v);
        return NULL;
    }
    json_init(&string->json, JSON_STRING);
    string->value = v;
    string->length = len;

    return &string->json;
}

json_t *json_string_nocheck(const char *value)
{
    if(!value)
        return NULL;

    return string_create(value, strlen(value), 0);
}

json_t *json_stringn_nocheck(const char *value, size_t len)
{
    return string_create(value, len, 0);
}

/* this is private; "steal" is not a public API concept */
json_t *jsonp_stringn_nocheck_own(const char *value, size_t len)
{
    return string_create(value, len, 1);
}

json_t *json_string(const char *value)
{
    if(!value)
        return NULL;

    return json_stringn(value, strlen(value));
}

json_t *json_stringn(const char *value, size_t len)
{
    if(!value || !utf8_check_string(value, len))
        return NULL;

    return json_stringn_nocheck(value, len);
}

const char *json_string_value(const json_t *json)
{
    if(!json_is_string(json))
        return NULL;

    return json_to_string(json)->value;
}

size_t json_string_length(const json_t *json)
{
    if(!json_is_string(json))
        return 0;

    return json_to_string(json)->length;
}

int json_string_set_nocheck(json_t *json, const char *value)
{
    if(!value)
        return -1;

    return json_string_setn_nocheck(json, value, strlen(value));
}

int json_string_setn_nocheck(json_t *json, const char *value, size_t len)
{
    char *dup;
    json_string_t *string;

    if(!json_is_string(json) || !value)
        return -1;

    dup = jsonp_strndup(value, len);
    if(!dup)
        return -1;

    string = json_to_string(json);
    jsonp_free(string->value);
    string->value = dup;
    string->length = len;

    return 0;
}

int json_string_set(json_t *json, const char *value)
{
    if(!value)
        return -1;

    return json_string_setn(json, value, strlen(value));
}

int json_string_setn(json_t *json, const char *value, size_t len)
{
    if(!value || !utf8_check_string(value, len))
        return -1;

    return json_string_setn_nocheck(json, value, len);
}

static void json_delete_string(json_string_t *string)
{
    jsonp_free(string->value);
    jsonp_free(string);
}

static int json_string_equal(json_t *string1, json_t *string2)
{
    json_string_t *s1, *s2;

    if(!json_is_string(string1) || !json_is_string(string2))
        return 0;

    s1 = json_to_string(string1);
    s2 = json_to_string(string2);
    return s1->length == s2->length && !memcmp(s1->value, s2->value, s1->length);
}

static json_t *json_string_copy(const json_t *string)
{
    json_string_t *s;

    if(!json_is_string(string))
        return NULL;

    s = json_to_string(string);
    return json_stringn_nocheck(s->value, s->length);
}

/*** integer ***/

json_t *json_integer(json_int_t value)
{
    json_integer_t *integer = jsonp_malloc(sizeof(json_integer_t));
    if(!integer)
        return NULL;
    json_init(&integer->json, JSON_INTEGER);

    integer->value = value;
    return &integer->json;
}

json_int_t json_integer_value(const json_t *json)
{
    if(!json_is_integer(json))
        return 0;

    return json_to_integer(json)->value;
}

int json_integer_set(json_t *json, json_int_t value)
{
    if(!json_is_integer(json))
        return -1;

    json_to_integer(json)->value = value;

    return 0;
}

static void json_delete_integer(json_integer_t *integer)
{
    jsonp_free(integer);
}

static int json_integer_equal(json_t *integer1, json_t *integer2)
{
    return json_integer_value(integer1) == json_integer_value(integer2);
}

static json_t *json_integer_copy(const json_t *integer)
{
    return json_integer(json_integer_value(integer));
}

/*** real ***/

json_t *json_real(double value)
{
    json_real_t *real;

    if(isnan(value) || isinf(value))
        return NULL;

    real = jsonp_malloc(sizeof(json_real_t));
    if(!real)
        return NULL;
    json_init(&real->json, JSON_REAL);

    real->value = value;
    return &real->json;
}

double json_real_value(const json_t *json)
{
    if(!json_is_real(json))
        return 0;

    return json_to_real(json)->value;
}

int json_real_set(json_t *json, double value)
{
    if(!json_is_real(json) || isnan(value) || isinf(value))
        return -1;

    json_to_real(json)->value = value;

    return 0;
}

static void json_delete_real(json_real_t *real)
{
    jsonp_free(real);
}

static int json_real_equal(json_t *real1, json_t *real2)
{
    return json_real_value(real1) == json_real_value(real2);
}

static json_t *json_real_copy(const json_t *real)
{
    return json_real(json_real_value(real));
}

/*** number ***/

double json_number_value(const json_t *json)
{
    if(json_is_integer(json))
        return (double)json_integer_value(json);
    else if(json_is_real(json))
        return json_real_value(json);
    else
        return 0.0;
}

/*** simple values ***/

json_t *json_true(void)
{
    static json_t the_true = {JSON_TRUE, (size_t)-1};
    return &the_true;
}

json_t *json_false(void)
{
    static json_t the_false = {JSON_FALSE, (size_t)-1};
    return &the_false;
}

json_t *json_null(void)
{
    static json_t the_null = {JSON_NULL, (size_t)-1};
    return &the_null;
}

/*** deletion ***/

void json_delete(json_t *json)
{
    if (!json)
        return;

    switch(json_typeof(json)) {
        case JSON_OBJECT:
            json_delete_object(json_to_object(json));
            break;
        case JSON_ARRAY:
            json_delete_array(json_to_array(json));
            break;
        case JSON_STRING:
            json_delete_string(json_to_string(json));
            break;
        case JSON_INTEGER:
            json_delete_integer(json_to_integer(json));
            break;
        case JSON_REAL:
            json_delete_real(json_to_real(json));
            break;
        default:
            return;
    }

    /* json_delete is not called for true, false or null */
}

/*** equality ***/

int json_equal(json_t *json1, json_t *json2)
{
    if(!json1 || !json2)
        return 0;

    if(json_typeof(json1) != json_typeof(json2))
        return 0;

    /* this covers true, false and null as they are singletons */
    if(json1 == json2)
        return 1;

    switch(json_typeof(json1)) {
        case JSON_OBJECT:
            return json_object_equal(json1, json2);
        case JSON_ARRAY:
            return json_array_equal(json1, json2);
        case JSON_STRING:
            return json_string_equal(json1, json2);
        case JSON_INTEGER:
            return json_integer_equal(json1, json2);
        case JSON_REAL:
            return json_real_equal(json1, json2);
        default:
            return 0;
    }
}

/*** copying ***/

json_t *json_copy(json_t *json)
{
    if(!json)
        return NULL;

    switch(json_typeof(json)) {
        case JSON_OBJECT:
            return json_object_copy(json);
        case JSON_ARRAY:
            return json_array_copy(json);
        case JSON_STRING:
            return json_string_copy(json);
        case JSON_INTEGER:
            return json_integer_copy(json);
        case JSON_REAL:
            return json_real_copy(json);
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return json;
        default:
            return NULL;
    }

    return NULL;
}

json_t *json_deep_copy(const json_t *json)
{
    if(!json)
        return NULL;

    switch(json_typeof(json)) {
        case JSON_OBJECT:
            return json_object_deep_copy(json);
        case JSON_ARRAY:
            return json_array_deep_copy(json);
            /* for the rest of the types, deep copying doesn't differ from
               shallow copying */
        case JSON_STRING:
            return json_string_copy(json);
        case JSON_INTEGER:
            return json_integer_copy(json);
        case JSON_REAL:
            return json_real_copy(json);
        case JSON_TRUE:
        case JSON_FALSE:
        case JSON_NULL:
            return (json_t *)json;
        default:
            return NULL;
    }

    return NULL;
}
