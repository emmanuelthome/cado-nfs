#include "cado.h"
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cctype>
#include <gmp.h>
#include "fb.h"
#include "mod_ul.h"
#include "verbose.h"
#include "getprime.h"
#include "gmp_aux.h"
#include "gzip.h"


static unsigned int fb_log_2 (fbprime_t);
static bool fb_linear_root (fbroot_t *, const mpz_t *, fbprime_t);

/* strtoul(), but with const char ** for second argument.
   Otherwise it's not possible to do, e.g., strtoul(p, &p, 10) when p is
   of type const char *
*/
static inline unsigned long int
strtoul_const(const char *nptr, const char **endptr, const int base)
{
  char *end;
  unsigned long r;
  r = strtoul(nptr, &end, base);
  *endptr = end;
  return r;
}

static inline unsigned long long int
strtoull_const(const char *nptr, const char **endptr, const int base)
{
  char *end;
  unsigned long long r;
  r = strtoull(nptr, &end, base);
  *endptr = end;
  return r;
}

static inline redc_invp_t
compute_invq(fbprime_t q)
{
  if (q % 2 != 0) {
    ASSERT(sizeof(unsigned long) >= sizeof(redc_invp_t));
    return (redc_invp_t) (- ularith_invmod (q));
  } else {
    return 0;
  }
}

/* If q = p^k, with k maximal and k > 1, return q.
   Otherwise return 0. If final_k is not NULL, write k there. */
fbprime_t
fb_is_power (fbprime_t q, unsigned long *final_k)
{
  unsigned long maxk, k;
  uint32_t p;

  maxk = fb_log_2(q);
  for (k = maxk; k >= 2; k--)
    {
      double dp = pow ((double) q, 1.0 / (double) k);
      double rdp = trunc(dp + 0.5);
      if (fabs(dp - rdp) < 0.001) {
        p = (uint32_t) rdp ;
        if (q % p == 0) {
          // ASSERT (fb_pow (p, k) == q);
          if (final_k != NULL)
            *final_k = k;
          return p;
        }
      }
    }
  return 0;
}


/* Allow construction of a root from a linear polynomial and a prime (power) */
fb_general_root::fb_general_root (fbprime_t q, const mpz_t *poly,
  const unsigned char nexp, const unsigned char oldexp)
  : exp(nexp), oldexp(oldexp)
{
  proj = fb_linear_root (&r, poly, q);
}


/* Allow assignment-construction of general entries from simple entries */
template <int Nr_roots>
fb_general_entry::fb_general_entry (const fb_entry_x_roots<Nr_roots> &e) {
  p = q = e.p;
  k = 1;
  invq = compute_invq(q);
  for (int i = 0; i < Nr_roots; i++) {
    /* Use simple constructor for root */
    roots[i] = e.roots[i];
  }
}


/* Return whether this is a simple factor base prime.
   It is simple if it is a prime (not a power) and all its roots are simple. */
bool
fb_general_entry::is_simple() const
{
  bool is_simple = (k == 1);
  for (unsigned char i = 0; i < nr_roots; i++) {
    is_simple &= roots[i].is_simple();
  }
  return is_simple;
}


/* Read roots from a factor base file line and store them in roots.
   line must point at the first character of the first root on the line.
   linenr is used only for printing error messages in case of parsing error.
   Returns the number of roots read. */
