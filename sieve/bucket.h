/*
 * Some bucket stuff.
 */

#include <stdlib.h>   // for malloc and friends
#include <stdint.h>

//#define SAFE_BUCKETS

/*
 * This bucket module provides a way to store elements (that are called
 * updates), while partially sorting them, according to some criterion (to
 * be defined externally): the incoming data is stored into several
 * buckets. The user says for each data to which bucket it belongs. This
 * module is supposed to perform this storage in a cache-friendly way and
 * so on...
 * Updates can be "tagged" with a one-byte value which must be increasing
 * across all the updates stored.
 */

/*
 * Main available commands are 
 *   push_bucket_update(i, x)  :   add x to bucket number i
 *   get_next_bucket_update(i) :   iterator to read contents of bucket number i
 *
 * See the MAIN FUNCTIONS section below for the complete interface, with
 * prototypes of exported functions.
 */

/********  Data structure for the contents of buckets **************/

/* In principle, the typedef for the bucket_update can be changed without
 * affecting the rest of the code. 
 */

/*
 * For the moment, we will keep the bucket updates aligned by adding an
 * 8-bit field that can contain, for instance, the low bits of p.
 *
 * TODO:
 * If the memory pressure becomes too high with this, we can remove this
 * p_low field and pack the updates as follows:
 *    [ x0 ] [ x1 ] [ x2 ] [ x3 ] [logp0] [logp1] [logp2] [logp3]
 * in the bucket.
 *
 * This will be slightly more tricky to store/load bucket updates, but
 * the additional cost should be negligible.
 */

typedef struct {
    uint16_t x;
    char logp;
    // uint8_t p_low;  // keeping alignment seems to slow down the code.
    uint32_t p;   // TODO: Is 32 better than 64, here ?????
} bucket_update_t;


/*
 * will be used as a sentinel
 */
static const bucket_update_t LAST_UPDATE = {0,0,0};

/******** Bucket array typedef **************/

/*
 * A bucket is just an array of bucket_update_t.
 * We are going to manipulate several of them simultaneously when
 * sieving. In order to reduce the cache pressure, we are going to
 * introduce another structure that handles an array of bucket
 * efficiently.
 *
 * When doing a (long) sequence of pushes, only the bucket_start array is
 * used: pointer to it should be in a register, and the set of pointer
 * will probably fit in one page-size (please align!), so that the pressure
 * on the TLB is minimal. The other parts should not really be used
 * during this memory intensive part.
 * (except if one want to check for overflow, which can probably be
 * avoided by having large enough bucket_size).
 */

typedef struct {
    bucket_update_t ** bucket_start;    // Contains pointers to begining of
                                        // buckets.
    bucket_update_t ** bucket_write;    // Contains pointers to first empty
                                        // location in each bucket.
    bucket_update_t ** bucket_read;     // Contains pointers to first unread
                                        // location in each bucket.
    bucket_update_t *** first_logp;     // For each bucket, an array of 
                                        // pointers to the first bucket entry
                                        // with a given log_p
    int bucket_size;                    // The allocated size of one bucket.
    int n_bucket;                       // Number of buckets.
    unsigned char last_logp;
} bucket_array_t;

/*
 * Notes for future improvements:
 *
 * 1) Double buckets.
 * If we can not afford enough buckets (limited by TLB) and we don't want
 * to make the bucket_region larger than L1, we can have a double bucket
 * system: A first level of buckets, that correspond to very large
 * bucket_regions, and a second level of bucket regions fitting in L1.
 * We collect all updates in the buckets of the first level, then we
 * process one first level bucket at a time, copying its updates to the
 * corresponding second level buckets and processing each second level
 * bucket by applying its updates to the L1 bucket_regions.
 *
 * 2) Buffered buckets.
 * One can have a buffer that contains one cache-line for each bucket.
 * Once a cache-line is full of updates, it can be moved in a
 * write-combined (non-temporal) way to the memory location of the
 * bucket. Details are left to the reader ;-)
 *
 */


