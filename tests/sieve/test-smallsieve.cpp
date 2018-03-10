#include "cado.h"
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <gmp.h>
#include <time.h>
#include <vector>
#include <sstream>
#include <algorithm>
#include <x86intrin.h>
#include "utils.h"
#include "macros.h"

#define xxxLOG_BUCKET_REGION_IS_A_CONSTANT

#include "test-smallsieve-mock.hpp"

#include "sieve/las-smallsieve-lowlevel.hpp"
#include "sieve/las-smallsieve-glue.hpp"


int consistency_check_mode = 0;
int nthreads = 1;   /* number of threads */
int quiet = 0;
int abort_on_fail = 0;
int only_complete_functions = 0;

/* this is really a mock structure just for the fun of it. */
struct {
    struct {
        sublat_t sublat;
        int logI_adjusted;
    } conf;
    struct {
        mpz_t q;
    } qbasis;
} si;




/******************************************************************/
/* now provides routines for testing */

/* we create lists of functions, because from all these tidbits we need
 * to generate the more complete sieving functions below.
 */
struct all_generic_candidates {
    typedef
            list_car<manual_oldloop,
            list_car<manual_oldloop_nounroll,
            list_nil>> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly_generic_loop8,
            list_car<assembly_generic_loop12,
            list_car<assembly_generic_loop16,
            list_car<assembly_generic_loop8p0,
            list_car<assembly_generic_loop12p0,
            list_car<assembly_generic_loop16p0,
            list_car<assembly_generic_loop8p1,
            list_car<assembly_generic_loop12p1,
            list_car<assembly_generic_loop16p1,
            list_car<assembly_generic_loop12p2,
            list_car<assembly_generic_oldloop,
            type0> >>> >>> >>> > type;
#else
    typedef type0 type;
#endif
};