void
fb_general_entry::read_roots (const char *lineptr, const unsigned char nexp,
                              const unsigned char oldexp,
                              const unsigned long linenr)
{
    unsigned long long last_t = 0;

    nr_roots = 0;
    while (*lineptr != '\0')
    {
        if (nr_roots == MAXDEGREE) {
            verbose_output_print (1, 0,
                    "# Error, too many roots for prime (power) %" FBPRIME_FORMAT
                    " in factor base line %lu\n", q, linenr);
            exit(EXIT_FAILURE);
        }
        /* Projective roots r, i.e., ar == b (mod q), are stored as q + r in
           the factor base file; since q can be a 32-bit value, we read the
           root as a 64-bit integer first and subtract q if necessary. */
        const unsigned long long t = strtoull_const (lineptr, &lineptr, 10);
        if (nr_roots > 0 && t <= last_t) {
            verbose_output_print (1, 0,
                "# Error, roots must be sorted in the fb file, line %lu\n",
                linenr);
            exit(EXIT_FAILURE);
        }
        last_t = t;

        roots[nr_roots++] = fb_general_root(t, q, nexp, oldexp);
        if (*lineptr != '\0' && *lineptr != ',') {
            verbose_output_print (1, 0,
                    "# Incorrect format in factor base file line %lu\n",
                    linenr);
            exit(EXIT_FAILURE);
        }
        if (*lineptr == ',')
            lineptr++;
    }

    if (nr_roots == 0) {
        verbose_output_print (1, 0, "# Error, no root for prime (power) %"
                FBPRIME_FORMAT " in factor base line %lu\n", q, linenr - 1);
        exit(EXIT_FAILURE);
    }
}

/* Parse a factor base line.
   Return 1 if the line could be parsed and was a "short version", i.e.,
   without explicit old and new exponent.
   Return 2 if the line could be parsed and was a "long version".
   Otherwise return 0. */
void
fb_general_entry::parse_line (const char * lineptr, const unsigned long linenr)
{
    q = strtoul_const (lineptr, &lineptr, 10);
    if (q == 0) {
        verbose_output_print (1, 0, "# fb_read: prime is not an integer on line %lu\n",
                              linenr);
        exit (EXIT_FAILURE);
    } else if (*lineptr != ':') {
        verbose_output_print (1, 0,
                "# fb_read: prime is not followed by colon on line %lu",
                linenr);
        exit (EXIT_FAILURE);
    }

    lineptr++; /* Skip colon after q */
    const bool longversion = (strchr(lineptr, ':') != NULL);

    /* NB: a short version is not permitted for a prime power, so we
     * do the test for prime powers only for long version */
    p = q;
    unsigned char nexp = 1, oldexp = 0;
    k = 1;
    if (longversion) {
        unsigned long k_ul;
        const fbprime_t base = fb_is_power (q, &k_ul);
        ASSERT(ulong_isprime(base != 0 ? base : q));
        /* If q is not a power, then base==0, and we use p = q */
        if (base != 0) {
            p = base;
            k = static_cast<unsigned char>(k_ul);
        }

        /* read the multiple of logp, if any */
        /* this must be of the form  q:nlogp,oldlogp: ... */
        /* if the information is not present, it means q:1,0: ... */
        nexp = strtoul_const (lineptr, &lineptr, 10);

        if (nexp == 0) {
            verbose_output_print (1, 0, "# Error in fb_read: could not parse "
                "the integer after the colon of prime %" FBPRIME_FORMAT "\n",
                q);
            exit (EXIT_FAILURE);
        }
        if (*lineptr != ',') {
            verbose_output_print (1, 0,
		    "# fb_read: exp is not followed by comma on line %lu",
		    linenr);
            exit (EXIT_FAILURE);
        }
        lineptr++; /* skip comma */
        oldexp = strtoul_const (lineptr, &lineptr, 10);
        if (*lineptr != ':') {
            verbose_output_print (1, 0,
		    "# fb_read: oldlogp is not followed by colon on line %lu",
		    linenr);
            exit (EXIT_FAILURE);
        }
        ASSERT (nexp > oldexp);
        lineptr++; /* skip colon */
    }

    read_roots(lineptr, nexp, oldexp, linenr);

    /* exp and oldexp are a property of a root, not of a prime (power).
       The factor base file should specify them per root, but specifies
       them per prime instead - a bit of a design bug.
       For long version lines, we thus use the exp and oldexp values for all
       roots specified in that line. */
    for (unsigned char i = 1; i < nr_roots; i++) {
      roots[i].exp = roots[0].exp;
      roots[i].oldexp = roots[0].oldexp;
    }
}

void
fb_general_entry::fprint(FILE *out) const
{
  fprintf(out, "%" FBPRIME_FORMAT ": ", q);
  for (unsigned char i_root = 0; i_root < nr_roots; i_root++) {
    roots[i_root].fprint(out, q);
    if (i_root + 1 < nr_roots)
      fprintf(out, ",");
  }
  fprintf(out, "\n");
}