/******** MAIN FUNCTIONS **************/


/* Returns an allocated array of <n_bucket> buckets each having size
 * <bucket_size>. Must be freed with clear_bucket_array().
 * It also put pointers in position ready for read/writes.
 */
static inline bucket_array_t
init_bucket_array(const int n_bucket, const int bucket_size);

static inline void
clear_bucket_array(bucket_array_t BA);

/* Main writing function: appends update to bucket number i.
 * If SAFE_BUCKETS is not #defined, then there is no checking that there is
 * enough room for the update. This could lead to a segfault, with the
 * current implementation!
 */
static inline void
push_bucket_update(bucket_array_t BA, const int i, 
                   const bucket_update_t update);

/* Main reading iterator: returns the first unread update in bucket i.
 * If SAFE_BUCKETS is not #defined, there is no checking that you are reading
 * at most as much as what you wrote. If keeping the count while pushing
 * is impossible, the following functions can help:
 * - get the count after the last push using nb_of_updates()
 * - put a sentinel after your bucket using push_sentinel() and check if
 * the value returned by get_next_bucket_update() is LAST_UPDATE.
 * - call the is_end() functions that tells you if you can apply
 *   get_next_bucket_update().
 */
static inline bucket_update_t
get_next_bucket_update(bucket_array_t BA, const int i);
static inline int
nb_of_updates(const bucket_array_t BA, const int i);
static inline void
push_sentinel(bucket_array_t BA, const int i);
static inline int
is_end(const bucket_array_t BA, const int i);

/* If you need to read twice a bucket, you can rewind it: */
static inline void
rewind_bucket(bucket_array_t BA, const int i);

/* If you want to read the most recently read update again, rewind by 1: */
static inline void
rewind_bucket_by_1 (bucket_array_t BA, const int i);

/* If you want to access updates in a non-sequential way: */
static inline bucket_update_t
get_kth_bucket_update(const bucket_array_t BA, const int i, const int k);

/* Call this to mark the current write position in the buckets the start
   of the log(p) value logp. I.e. for each bucket number i, 
   the updates at addresses >= BA.first_logp[i][logp], 
   < BA.first_logp[i][logp+1] correspond to primes p with log(p) = logp.
   The logp can be set only incrementally, calling with a logp that is not
   larger than the largest previously used one has no effect. */
static inline void
bucket_new_logp(bucket_array_t BA, const unsigned char logp);

/******** Bucket array implementation **************/

#include "utils/misc.h"

static inline bucket_array_t
init_bucket_array(const int n_bucket, const int bucket_size)
{
    bucket_array_t BA;
    int i;
    BA.bucket_size = bucket_size;
    BA.n_bucket = n_bucket;

    BA.bucket_start = (bucket_update_t **)malloc_pagealigned(n_bucket*sizeof(bucket_update_t *));
    BA.bucket_write = (bucket_update_t **)malloc_check(n_bucket*sizeof(bucket_update_t *));
    BA.bucket_read  = (bucket_update_t **)malloc_check(n_bucket*sizeof(bucket_update_t *));
    BA.first_logp = (bucket_update_t ***)malloc_check(n_bucket*sizeof(bucket_update_t **));

    for (i = 0; i < n_bucket; ++i) {
        // TODO: shall we ensure here that those pointer do not differ by
        // a large power of 2, to take into account the associativity of
        // L1 cache ?
        // TODO: would it be better to have a single big malloc for all
        // the bucket_start[i] ?
        BA.bucket_start[i] = (bucket_update_t *)malloc_check(bucket_size*sizeof(bucket_update_t));
        BA.bucket_write[i] = BA.bucket_start[i];
        BA.bucket_read[i] = BA.bucket_start[i];
	BA.first_logp[i] = (bucket_update_t **)malloc_check(256 * sizeof(bucket_update_t *));
	BA.first_logp[i][0] = BA.bucket_write[i];
    }
    BA.last_logp = 0;
    return BA;
}