template<int bit> struct all_candidates_for_evenline {
    typedef all_generic_candidates::type type;
};
template<int bit> struct all_candidates_for_oddline {
    typedef all_generic_candidates::type type;
};
template<> struct all_candidates_for_evenline<0> {
    typedef list_car<manual0, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly0, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<1> {
    typedef list_car<manual0, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly0, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<2> {
    typedef list_car<manual1, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly1, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<3> {
    typedef list_car<manual2, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly2, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<4> {
    typedef list_car<manual3, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly3, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<5> {
    typedef list_car<manual4, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly4, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<6> {
    typedef all_generic_candidates::type type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly5, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_evenline<7> {
    typedef all_generic_candidates::type type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly6, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<0> {
    typedef list_car<manual0, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly0, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<1> {
    typedef list_car<manual1, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly1, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<2> {
    typedef list_car<manual2, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly2, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<3> {
    typedef list_car<manual3, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly3, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<4> {
    typedef list_car<manual4, all_generic_candidates::type> type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly4, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<5> {
    typedef all_generic_candidates::type type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly5, type0> type;
#else
    typedef type0 type;
#endif
};
template<> struct all_candidates_for_oddline<6> {
    typedef all_generic_candidates::type type0;
#ifdef HAVE_GCC_STYLE_AMD64_INLINE_ASM
    typedef list_car<assembly6, type0> type;
#else
    typedef type0 type;
#endif
};

/* For 2^(I-k) <= p < 2^(I-k+1), we have: 2^k >= 2^I/p > 2^(k-1), and
 *
 * so that the number of intervals of width p that fit within 2^I,
 * which is (2^I-1)/p, is between:
 *
 * 2^k > (2^I-1)/p >= 2^(k-1)
 *
 * The number of hits (endpoints of the intervals) is thus <= 2^k.
 *
 * For a lower bound, 2^(k-1) is attained because of the offset.
 *
 * Note that on even lines, p behaves as if it was doubled.
 */

static inline size_t sieve_full_line(unsigned char * S0, unsigned char * S1, size_t x0 MAYBE_UNUSED, size_t pos, size_t p_or_2p, unsigned char logp, where_am_I w MAYBE_UNUSED)
{
    unsigned char * pi = S0 + pos;
    SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S1, logp);
    return pi - S1;
}
static inline size_t sieve_full_line_new_half(unsigned char * S0, unsigned char * S1, size_t x0 MAYBE_UNUSED, size_t pos, size_t p_or_2p, unsigned char logp, where_am_I w MAYBE_UNUSED)
{
    unsigned char * pi = S0 + pos;
    SMALLSIEVE_ASSEMBLY_NEW_LOOP12(pi, p_or_2p, S1, logp);
    return pi - S1;
}
static inline size_t sieve_full_line_new(unsigned char * S0, unsigned char * S1, size_t x0 MAYBE_UNUSED, size_t pos, size_t p_or_2p, unsigned char logp, where_am_I w MAYBE_UNUSED)
{
    unsigned char * pi = S0 + pos;
    SMALLSIEVE_ASSEMBLY_NEW_LOOP12(pi, p_or_2p, S1, logp);
    return pi - S1;
}

void current_I18_branch(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /* {{{ */
{
    SMALLSIEVE_COMMON_DEFS();
    ASSERT_ALWAYS(positions.size() == primes.size());
    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;
        unsigned char * S0 = S;

        size_t overrun = 0; /* tame gcc */
        unsigned int i_compens_sublat = si.conf.sublat.i0 & 1;
        unsigned int j = j0;


        /* we sieve over the area [S0..S0+(i1-i0)], which may
         * actually be just a fragment of a line. After that, if
         * (i1-i0) is different from I, we'll break anyway. So
         * whether we add I or (i1-i0) to S0 does not matter much.
         */
        if (j & 1) {
            WHERE_AM_I_UPDATE(w, j, j - j0);
            overrun = sieve_full_line(S0, S0 + (i1 - i0), S0 - S,
                    pos, p, logp, w);
            S0 += I;
            j++;
            pos += r; if (pos >= (int) p) pos -= p;
        }
        for( ; j < j1 ; ) {
            /* for j even, we sieve only odd pi, so step = 2p. */
            int xpos = ((i_compens_sublat + pos) & 1) ? pos : (pos+p);
            overrun = sieve_full_line(S0, S0 + (i1 - i0), S0 - S,
                    xpos, p+p, logp, w);
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p; 
            if (++j >= j1) break;

            /* now j odd again */
            WHERE_AM_I_UPDATE(w, j, j - j0);
            overrun = sieve_full_line(S0, S0 + (i1 - i0), S0 - S,
                    pos, p, logp, w);
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p;
            ++j;
        }
        if (logI > LOG_BUCKET_REGION) {
            /* quick notes for incremental adjustment in case I>B (B =
             * LOG_BUCKET_REGION).
             *
             * Let q = 2^(I-B).
             * Let N = a*q+b, and N'=N+nthreads=a'*q+b' ; N' is the
             * next bucket region we'll handle.
             *
             * Let nthreads = u*q+v
             *
             * The row increase is dj = (N' div q) - (N div q) = a'-a
             * The fragment increase is di = (N' mod q) - (N mod q) = b'-b
             *
             * Of course we have -q < b'-b < q
             *

             * dj can be written as (N'-N-(b'-b)) div q, which is an
             * exact division. we rewrite that as:
             *
             * dj = u + (v - (b'-b)) div q
             *
             * where the division is again exact. Now (v-(b'-b))
             * satisfies:
             * -q < v-(b'-b) < 2*q-1
             *
             * so that the quotient may only be 0 or 1.
             *
             * It is 1 if and only if v >= q + b'-b, which sounds like a
             * reasonable thing to check.
             */
            int N1 = N + nthreads;
            int Q = logI - LOG_BUCKET_REGION;
            int dj = (N1>>Q) - j0;
            int di = (N1&((1<<Q)-1)) - (N&((1<<Q)-1));
            /* Note that B_mod p is not reduced. It may be <0, and
             * may also be >= p if we sieved with 2p because of even j
             */
    ASSERT(overrun < (int) 2*p);
    ASSERT(p_pos < (int) p);
            int B_mod_p = overrun - p_pos;
            /* We may avoid some of the cost for the modular
             * reduction, here:
             *
             * dj is either always the same thing, or that same thing
             * + 1.
             *
             * di is within a small interval (albeit a centered one).
             * 
             * So it seems feasible to get by with a fixed number of
             * conditional subtractions.
             */
            pos = (p_pos + B_mod_p * di + dj * (int) r) % (int) p;
            if (pos < 0) pos += p;
        } else {
            /* skip stride */
            pos += ssp.get_offset();
            if (pos >= (int) p) pos -= p;
        }

        p_pos = pos;
    }
}/*}}}*/
void modified_I18_branch_C(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /* {{{ */
{
    SMALLSIEVE_COMMON_DEFS();
    ASSERT_ALWAYS(positions.size() == primes.size());
    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;
        unsigned char * S0 = S;

        size_t overrun = 0; /* tame gcc */

        /* we sieve over the area [S0..S0+(i1-i0)], which may
         * actually be just a fragment of a line. After that, if
         * (i1-i0) is different from I, we'll break anyway. So
         * whether we add I or (i1-i0) to S0 does not matter much.
         */
        fbprime_t spx = (p+p)^p;
        fbprime_t px = (j0&1)?p:(p+p);
        /* The increment is i1-i0, not I, to cover the case where 
         * B <= I; we then have i1-i0 = B
         * because there's a min. (and in that case, there's only a
         * single line).
         */
        for(unsigned int j = j0 ; j < j1 ; ++j) {
            /* for j even, we sieve only odd pi, so step = 2p. */
            WHERE_AM_I_UPDATE(w, j, j);
            overrun = sieve_full_line(S0, S0 + (i1 - i0), S0 - S,
                    pos + (((si.conf.sublat.i0^pos^1)&1&~j)?p:0), px, logp, w);
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p; 
            px ^= spx;
        }
        if (logI > LOG_BUCKET_REGION) {
            int N1 = N + nthreads;
            int Q = logI - LOG_BUCKET_REGION;
            int dj = (N1>>Q) - j0;
            int di = (N1&((1<<Q)-1)) - (N&((1<<Q)-1));
    ASSERT(overrun < (int) 2*p);
    ASSERT(p_pos < (int) p);
            int B_mod_p = overrun - p_pos;
            pos = (p_pos + B_mod_p * di + dj * (int) r) % (int) p;
            if (pos < 0) pos += p;
        } else {
            pos += ssp.get_offset();
            if (pos >= (int) p) pos -= p;
        }

        p_pos = pos;
    }
}/*}}}*/
void legacy_branch(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /*{{{*/
{
    SMALLSIEVE_COMMON_DEFS();
    const unsigned long bucket_region = (1UL << LOG_BUCKET_REGION);
    ASSERT_ALWAYS(positions.size() == primes.size());
    ASSERT_ALWAYS(logI <= LOG_BUCKET_REGION);

    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        unsigned int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;

        unsigned long j;
        const int test_divisibility MAYBE_UNUSED = 0; /* very slow, but nice for debugging */
        const unsigned long nj = bucket_region >> si.conf.logI_adjusted; /* Nr. of lines 
                                                                            per bucket region */

        WHERE_AM_I_UPDATE(w, p, p);

        //XXX XXX XXX const unsigned char logp = ssd->logp[k];
        unsigned char *S_ptr = S;
        size_t p_or_2p = p;
        ASSERT(pos < p);
        unsigned int i_compens_sublat = 0;
        if (si.conf.sublat.m != 0) {
            i_compens_sublat = si.conf.sublat.i0 & 1;
        }
        j = 0;
        if (row0_is_oddj) goto j_odd;
        // it is possible to make this code work with sieve_full_line by
        // uncommenting all lines that match S0. Performance is
        // apparently identical, but it's not fair to say that this *is*
        // the reference code, as far as performance is concerned.
        // unsigned char * S0 MAYBE_UNUSED;
        do {
            unsigned char *pi;
            WHERE_AM_I_UPDATE(w, j, j);
            // S0 = S_ptr;
            pi = S_ptr + pos;
            S_ptr += I;
            /* for j even, we sieve only odd pi, so step = 2p. */
            p_or_2p += p_or_2p;
            if (!((i_compens_sublat + pos) & 1)) {
                pi += p;
            }
            SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S_ptr, logp);
            // sieve_full_line(S0, S_ptr, S0 - S, pi - S0, p_or_2p, logp, w);
            pos += r; if (pos >= p) pos -= p; 
            /* Next line */
            if (++j >= nj) break;
            p_or_2p >>= 1;
j_odd:
            WHERE_AM_I_UPDATE(w, j, j);
            // S0 = S_ptr;
            pi = S_ptr + pos;
            S_ptr += I;
            SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S_ptr, logp);
            // sieve_full_line(S0, S_ptr, S0 - S, pi - S0, p_or_2p, logp, w);
            pos += r; if (pos >= p) pos -= p;
        } while (++j < nj);

        //XXX XXX XXX XXX adding this here, as it is relevant to the comparison
        //with branch I18.
        pos += ssp.get_offset();
        if (pos >= (unsigned int) p)
            pos -= p;
        //XXX XXX XXX XXX

        p_pos = pos;
    }
}
/*}}}*/
void devel_branch(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /*{{{*/
{
    SMALLSIEVE_COMMON_DEFS();
    ASSERT_ALWAYS(positions.size() == primes.size());
    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;
        unsigned char * S0 = S;

        size_t overrun = 0; /* tame gcc */
        unsigned int j = j0;

        /* we sieve over the area [S0..S0+(i1-i0)], which may
         * actually be just a fragment of a line. After that, if
         * (i1-i0) is different from I, we'll break anyway. So
         * whether we add I or (i1-i0) to S0 does not matter much.
         */
        /* TODO: sublat !!!! j actually corresponds to row S(j), with
         * S(j)=sublatm*j + sublatj0. Therefore:
         *  - j0&1 == 0 is not sufficient to tell the parity of S(j)
         *  - if sublatm is even, we not every other line is even (resp.
         *    odd).
         *  - if sublatm is even, we should actually take advantage of
         *    it, and run specific code, probably at the outer loop
         *    level.
         */
        if (j0 & 1)
            goto j_odd_devel;

        for( ; j < j1 ; ) {
            /* for S(j) even, we sieve only odd pi, so step = 2p. */
            {
            int xpos = ((si.conf.sublat.i0 + pos) & 1) ? pos : (pos+p);
            overrun = sieve_full_line_new_half(S0, S0 + (i1 - i0), S0 - S,
                    xpos, p+p, logp, w);
            }
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p; 
            if (++j >= j1) break;

j_odd_devel:
            /* now S(j) odd again */
            WHERE_AM_I_UPDATE(w, j, j - j0);
            overrun = sieve_full_line_new(S0, S0 + (i1 - i0), S0 - S,
                    pos, p, logp, w);
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p;
            ++j;
        }
        if (logI > LOG_BUCKET_REGION) {
            /* quick notes for incremental adjustment in case I>B (B =
             * LOG_BUCKET_REGION).
             *
             * Let q = 2^(I-B).
             * Let N = a*q+b, and N'=N+nthreads=a'*q+b' ; N' is the
             * next bucket region we'll handle.
             *
             * Let nthreads = u*q+v
             *
             * The row increase is dj = (N' div q) - (N div q) = a'-a
             * The fragment increase is di = (N' mod q) - (N mod q) = b'-b
             *
             * Of course we have -q < b'-b < q
             *

             * dj can be written as (N'-N-(b'-b)) div q, which is an
             * exact division. we rewrite that as:
             *
             * dj = u + (v - (b'-b)) div q
             *
             * where the division is again exact. Now (v-(b'-b))
             * satisfies:
             * -q < v-(b'-b) < 2*q-1
             *
             * so that the quotient may only be 0 or 1.
             *
             * It is 1 if and only if v >= q + b'-b, which sounds like a
             * reasonable thing to check.
             */
            int N1 = N + nthreads;
            int Q = logI - LOG_BUCKET_REGION;
            int dj = (N1>>Q) - j0;
            int di = (N1&((1<<Q)-1)) - (N&((1<<Q)-1));
            /* Note that B_mod p is not reduced. It may be <0, and
             * may also be >= p if we sieved with 2p because of even j
             */
    ASSERT(overrun < (int) 2*p);
    ASSERT(p_pos < (int) p);
            int B_mod_p = overrun - p_pos;
            /* FIXME: we may avoid some of the cost for the modular
             * reduction, here. Having a mod operation in this place
             * seems to be a fairly terrible idea.
             *
             * dj is either always the same thing, or that same thing +1.
             * di is within a small interval (albeit a centered one).
             * 
             * It seems feasible to get by with a fixed number of
             * conditional subtractions.
             */
            pos = (p_pos + B_mod_p * di + dj * (int) r) % (int) p;
            if (pos < 0) pos += p;
        } else {
            /* skip stride */
            pos += ssp.get_offset();
            if (pos >= (int) p) pos -= p;
        }

        p_pos = pos;
    }
}
/*}}}*/
void legacy_mod_branch(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /*{{{*/
{
    SMALLSIEVE_COMMON_DEFS();
    const unsigned long bucket_region = (1UL << LOG_BUCKET_REGION);
    ASSERT_ALWAYS(positions.size() == primes.size());

    if (logI > LOG_BUCKET_REGION) {
    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;

        size_t overrun MAYBE_UNUSED = 0; /* tame gcc */
        unsigned long j;
        const int test_divisibility MAYBE_UNUSED = 0; /* very slow, but nice for debugging */
        const unsigned long nj = bucket_region >> si.conf.logI_adjusted; /* Nr. of lines 
                                                                            per bucket region */

        WHERE_AM_I_UPDATE(w, p, p);

        //XXX XXX XXX const unsigned char logp = ssd->logp[k];
        unsigned char *S_ptr = S;
        unsigned char *S1;
        size_t p_or_2p = p;
        ASSERT(pos < (int) p);
        unsigned int i_compens_sublat = 0;
        if (si.conf.sublat.m != 0) {
            i_compens_sublat = si.conf.sublat.i0 & 1;
        }
        j = 0;
            if (row0_is_oddj) goto j_odd0;
        // it is possible to make this code work with sieve_full_line by
        // uncommenting all lines that match S0. Performance is
        // apparently identical, but it's not fair to say that this *is*
        // the reference code, as far as performance is concerned.
        // unsigned char * S0 MAYBE_UNUSED;
        do {
            unsigned char *pi;
            WHERE_AM_I_UPDATE(w, j, j);
            // S0 = S_ptr;
            pi = S_ptr + pos;
            S1 = S_ptr + (i1 - i0);
            S_ptr += I;
            /* for j even, we sieve only odd pi, so step = 2p. */
            p_or_2p += p_or_2p;
            if (!((i_compens_sublat + pos) & 1)) {
                pi += p;
            }
            SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S1, logp);
            overrun = pi - S1;
            // sieve_full_line(S0, S_ptr, S0 - S, pi - S0, p_or_2p, logp, w);
            pos += r; if (pos >= (int) p) pos -= p; 
            /* Next line */
            if (++j >= nj) break;
            p_or_2p >>= 1;
j_odd0:
            WHERE_AM_I_UPDATE(w, j, j);
            // S0 = S_ptr;
            pi = S_ptr + pos;
            S1 = S_ptr + (i1 - i0);
            S_ptr += I;
            SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S1, logp);
            overrun = pi - S1;
            pos += r; if (pos >= (int) p) pos -= p;
        } while (++j < nj);

            int N1 = N + nthreads;
            int Q = logI - LOG_BUCKET_REGION;
            int dj = (N1>>Q) - j0;
            int di = (N1&((1<<Q)-1)) - (N&((1<<Q)-1));
    ASSERT(overrun < (int) 2*p);
    ASSERT(p_pos < (int) p);
            int B_mod_p = overrun - p_pos;
            pos = (p_pos + B_mod_p * di + dj * (int) r) % (int) p;
            if (pos < 0) pos += p;

            p_pos = pos;
        }
        } else {
        for(auto const & ssp : primes) {
            int & p_pos(positions[&ssp - &primes.front()]);
            int pos = p_pos;

            const fbprime_t p = ssp.get_p();
            const fbprime_t r = ssp.get_r();
            const unsigned char logp = ssp.logp;

            unsigned long j;
            const int test_divisibility MAYBE_UNUSED = 0; /* very slow, but nice for debugging */
            const unsigned long nj = bucket_region >> si.conf.logI_adjusted; /* Nr. of lines 
                                                                                per bucket region */

            WHERE_AM_I_UPDATE(w, p, p);

            //XXX XXX XXX const unsigned char logp = ssd->logp[k];
            unsigned char *S_ptr = S;
            size_t p_or_2p = p;
            ASSERT(pos < (int) p);
            unsigned int i_compens_sublat = 0;
            if (si.conf.sublat.m != 0) {
                i_compens_sublat = si.conf.sublat.i0 & 1;
            }
            j = 0;
            if (row0_is_oddj) goto j_odd;
            // it is possible to make this code work with sieve_full_line by
            // uncommenting all lines that match S0. Performance is
            // apparently identical, but it's not fair to say that this *is*
            // the reference code, as far as performance is concerned.
            // unsigned char * S0 MAYBE_UNUSED;
            do {
                unsigned char *pi;
                WHERE_AM_I_UPDATE(w, j, j);
                // S0 = S_ptr;
                pi = S_ptr + pos;
                S_ptr += I;
                /* for j even, we sieve only odd pi, so step = 2p. */
                p_or_2p += p_or_2p;
                if (!((i_compens_sublat + pos) & 1)) {
                    pi += p;
                }
                SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S_ptr, logp);
                pos += r; if (pos >= (int) p) pos -= p; 
                /* Next line */
                if (++j >= nj) break;
                p_or_2p >>= 1;
j_odd:
                WHERE_AM_I_UPDATE(w, j, j);
                // S0 = S_ptr;
                pi = S_ptr + pos;
                S_ptr += I;
                SMALLSIEVE_ASSEMBLY_OLD(pi, p_or_2p, S_ptr, logp);
                pos += r; if (pos >= (int) p) pos -= p;
            } while (++j < nj);

            pos += ssp.get_offset();
            if (pos >= (int) p) pos -= p;

        p_pos = pos;
    }
    }
}
/*}}}*/

template<typename even_code, typename odd_code, bool fragment> void devel_branch_meta(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /*{{{*/
{
    SMALLSIEVE_COMMON_DEFS();
    ASSERT_ALWAYS(positions.size() == primes.size());
    for(auto const & ssp : primes) {
        int & p_pos(positions[&ssp - &primes.front()]);
        int pos = p_pos;

        const fbprime_t p = ssp.get_p();
        const fbprime_t r = ssp.get_r();
        const unsigned char logp = ssp.logp;
        unsigned char * S0 = S;

        overrun_t<fragment> overrun;
        unsigned int j = j0;

        /* we sieve over the area [S0..S0+(i1-i0)], which may
         * actually be just a fragment of a line. After that, if
         * (i1-i0) is different from I, we'll break anyway. So
         * whether we add I or (i1-i0) to S0 does not matter much.
         */
        /* TODO: sublat !!!! j actually corresponds to row S(j), with
         * S(j)=sublatm*j + sublatj0. Therefore:
         *  - j0&1 == 0 is not sufficient to tell the parity of S(j)
         *  - if sublatm is even, we not every other line is even (resp.
         *    odd).
         *  - if sublatm is even, we should actually take advantage of
         *    it, and run specific code, probably at the outer loop
         *    level.
         */
        if (j0 & 1)
                goto j_odd_devel0;

        for( ; j < j1 ; ) {
            /* for j even, we sieve only odd pi, so step = 2p. */
            {
            int xpos = ((si.conf.sublat.i0 + pos) & 1) ? pos : (pos+p);
            overrun = even_code()(S0, S0 + (i1 - i0), S0 - S, xpos, p+p, logp, w);
            }
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p; 
            if (++j >= j1) break;

j_odd_devel0:
            /* now j odd again */
            WHERE_AM_I_UPDATE(w, j, j - j0);
            overrun = odd_code()(S0, S0 + (i1 - i0), S0 - S, pos, p, logp, w);
            S0 += I;
            pos += r; if (pos >= (int) p) pos -= p;
            ++j;
        }

        if (fragment) {
            /* quick notes for incremental adjustment in case I>B (B =
             * LOG_BUCKET_REGION).
             *
             * Let q = 2^(I-B).
             * Let N = a*q+b, and N'=N+nthreads=a'*q+b' ; N' is the
             * next bucket region we'll handle.
             *
             * Let nthreads = u*q+v
             *
             * The row increase is dj = (N' div q) - (N div q) = a'-a
             * The fragment increase is di = (N' mod q) - (N mod q) = b'-b
             *
             * Of course we have -q < b'-b < q
             *
             * dj can be written as (N'-N-(b'-b)) div q, which is an
             * exact division. we rewrite that as:
             *
             * dj = u + (v - (b'-b)) div q
             *
             * where the division is again exact. Now (v-(b'-b))
             * satisfies:
             * -q < v-(b'-b) < 2*q-1
             *
             * so that the quotient may only be 0 or 1.
             *
             * It is 1 if and only if v >= q + b'-b, which sounds like a
             * reasonable thing to check.
             */
            /* available stuff, for computing the next position:
             * p_pos was the position of the first hit on the first line
             *       in this bucket.
             * pos is the position of the first hit on the next line just
             *       above this bucket (possibly +p if we just sieved an
             *       even line -- this is a catch, by the way).
             * overrun is the position of the first hit on the last line
             *       a bucket that would be on the right of this one
             *       (even if we're at the end of a line of buckets. In
             *       effect, (overrun-p_pos) is congruent to B mod p when
             *       the bucket is a single line fragment of size B.
             */
            int N1 = N + nthreads;
            int Q = logI - LOG_BUCKET_REGION;
            int dj = (N1>>Q) - j0;
            int di = (N1&((1<<Q)-1)) - (N&((1<<Q)-1));
            /* Note that B_mod p is not reduced. It may be <0, and
             * may also be >= p if we sieved with 2p because of even j
             *
             * (0 <= overrun < 2p), and (0 <= pos < p), so -p < B_mod_p < 2p
             */
            int B_mod_p = overrun - p_pos;
            /* FIXME: we may avoid some of the cost for the modular
             * reduction, here. Having a mod operation in this place
             * seems to be a fairly terrible idea.
             *
             * dj is either always the same thing, or that same thing +1.
             * di is within a small interval (albeit a centered one).
             * 
             * It seems feasible to get by with a fixed number of
             * conditional subtractions.
             */
            pos = (p_pos + B_mod_p * di + dj * (int) r) % (int) p;
            if (pos < 0) pos += p;
        } else {
            /* skip stride */
            pos += ssp.get_offset();
            if (pos >= (int) p) pos -= p;

    }
        p_pos = pos;
    }
}/*}}}*/


void generated(std::vector<int> & positions, std::vector<ssp_simple_t> const & primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED) /*{{{*/
{
    std::vector<ssp_t> not_nice_primes;
    if (logI > LOG_BUCKET_REGION) {
        small_sieve<true> SS(positions, primes, not_nice_primes, S, logI, N, si.conf.sublat, nthreads);
        // SS.pattern_sieve2(w);
        // SS.pattern_sieve3(w);
        SS.normal_sieve(w);
    } else {
        small_sieve<false> SS(positions, primes, not_nice_primes, S, logI, N, si.conf.sublat, nthreads);
        // SS.pattern_sieve2(w);
        // SS.pattern_sieve3(w);
        SS.normal_sieve(w);
    }
}
/*}}}*/


struct cand_func {
    typedef void (*ss_func)(std::vector<int> & positions, std::vector<ssp_simple_t> const& primes, unsigned char * S, int logI, unsigned int N, where_am_I & w MAYBE_UNUSED);
    bool sel;
    ss_func f;
    const char * name;
    cand_func(bool sel, ss_func f, const char * name) : sel(sel), f(f), name(name) {}
};

typedef std::vector<cand_func> candidate_list;

/* code factory: first we create a list of functions that have the
 * currently recorded best code for even lines, and then we iterate on
 * the various options for odd lines
 */
template<int bit, bool fragment> struct factory_for_bit_round1 {
    typedef typename best_evenline<bit>::type Be;
    typedef typename best_oddline<bit>::type Bo;
    template<typename T> struct iterator {
        void operator()(candidate_list &) {}
    };
    template<typename T, typename U> struct iterator<list_car<T, U>> {
        void operator()(candidate_list & res) const {
            bool sel = std::is_same<Bo, T>::value;
            res.push_back(cand_func {sel, &devel_branch_meta<Be, T, fragment>, T::name});
            iterator<U>()(res);
        }
    };
    void operator()(candidate_list & res) const {
        iterator<typename all_candidates_for_oddline<bit>::type>()(res);
    }
};
/* and now the other way around. */
template<int bit, bool fragment> struct factory_for_bit_round2 {
    typedef typename best_evenline<bit>::type Be;
    typedef typename best_oddline<bit>::type Bo;
    template<typename T> struct iterator {
        void operator()(candidate_list &) {}
    };
    template<typename T, typename U> struct iterator<list_car<T, U>> {
        void operator()(candidate_list & res) const {
            bool sel = std::is_same<Be, T>::value;
            res.push_back(cand_func {sel, &devel_branch_meta<T, Bo, fragment>, T::name});
            iterator<U>()(res);
        }
    };
    void operator()(candidate_list & res) const {
        iterator<typename all_candidates_for_evenline<bit>::type>()(res);
    }
};

struct bench_base {
#define BBLACK(X) "\e[01;30m" X "\e[00;30m"
#define BRED(X)   "\e[01;31m" X "\e[00;30m"
#define BGREEN(X) "\e[01;32m" X "\e[00;30m"
#define BYELLOW(X)"\e[01;33m" X "\e[00;30m"
#define BBLUE(X)  "\e[01;34m" X "\e[00;30m"
#define BVIOLET(X)"\e[01;35m" X "\e[00;30m"
#define BNAVY(X)  "\e[01;36m" X "\e[00;30m"
#define BLACK(X) "\e[00;30m" X "\e[00;30m"
#define RED(X)   "\e[00;31m" X "\e[00;30m"
#define GREEN(X) "\e[00;32m" X "\e[00;30m"
#define YELLOW(X)"\e[00;33m" X "\e[00;30m"
#define BLUE(X)  "\e[00;34m" X "\e[00;30m"
#define VIOLET(X)"\e[00;35m" X "\e[00;30m"
#define NAVY(X)  "\e[00;36m" X "\e[00;30m"

    std::vector<ssp_simple_t> allprimes;
    std::vector<int> positions;
    unsigned char * S;
    size_t B;
    int logI;
    int logA;

    bench_base(int logB, int logI, int logA) : logI(logI), logA(logA) {
        B = 1UL << logB;
        posix_memalign((void**)&S, B, B);
    }
    bench_base(bench_base const &) = delete;
    ~bench_base() { free(S); }
    bool test(candidate_list const & cand, const char * pfx="", std::string const& goal="current selection") {
        if (consistency_check_mode) {
            return test_correctness(cand);
        }
        if (cand.empty()) return true;
        std::vector<unsigned char *> refS;
        size_t I = 1UL << logI;
        int Nmax = 1 << (logA - LOG_BUCKET_REGION);

        std::vector<double> timings;
        int sel_index = -1;

        where_am_I w;
        for(auto const& bf : cand) {
            if (bf.sel) sel_index = &bf-&cand.front();

            /* Do it once just to warm up */
            positions.clear();
            for(auto const & ssp : allprimes)
                positions.push_back((I/2)%ssp.get_p());
            memset(S, 0, B);
            for(int N = 0 ; N < Nmax ; N++) {
                (*bf.f)(positions, allprimes, S, logI, N, w);
            }

            /* and now with the intent of computing timings.  */
            positions.clear();
            for(auto const & ssp : allprimes)
                positions.push_back((I/2)%ssp.get_p());
            memset(S, 0, B);
            clock_t tt = clock();
            for(int N = 0 ; N < Nmax ; N++) {
                (*bf.f)(positions, allprimes, S, logI, N, w);
            }

            timings.push_back((double) (clock()-tt) / CLOCKS_PER_SEC);
            printf(".");
            fflush(stdout);
            /* FIXME: we're only checking the very last region, here ! */
            if (!refS.empty()) {
                if (memcmp(S, refS.front(), B) != 0) {
                    fprintf(stderr, "inconsistency between %s and %s\n",
                            bf.name, cand.front().name);
                    for(size_t i = 0, n = 0 ; i < B && n < 16 ; i++) {
                        if (S[i] != refS.front()[i]) {
                            fprintf(stderr, "%04x: %02x != %02x\n",
                                    (unsigned int) i,
                                    (unsigned int) S[i],
                                    (unsigned int) refS.front()[i]);
                            n++;
                        }
                    }
                    if (abort_on_fail) abort();
                }
            }
            unsigned char * Scopy;
            posix_memalign((void**)&Scopy, B, B);
            memcpy(Scopy, S, B);
            refS.push_back(Scopy);
        }
        printf("\n");

        size_t best_index = std::min_element(timings.begin(), timings.end()) - timings.begin();
        double best_time = timings[best_index];
        if (sel_index >= 0) {
            bool sel_best = (size_t) sel_index == best_index;
            bool sel_tied = timings[sel_index] <= 1.05 * best_time;;
            for(auto const& bf : cand) {
                size_t index = &bf-&cand.front();
                double tt = timings[index];
                bool is_best = index == best_index;
                bool near_best = tt <= 1.05 * best_time;
                printf("%s%-26s:\t%.3f\t", pfx, bf.name, tt);
                if (bf.sel) {
                    if (sel_best) {
                        printf(BGREEN("%s") "\n", goal.c_str());
                    } else if (sel_tied) {
                        printf(GREEN("%s, tied") "\n", goal.c_str());
                    } else {
                        printf(BRED("%s, wrong (+%.1f%%)?") "\n",
                                goal.c_str(),
                                100.0*(timings[sel_index]/best_time-1)
                                );
                    }
                } else if (is_best && !sel_best) {
                    if (sel_tied) {
                         printf(GREEN("current best, tied") "\n");
                    } else {
                        printf(BRED("current best") "\n");
                    }
                } else if (near_best) {
                    printf(GREEN("tied")"\n");
                } else {
                    printf("\n");
                }
            }
        } else {
            for(auto const& bf : cand) {
                size_t index = &bf-&cand.front();
                double tt = timings[index];
                bool is_best = index == best_index;
                bool near_best = tt <= 1.05 * best_time;
                printf("%s%-24s:\t%.3f\t", pfx, bf.name, tt);
                if (is_best) {
                    printf(GREEN("current best") "\n");
                } else if (near_best) {
                    printf(GREEN("tied")"\n");
                } else {
                    printf("\n");
                }
            }
        }

        for(unsigned char * Sp : refS)
            free(Sp);
        return true;
    }

    /* This one has the loop order reversed. We're no longer testing
     * speed, we're checking correctness.
     */
    bool test_correctness(candidate_list const & cand) {
        if (cand.empty()) return true;
        if (cand.size()==1) {
            fprintf(stderr, "warning, %s is the only function we have, there's nothing to check againts...\n", cand.front().name);
            return true;
        }
        size_t I = 1UL << logI;
        int Nmax = 1 << (logA - LOG_BUCKET_REGION);

        positions.clear();
        for(auto const & ssp : allprimes)
            positions.push_back((I/2)%ssp.get_p());

        std::vector<unsigned char *> refS;
        std::vector<std::vector<int>> refpos;
        for(size_t i = 0 ; i < cand.size() ; i++) {
            unsigned char * Scopy;
            posix_memalign((void**)&Scopy, B, B);
            refS.push_back(Scopy);
            /* make a copy */
            refpos.push_back(positions);
        }
        printf("\n");
        if (!quiet) printf("Testing all %d buckets (logA=%d, logB=%d, logI=%d) for functions:\n",
                Nmax, logA, LOG_BUCKET_REGION, logI);
        for(auto const& bf : cand)
            if (!quiet) printf("  %s\n", bf.name);
        bool ok=true;
        std::vector<bool> ok_perfunc (cand.size(), true);
        int ndisp = 1;
        where_am_I w;
        for(int N = 0 ; N < Nmax ; N++) {
            for(auto const& bf : cand) {
                size_t index = &bf-&cand.front();
                memset(refS[index], 0, B);
                (*bf.f)(refpos[index], allprimes, refS[index], logI, N, w);
            }
            for(auto const& bf : cand) {
                size_t index = &bf-&cand.front();
                if (!ok_perfunc[index]) continue;
                if (index > 0) {
                    if (memcmp(refS[index], refS.front(), B) != 0) {
                        ok = ok_perfunc[index] = false;
                        fprintf(stderr, "bucket %d: inconsistency between %s and %s\n",
                                N, bf.name, cand.front().name);
                        for(size_t i = 0, n = 0 ; i < B && n < 16 ; i++) {
                            if (refS[index][i] != refS.front()[i]) {
                                fprintf(stderr, "%04x: %02x != %02x\n",
                                        (unsigned int) i,
                                        (unsigned int) refS[index][i],
                                        (unsigned int) refS.front()[i]);
                                n++;
                            }
                        }
                    }
                    if (refpos[index] != refpos.front()) {
                        ok = ok_perfunc[index] = false;
                        fprintf(stderr, "bucket %d: inconsistency in post-sieve positions between %s and %s\n",
                                N, bf.name, cand.front().name);
                        for(size_t i = 0, n = 0 ; i < allprimes.size() && n < 16 ; i++) {
                            if (refpos[index][i] != refpos.front()[i]) {
                                fprintf(stderr, "p=%u r=%u ; %d!=%d\n",
                                        allprimes[i].get_p(),
                                        allprimes[i].get_r(),
                                        refpos[index][i],
                                        refpos.front()[i]);
                                n++;
                            }
                        }
                    }
                }
            }
            if (abort_on_fail && !ok) abort();
            if (!quiet) for( ; (N+1)*16 >= (ndisp * Nmax) ; ndisp++) {
                printf(".");
                fflush(stdout);
            }
        }
        if (!quiet) printf("\n");
        for(unsigned char * Sp : refS)
            free(Sp);
        return ok;
    }
};

void store_primes(std::vector<ssp_simple_t>& allprimes, int bmin, int bmax, gmp_randstate_t rstate)
{
    mpz_t pz;
    mpz_init(pz);
    mpz_set_ui(pz, 1u << bmin);
    for(;;) {
        mpz_nextprime(pz, pz);
        if (mpz_cmp_ui(pz, 1u << bmax) >= 0) break;
        unsigned long p = mpz_get_ui(pz);
        unsigned long r = gmp_urandomm_ui(rstate, p);
        /* not a problem if we put garbage here */
        unsigned char logp = log2(p)/log2(1.6);
        allprimes.emplace_back(p, r, logp, nthreads-1);
    }
    if (!quiet) printf("created a list of %zu primes\n", allprimes.size());
    mpz_clear(pz);
}

void declare_usage(cxx_param_list & pl)
{
    param_list_decl_usage(pl, "q",  "quiet mode (for tests, mostly)");
    param_list_decl_usage(pl, "C",  "run tests, not timings");
    param_list_decl_usage(pl, "F",  "abort on test failure");
    param_list_decl_usage(pl, "I",  "set logI (default = 16)");
    param_list_decl_usage(pl, "A",  "set logA (default = 2*logI-1)");
#ifndef LOG_BUCKET_REGION_IS_A_CONSTANT
    param_list_decl_usage(pl, "B",  "set LOG_BUCKET_REGION (default = 16)");
#endif
    param_list_decl_usage(pl, "bmin",  "restrict test or timings to primes >= 2^bmin (i.e. (bmin+1)-bit primes)");
    param_list_decl_usage(pl, "bmax",  "restrict test or timings to primes < 2^bmax (i.e. up to bmax-bit primes)");
    param_list_decl_usage(pl, "only-complete-functions",  "restrict to testing the complete small sieve functions");
}

int main(int argc0, char * argv0[])
{
    int logI = 16;
    int logA = 0;
    int bmin = 0;
    int bmax = 0;
    int argc = argc0;
    char **argv = argv0;
    cxx_param_list pl;

    declare_usage(pl);
    param_list_configure_switch(pl, "-q", &quiet);
    param_list_configure_switch(pl, "-C", &consistency_check_mode);
    param_list_configure_switch(pl, "-F", &abort_on_fail);
    param_list_configure_switch(pl, "--only-complete-functions", &only_complete_functions);
    argv++, argc--;
    for( ; argc ; ) {
        if (param_list_update_cmdline(pl, &argc, &argv)) { continue; }
        fprintf(stderr, "Unhandled parameter %s\n", argv[0]);
        param_list_print_usage(pl, argv0[0], stderr);
        exit(EXIT_FAILURE);
    }


    param_list_parse_int(pl, "I", &logI);
    param_list_parse_int(pl, "A", &logA);
#ifndef LOG_BUCKET_REGION_IS_A_CONSTANT
    param_list_parse_int(pl, "B", &LOG_BUCKET_REGION);
#endif
    param_list_parse_int(pl, "bmin", &bmin);
    param_list_parse_int(pl, "bmax", &bmax);

    si.conf.logI_adjusted = logI;
    if (!bmax) bmax = logI;
    if (!logA) logA = 2*logI-1;

    gmp_randstate_t rstate;
    gmp_randinit_default(rstate);

    nthreads=1;

    mpz_init_set_ui(si.qbasis.q, 4294967291);

    int errors = 0;

    if (!only_complete_functions) {
        // std::vector<std::pair<size_t, size_t>> bounds_perbit(logI + 1, {0,0});
        for(int b = bmin ; b < bmax ; b++) {
            if (b == 0) continue;
#if 0
            /* primes within [1<<b, 1<<(b+1)] are (b+1)-bit primes. With respect to the
             * semantics that we use in the template code, those are counted as
             * (logI-bit) off the bound. So they will use the code in
             * all_candidates_for_evenline<logI-bit> and
             * all_candidates_for_oddline<logI-bit>.
             */
            size_t i0 = 0;
            for( ; i0 < allprimes.size() && !(allprimes[i0].get_p() >> b) ; i0++);
            size_t i1 = i0;
            for( ; i1 < allprimes.size() && (allprimes[i1].get_p() >> b) ; i1++);
            bounds_perbit[b] = { i0, i1 };
#endif
            int logfill = std::min(logI, LOG_BUCKET_REGION);

            printf("===== now doing specific tests for %d-bit primes using candidates<%d> =====\n", b+1, logfill-b);
            bench_base bbase(LOG_BUCKET_REGION, logI, logA);
            // std::vector<ssp_simple_t>& allprimes(bbase.allprimes);
            // std::vector<int>& positions(bbase.positions);
            // unsigned char * & S (bbase.S);
            store_primes(bbase.allprimes, b, b + 1, rstate);


            // std::vector<ssp_simple_t> lp(allprimes.begin() + i0, allprimes.begin() + i1);
            // std::vector<int>& lx(positions.begin() + i0, positions.begin() + i1);
            candidate_list cand1, cand2;

            if (logI > LOG_BUCKET_REGION) {
                // (true!) bool fragment = logI > LOG_BUCKET_REGION;

                switch(logfill-b) {
#define CASE(xxx)						\
                    case xxx:					\
                            factory_for_bit_round1<xxx, true>()(cand1);	\
                            factory_for_bit_round2<xxx, true>()(cand2);	\
                    break
                    CASE(1); CASE(2); CASE(3); CASE(4);
                    CASE(5); CASE(6); CASE(7); CASE(8);
                    CASE(9); CASE(10); CASE(11); CASE(12);
                    CASE(13); CASE(14); CASE(15);
#undef CASE
                    default:
                    if (logfill <= b) {
                        factory_for_bit_round1<0, true>()(cand1);
                        factory_for_bit_round2<0, true>()(cand2);
                    } else {
                        fprintf(stderr, "sorry, not handled\n");
                    }
                }
            } else {
            switch(logfill-b) {
#define CASE(xxx)						\
                case xxx:					\
                            factory_for_bit_round1<xxx, false>()(cand1);	\
                            factory_for_bit_round2<xxx, false>()(cand2);	\
                break
                CASE(1); CASE(2); CASE(3); CASE(4);
                CASE(5); CASE(6); CASE(7); CASE(8);
                CASE(9); CASE(10); CASE(11); CASE(12);
                CASE(13); CASE(14); CASE(15);
#undef CASE
                default:
                if (logfill <= b) {
                        factory_for_bit_round1<0, false>()(cand1);
                        factory_for_bit_round2<0, false>()(cand2);
                } else {
                    fprintf(stderr, "sorry, not handled\n");
                }
            }
            }

            std::vector<unsigned char *> refS;

            if (!cand1.empty()) {
                std::ostringstream goal_name;
                goal_name << "best_oddline<" << logfill-b << ">";
                printf("  testing odd lines ");
                errors += !bbase.test(cand1, "    ", goal_name.str());
            }
            if (!cand2.empty()) {
                std::ostringstream goal_name;
                goal_name << "best_evenline<" << logfill-b << ">";
                printf("  testing even lines ");
                errors += !bbase.test(cand2, "    ", goal_name.str());
            }
        }
    }

    printf("===== now doing tests for complete functions =====\n");
    {
        bench_base bbase(LOG_BUCKET_REGION, logI, logA);
        // std::vector<ssp_simple_t>& allprimes(bbase.allprimes);
        // std::vector<int>& positions(bbase.positions);
        // unsigned char * & S (bbase.S);
        store_primes(bbase.allprimes, bmin, bmax, rstate);

        candidate_list funcs {
            // modified_I18_branch_C,
                { false, current_I18_branch, "I18" },
                { false, devel_branch, "devel" },
                { false, generated, "devel-meta" },
        };

        if (logI <= LOG_BUCKET_REGION) funcs.emplace_back(false, legacy_branch, "legacy");
        funcs.emplace_back(false, legacy_mod_branch, "legacy_mod");

        errors += !bbase.test(funcs);
    }

    mpz_clear(si.qbasis.q);
    gmp_randclear(rstate);

    return errors ? EXIT_FAILURE : EXIT_SUCCESS;
}