void
fb_general_entry::merge (const fb_general_entry &other)
{
  ASSERT_ALWAYS(p == other.p && q == other.q && k == other.k);
  for (unsigned char i_root = 0; i_root < other.nr_roots; i_root++) {
    ASSERT_ALWAYS(nr_roots < MAXDEGREE);
    roots[nr_roots++] = other.roots[i_root];
  }
}

void
fb_general_entry::transform_roots(fb_general_entry::transformed_entry_t &result,
                                  qlattice_basis_srcptr basis) const
{
  result.p = p;
  result.q = q;
  result.invq = invq;
  result.k = k;
  result.nr_roots = nr_roots;
  /* TODO: Use batch-inversion here */
  for (unsigned char i_root = 0; i_root < nr_roots; i_root++)
    roots[i_root].transform(result.roots[i_root], q, invq, basis);
}


template <int Nr_roots>
void
fb_entry_x_roots<Nr_roots>::transform_roots(fb_entry_x_roots<Nr_roots>::transformed_entry_t &result, qlattice_basis_srcptr basis) const
{
  result.p = p;
  const redc_invp_t invq = compute_invq(p);
  /* TODO: Use batch-inversion here */
  for (unsigned char i_root = 0; i_root < nr_roots; i_root++) {
    const unsigned long long t = fb_root_in_qlattice(p, roots[i_root], invq, basis);
    result.proj[i_root] = (t >= p);
    result.roots[i_root] = (t < p) ? t : (t - p);
  }
}


template <int Nr_roots>
void
fb_entry_x_roots<Nr_roots>::fprint(FILE *out) const
{
  fprintf(out, "%" FBPRIME_FORMAT ": ", p);
  for (size_t i = 0; i < Nr_roots; i++) {
    fprintf(out, "%" FBROOT_FORMAT "%s", roots[i],
	    (i + 1 < Nr_roots) ? "," : "");
  }
  fprintf(out, "\n");
}


template <class FB_ENTRY_TYPE>
void
fb_vector<FB_ENTRY_TYPE>::_count_entries(size_t *nprimes, size_t *nroots, double *weight) const
{
  if (nprimes != NULL)
    *nprimes += this->size();
  double w = 0.;
  size_t nr = 0;
  for (size_t i = 0; i < this->size(); i++) {
    nr += (*this)[i].nr_roots;
    w += (double) (*this)[i].nr_roots / (double) (*this)[i].get_q();
  }
  if (nroots != NULL)
    *nroots += nr;
  if (weight != NULL)
    *weight += w;
}

template <class FB_ENTRY_TYPE>
void
fb_vector<FB_ENTRY_TYPE>::fprint(FILE *out) const
{
  for (size_t i = 0; i < this->size(); i++)
    (*this)[i].fprint(out);
}

template <class FB_ENTRY_TYPE>
void
fb_vector<FB_ENTRY_TYPE>::extract_bycost(std::vector<unsigned long> &p, fbprime_t pmax, fbprime_t td_thresh) const
{
  for (size_t i = 0; i < this->size(); i++)
    (*this)[i].extract_bycost(p, pmax, td_thresh);
}


template <class FB_ENTRY_TYPE>
fb_transformed_vector *
fb_slice<FB_ENTRY_TYPE>::make_lattice_bases(qlattice_basis_srcptr basis, const int logI) const
{
  typename FB_ENTRY_TYPE::transformed_entry_t transformed;
  /* Create a transformed vector and store the index of the slice we currently
     transform */
  fb_transformed_vector *result = new fb_transformed_vector(get_index());
  size_t i_entry = 0;
  for (const FB_ENTRY_TYPE *it = cbegin(); it != cend(); it++, i_entry++) {
    it->transform_roots(transformed, basis);
    for (unsigned char i_root = 0; i_root < transformed.nr_roots; i_root++) {
      const fbroot_t r = transformed.get_r(i_root);
      const bool proj = transformed.get_proj(i_root);
      plattice_sieve_entry pli = plattice_sieve_entry(transformed.p, r, proj, logI, (prime_hint_t)i_entry);
      result->push_back(pli);
    }
  }
  return result;
}