static inline void
clear_bucket_array(bucket_array_t BA)
{
    int i;
    for (i = 0; i < BA.n_bucket; ++i)
      {
        free(BA.bucket_start[i]);
	free(BA.first_logp[i]);
      }
    free_pagealigned(BA.bucket_start, BA.n_bucket*sizeof(bucket_update_t *));
    free(BA.bucket_write);
    free(BA.bucket_read);
    free(BA.first_logp);
}

static inline void
push_bucket_update(bucket_array_t BA, const int i, 
                   const bucket_update_t update)
{
    *(BA.bucket_write[i])++ = update; /* Pretty! */
#ifdef SAFE_BUCKETS
    if (BA.bucket_start[i] + BA.bucket_size <= BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: hit end of bucket nb %d\n", i);
        BA.bucket_write[i]--;
    }
#endif
}

static inline void
bucket_new_logp(bucket_array_t BA, const unsigned char logp)
{
  int i, j;
  for (i = 0; i < BA.n_bucket; ++i)
    for (j = (int) BA.last_logp + 1; j <= (int) logp; j++)
      BA.first_logp[i][j] = BA.bucket_write[i];
  if (BA.last_logp < logp)
    BA.last_logp = logp;
}

static inline void
rewind_bucket(bucket_array_t BA, const int i)
{
    BA.bucket_read[i] = BA.bucket_start[i];
}

static inline void
rewind_bucket_by_1 (bucket_array_t BA, const int i)
{
  if (BA.bucket_read[i] > BA.bucket_start[i])
    BA.bucket_read[i]--;
}

static inline bucket_update_t
get_next_bucket_update(bucket_array_t BA, const int i)
{
    bucket_update_t rep = *(BA.bucket_read[i])++;
#ifdef SAFE_BUCKETS
    if (BA.bucket_read[i] > BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: reading too many updates in bucket nb %d\n", i);
        BA.bucket_read[i]--;
        return LAST_UPDATE;
    }
#endif
    return rep;
}

static inline bucket_update_t
get_kth_bucket_update(const bucket_array_t BA, const int i, const int k)
{
    bucket_update_t rep = (BA.bucket_start[i])[k];
#ifdef SAFE_BUCKETS
    if (BA.bucket_start[i] + k >= BA.bucket_write[i]) {
        fprintf(stderr, "# Warning: reading outside valid updates in bucket nb %d\n", i);
        return LAST_UPDATE;
    }
#endif
    return rep;
}

static inline int
nb_of_updates(const bucket_array_t BA, const int i)
{
    return (BA.bucket_write[i] - BA.bucket_start[i]);
}

static inline void
push_sentinel(bucket_array_t BA, const int i)
{
    push_bucket_update(BA, i, LAST_UPDATE);
}

static inline int
is_end(const bucket_array_t BA, const int i)
{
    return (BA.bucket_read[i] == BA.bucket_write[i]);
}

/* Delete the updates whose values of x do not yield a report.
   This will speed up the trial division, since we will loop
   over less entries. */
static void
purge_bucket (bucket_array_t BA, const int i, unsigned char *S)
{
  bucket_update_t *u, *v;

  for (u = v = BA.bucket_start[i]; u < BA.bucket_write[i]; u++)
    if (S[u->x] != 255)
      *v++ = *u;
  BA.bucket_write[i] = v;
}

/* A compare function suitable for sorting updates in order of ascending x
   with qsort() */
static int
bucket_cmp_update (const bucket_update_t *a, const bucket_update_t *b)
{
  if (a->x < b->x)
    return -1;
  if (a->x == b->x)
    return 0;
  return 1;
}

static void
bucket_sortbucket (bucket_array_t BA, const int i)
{
  qsort (BA.bucket_start[i], nb_of_updates (BA, i), sizeof (bucket_update_t), 
	 (int(*)(const void *, const void *)) &bucket_cmp_update);
}