template <class FB_ENTRY_TYPE>
void
fb_slice<FB_ENTRY_TYPE>::fprint(FILE *out) const
{
  for (const FB_ENTRY_TYPE *it = cbegin(); it != cend(); it++) {
    it->fprint(out);
  }
}


/* http://stackoverflow.com/questions/24130093/gdb-could-not-find-operator */
template class std::vector<fb_general_entry>;


template <class FB_ENTRY_TYPE>
void
fb_slices<FB_ENTRY_TYPE>::make_slices(const double scale, slice_index_t &next_index)
{
  /* Entries in vec must be sorted */

  /* Remove old entries in slices */
  slices.clear();

  size_t cur_slice_start = 0;

  while (cur_slice_start < vec.size()) {
    const unsigned char cur_logp = fb_log (vec[cur_slice_start].p, scale, 0.);

    size_t next_slice_start = std::min(cur_slice_start + max_slice_len, vec.size());
    ASSERT_ALWAYS(cur_slice_start != next_slice_start);

    /* See if last element of the current slice has a greater log(p) than
       the first element */
    if (cur_logp < fb_log (vec[next_slice_start - 1].p, scale, 0.)) {
      /* Find out the first place where log(p) changes occurs, and make that
         next_slice_start */
      for (next_slice_start = cur_slice_start;
           cur_logp == fb_log (vec[next_slice_start].p, scale, 0.);
           next_slice_start++);
      /* We know that a log(p) change occurs so the slice len cannot exceed
         the max allowed */
      ASSERT_ALWAYS(next_slice_start <= cur_slice_start + max_slice_len);
    }

    verbose_output_print (0, 4, "# Slice %u starts at offset %zu (p = %"
        FBPRIME_FORMAT ", log(p) = %u) and ends at offset %zu (p = %"
        FBPRIME_FORMAT ", log(p) = %u)\n",
           (unsigned int) next_index, cur_slice_start, vec[cur_slice_start].p,
           (unsigned int) fb_log (vec[cur_slice_start].p, scale, 0.), 
           next_slice_start - 1, vec[next_slice_start - 1].p,
           (unsigned int) fb_log (vec[next_slice_start - 1].p, scale, 0.));
    fb_slice<FB_ENTRY_TYPE> s = fb_slice<FB_ENTRY_TYPE>(vec, vec.data() + cur_slice_start, vec.data() + next_slice_start, cur_logp, next_index++);
    slices.push_back(s);

    cur_slice_start = next_slice_start;
  }
}

template <class FB_ENTRY_TYPE>
void
fb_slices<FB_ENTRY_TYPE>::sort()
{
  std::sort(vec.begin(), vec.end());
}


template <class FB_ENTRY_TYPE>
void
fb_slices<FB_ENTRY_TYPE>::fprint(FILE *out) const
{
  /* If we have sliced the entries, we print them one slice at a time */
  if (!slices.empty()) {
    for (typename std::vector<fb_slice<FB_ENTRY_TYPE> >::const_iterator it = slices.begin(); it != slices.end(); it++) {
      fprintf(out, "#    Slice index = %u, logp = %hhu:\n", (unsigned int) it->get_index(), it->get_logp());
      it->fprint(out);
    }
  } else {
    /* Otherwise we delegate to the vector */
    fprintf(out, "#    Not sliced\n");
    vec.fprint(out);
  }
}


/* Append a factor base entry given in fb_cur to the
   correct vector, as determined by the number of roots.
   "Special" primes are added to the general-primes vector.
   This function is a de-multiplexer: Using "switch" to turn a run-time value
   (fb_cur->nr_roots) into a compile-time constant which can be used as a
   template specifier. */
void
fb_part::append(const fb_general_entry &fb_cur)
{
  /* Non-simple ones go in the general vector, or, if this is an only_general
     part, then all entries do */
  if (only_general || !fb_cur.is_simple()) {
    general_vector.append(fb_cur);
    return;
  }

  /* Simple ones go in the simple vector with the corresponding number of
     roots */
  get_slices(fb_cur.nr_roots)->append(fb_cur);
}

void
fb_part::finalize()
{
  if (!only_general) {
    for (int i_roots = 0; i_roots <= MAXDEGREE; i_roots++) {
      get_slices(i_roots)->finalize();
    }
  }
  general_vector.finalize();
}

void
fb_part::fprint(FILE *out) const
{
  if (!only_general) {
    for (int i_roots = 0; i_roots <= MAXDEGREE; i_roots++) {
      fprintf(out, "#   Entries with %d roots:\n", i_roots);
      cget_slices(i_roots)->fprint(out);
    }
  }

  fprintf(out, "#   General entries (%s):\n",
	  only_general ? "contains all entries" :
	  "powers, ramified primes or primes with projective roots");
  general_vector.fprint(out);
}

void
fb_part::_count_entries(size_t *nprimes, size_t *nroots, double *weight) const
{
  if (!only_general) {
    for (int i_roots = 0; i_roots <= MAXDEGREE; i_roots++)
      cget_slices(i_roots)->_count_entries(nprimes, nroots, weight);
  }
  general_vector._count_entries(nprimes, nroots, weight);
}


void
fb_part::extract_bycost(std::vector<unsigned long> &p, fbprime_t pmax, fbprime_t td_thresh) const
{
  if (!only_general) {
    for (int i_roots = 0; i_roots <= MAXDEGREE; i_roots++)
      cget_slices(i_roots)->extract_bycost(p, pmax, td_thresh);
  }

  general_vector.extract_bycost(p, pmax, td_thresh);
}


void
fb_part::make_slices(const double scale, slice_index_t &next_index)
{
  if (!only_general) {
    for (int i_roots = 0; i_roots <= MAXDEGREE; i_roots++)
      get_slices(i_roots)->make_slices(scale, next_index);
    /* If we store all entries as general entries, we don't slice them,
       as those are the small primes in part 0 which get line sieved */
  }
}


fb_factorbase::fb_factorbase(const fbprime_t *thresholds,
			     const bool *only_general)
{
  for (size_t i = 0; i < FB_MAX_PARTS; i++) {
    ASSERT_ALWAYS(i == 0 || thresholds[i - 1] <= thresholds[i]);
    this->thresholds[i] = thresholds[i];
    // By default, only_general is true for part 0, and false for all others
    const bool og = (only_general == NULL) ? (i == 0) : only_general[i];
    parts[i] = new fb_part(og);
  }
}

fb_factorbase::~fb_factorbase()
{
  for (size_t i = 0; i < FB_MAX_PARTS; i++) {
    delete parts[i];
  }
}

/* Append a factor base entry to the factor base.
   The new entry is inserted into the correct part, as determined by the
   size of the prime p, and within that part, into the correct slice, as
   determined by the number of roots. */
void
fb_factorbase::append(const fb_general_entry &fb_cur)
{
  int i;
  static bool printed_too_large_prime_warning = false;

  /* Find the smallest threshold t such that t >= q */
  for (i = 0; i < FB_MAX_PARTS && fb_cur.q > thresholds[i]; i++);
  /* No prime > largest threshold should ever be added */
  if (i == FB_MAX_PARTS) {
    if (!printed_too_large_prime_warning) {
      verbose_output_print(1, 0, "Factor base entry %" FBPRIME_FORMAT 
                           " is above factor base bound, skipping it "
                           "(and all other too large entries)\n", fb_cur.q);
      printed_too_large_prime_warning = true;
    }
    return; /* silently skip this entry */
  }
  parts[i]->append(fb_cur);
}

/* Remove newline, comment, and trailing space from a line. Write a
   '\0' character to the line at the position where removed part began (i.e.,
   line gets truncated).
   Return length in characters or remaining line, without trailing '\0'
   character.
*/
size_t
read_strip_comment (char *const line)
{
    size_t linelen, i;

    linelen = strlen (line);
    if (linelen > 0 && line[linelen - 1] == '\n')
        linelen--; /* Remove newline */
    for (i = 0; i < linelen; i++) /* Skip comments */
        if (line[i] == '#') {
            linelen = i;
            break;
        }
    while (linelen > 0 && isspace((int)(unsigned char)line[linelen - 1]))
        linelen--; /* Skip whitespace at end of line */
    line[linelen] = '\0';

    return linelen;
}

/* Read a factor base file, splitting it into pieces.
   
   Primes and prime powers up to smalllim go into fb_small. If smalllim is 0,
   all primes go into fb_small, and nothing is written to fb_pieces.
   
   If smalllim is not 0, then nr_pieces separate factor bases are made for
   primes/powers > smalllim; factor base entries from the file are written to 
   these pieces in round-robin manner.

   Pointers to the allocated memory of the factor bases are written to fb_small 
   and, if smalllim > 0, to fb_pieces[0, ..., nr_pieces-1].

   Returns 1 if everything worked, and 0 if not (i.e., if the file could not be 
   opened, or memory allocation failed)
*/

void
fb_factorbase::read(const char * const filename)
{
  fb_general_entry fb_cur, fb_last;
  bool had_entry = false;
  FILE *fbfile;
  // too small linesize led to a problem with rsa768;
  // it would probably be a good idea to get rid of fgets
  const size_t linesize = 1000;
  char line[linesize];
  unsigned long linenr = 0;
  fbprime_t maxprime = 0;
  unsigned long nr_primes = 0;
  
  fbfile = fopen_maybe_compressed (filename, "r");
  if (fbfile == NULL) {
    verbose_output_print (1, 0, "# Could not open file %s for reading\n",
        filename);
    return;
  }
  
  while (!feof(fbfile)) {
    /* Sadly, the size parameter of fgets() is of type int */
    if (fgets (line, static_cast<int>(linesize), fbfile) == NULL)
      break;
    linenr++;
    if (read_strip_comment(line) == (size_t) 0) {
      /* Skip empty/comment lines */
      continue;
    }
    
    fb_cur.parse_line (line, linenr);
    fb_cur.invq = compute_invq(fb_cur.q);
    if (!had_entry) {
      fb_last = fb_cur;
      had_entry = true;
    } else if (fb_cur.q == fb_last.q) {
      fb_last.merge(fb_cur);
    } else {
      append(fb_last);
      fb_last = fb_cur;
    }
    
    /* fb_fprint_entry (stdout, fb_cur); */
    if (fb_cur.p > maxprime)
      maxprime = fb_cur.p;
    nr_primes++;
  }

  if (had_entry)
    append(fb_last);
  
  verbose_output_print (0, 2, "# Factor base successfully read, %lu primes, "
			"largest was %" FBPRIME_FORMAT "\n",
			nr_primes, maxprime);
  
  fclose_maybe_compressed (fbfile, filename);
  
  finalize();
  return;
}

/* Return p^e. Trivial exponentiation for small e, no check for overflow */
fbprime_t
fb_pow (const fbprime_t p, const unsigned long e)
{
    fbprime_t r = 1;

    for (unsigned long i = 0; i < e; i++)
      r *= p;
    return r;
}

/* Returns floor(log_2(n)) for n > 0, and 0 for n == 0 */
static unsigned int
fb_log_2 (fbprime_t n)
{
  unsigned int k;
  for (k = 0; n > 1; n /= 2, k++);
  return k;
}


unsigned char
fb_log (double n, double log_scale, double offset)
{
  const long l = floor (log (n) * log_scale + offset + 0.5);
  return static_cast<unsigned char>(l);
}





/* Make one factor base entry for a linear polynomial poly[1] * x + poly[0]
   and the prime (power) q. We assume that poly[0] and poly[1] are coprime.
   Non-projective roots a/b such that poly[1] * a + poly[0] * b == 0 (mod q)
   with gcd(poly[1], q) = 1 are stored as a/b mod q.
   If do_projective != 0, also stores projective roots with gcd(q, f_1) > 1,
   but stores the reciprocal root.
   Returns true if the roots was projective, and false otherwise. */

static bool
fb_linear_root (fbroot_t *root, const mpz_t *poly, const fbprime_t q)
{
  modulusul_t m;
  residueul_t r0, r1;
  bool is_projective;

  modul_initmod_ul (m, q);
  modul_init_noset0 (r0, m);
  modul_init_noset0 (r1, m);

  modul_set_ul_reduced (r0, mpz_fdiv_ui (poly[0], q), m);
  modul_set_ul_reduced (r1, mpz_fdiv_ui (poly[1], q), m);

  /* We want poly[1] * a + poly[0] * b == 0 <=>
     a/b == - poly[0] / poly[1] */
  is_projective = (modul_inv (r1, r1, m) == 0); /* r1 = 1 / poly[1] */

  if (is_projective)
    {
      ASSERT_ALWAYS(mpz_gcd_ui(NULL, poly[1], q) > 1);
      /* Set r1 = poly[0] % q, r0 = poly[1] (mod q) */
      modul_set (r1, r0, m);
      modul_set_ul_reduced (r0, mpz_fdiv_ui (poly[1], q), m);
      int rc = modul_inv (r1, r1, m);
      ASSERT_ALWAYS(rc != 0);
    }

  modul_mul (r1, r0, r1, m); /* r1 = poly[0] / poly[1] */
  modul_neg (r1, r1, m); /* r1 = - poly[0] / poly[1] */

  *root = modul_get_ul (r1, m);

  modul_clear (r0, m);
  modul_clear (r1, m);
  modul_clearmod (m);

  return is_projective;
}

class fb_powers {
  struct fb_power_t {
    fbprime_t p, q;
    unsigned char k;
  };
  std::vector<fb_power_t> *powers;

  static int
  cmp_powers(fb_power_t a, fb_power_t b)
  {
    return a.q < b.q;
  }

  public:
  fb_powers(fbprime_t);
  ~fb_powers(){delete this->powers;};
  fb_power_t operator[] (const size_t i) {
    return (*this->powers)[i];
  }
  size_t size() {return powers->size();}
};

/* Create a list of prime powers (with exponent >1) up to lim */
fb_powers::fb_powers (const fbprime_t lim)
{
  fbprime_t p;
  powers = new std::vector<fb_power_t>;
  
  for (p = 2; p <= lim / p; p = getprime(1)) {
    fbprime_t q = p;
    unsigned char k = 1;
    do {
      q *= p;
      k++;
      fb_power_t new_entry = {p, q, k};
      powers->push_back(new_entry);
    } while (q <= lim / p);
  }
  getprime(0);
  
  std::sort (powers->begin(), powers->end(), cmp_powers);
}

/* Generate a factor base with primes <= bound and prime powers <= powbound
   for a linear polynomial. If projective != 0, adds projective roots
   (for primes that divide leading coefficient).
   Returns 1 on success, 0 on error. */

void
fb_factorbase::make_linear (const mpz_t *poly, const fbprime_t powbound)
			    
{
  fbprime_t next_prime;
  fb_general_entry fb_cur;

  fb_powers *powers = new fb_powers(powbound);
  size_t next_pow = 0;

  verbose_output_vfprint(0, 1, gmp_vfprintf,
               "# Making factor base for polynomial g(x) = %Zd*x%s%Zd,\n"
               "# including primes up to %" FBPRIME_FORMAT
               " and prime powers up to %" FBPRIME_FORMAT ".\n",
               poly[1], (mpz_cmp_ui (poly[0], 0) >= 0) ? "+" : "",
               poly[0], thresholds[FB_MAX_PARTS-1], powbound);

  for (next_prime = 2; next_prime <= thresholds[FB_MAX_PARTS-1]; ) {
    /* Handle any prime powers that are smaller than next_prime */
    if (next_pow < powers->size() && (*powers)[next_pow].q <= next_prime) {
      /* The list of powers must not include primes */
      ASSERT_ALWAYS((*powers)[next_pow].q < next_prime);
      fb_cur.q = (*powers)[next_pow].q;
      fb_cur.p = (*powers)[next_pow].p;
      fb_cur.k = (*powers)[next_pow].k;
      next_pow++;
    } else {
      fb_cur.q = fb_cur.p = next_prime;
      fb_cur.k = 1;
      next_prime = getprime(1);
    }
    fb_cur.nr_roots = 1;
    fb_cur.roots[0].exp = fb_cur.k;
    fb_cur.roots[0].oldexp = fb_cur.k - 1U;
    fb_cur.roots[0].proj = fb_linear_root (&fb_cur.roots[0].r, poly, fb_cur.q);
    fb_cur.invq = compute_invq(fb_cur.q);
    append(fb_cur);
  }

  getprime (0); /* free prime iterator */

  delete (powers);
  finalize();
}

void
fb_factorbase::finalize()
{
  for (size_t part = 0; part < FB_MAX_PARTS; part++) {
    parts[part]->finalize();
  }
}

void
fb_factorbase::fprint(FILE *out) const
{
  for (size_t part = 0; part < FB_MAX_PARTS; part++) {
    fprintf(out, "#  Factor base entries up to %" FBPRIME_FORMAT "\n", thresholds[part]);
    parts[part]->fprint(out);
  }
}

void
fb_factorbase::_count_entries(size_t *nprimes, size_t *nroots, double *weight) const
{
  for (size_t part = 0; part < FB_MAX_PARTS; part++) {
    parts[part]->_count_entries(nprimes, nroots, weight);
  }
}

void
fb_factorbase::extract_bycost(std::vector<unsigned long> &extracted, fbprime_t pmax, fbprime_t td_thresh) const
{
  for (size_t part = 0; part < FB_MAX_PARTS; part++) {
    parts[part]->extract_bycost(extracted, pmax, td_thresh);
  }
}

void
fb_factorbase::make_slices(const double scale)
{
  slice_index_t next_index = 0;
  for (size_t part = 0; part < FB_MAX_PARTS; part++) {
    parts[part]->make_slices(scale, next_index);
  }
}


#ifdef TESTDRIVE

void output(fb_factorbase *fb, const char *name)
{
  size_t n_primes = 0, n_roots = 0;
  double weight = 0.;
  fb->_count_entries(&n_primes, &n_roots, &weight);
  verbose_output_print (0, 1,
	   "# Factor base %s (%zu primes, %zu roots, %f weight):\n",
	   name, n_primes, n_roots, weight);
  fb->fprint(stdout);
}

int main(int argc, char **argv)
{
  fbprime_t thresholds[4] = {200, 1000, 1000, 1000};
  fbprime_t powbound = 100;
  fb_factorbase *fb1 = new fb_factorbase(thresholds),
    *fb2 = new fb_factorbase(thresholds);

  mpz_t poly[2];

  mpz_init(poly[0]);
  mpz_init(poly[1]);

  mpz_set_ui(poly[0], 727);
  mpz_set_ui(poly[1], 210); /* Bunch of projective primes */

  fb1->make_linear(poly, powbound);
  fb1->make_slices(2.0);
  output(fb1, "from linear polynomial");

  // This line does (and should) fail to compile, as fb_factorbase is
  // NonCopyable:
  // fb_factorbase fb3(*fb1);

  if (argc > 1) {
    fb2->read(argv[1]);
    fb2->make_slices(2.0);
    output(fb2, "from file");
  }

  delete fb1;
  delete fb2;
  bool only_general[4] = {false, false, false, false};
  fb1 = new fb_factorbase(thresholds, only_general);
  fb1->make_linear(poly, powbound);
  fb1->make_slices(2.0);
  output(fb1, "from linear polynomial, only_general = false");

  printf("Trialdiv primes:\n");
  std::vector<unsigned long> *extracted = new std::vector<unsigned long>;
  fb1->extract_bycost(*extracted, 100, 200);
  for (std::vector<unsigned long>::iterator it = extracted->begin(); it != extracted->end(); it++) {
    printf("%lu ", *it);
  }
  printf("\n");

  delete fb1;
  mpz_clear(poly[0]);
  mpz_clear(poly[1]);
  exit(EXIT_SUCCESS);
}
#endif
