#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gmp.h>
#include <assert.h>
#include "cado.h"
#include "utils/utils.h"
#include "poly.h"


void polymodF_from_ab(polymodF_t tmp, long a, unsigned long b) {
  tmp->v = 0;
  tmp->p->deg = 1;
  mpz_set_ui(tmp->p->coeff[1], b);
  mpz_neg(tmp->p->coeff[1], tmp->p->coeff[1]);
  mpz_set_si(tmp->p->coeff[0], a);
}

// Check whether the coefficients of R (that are given modulo m) are in
// fact genuine integers. We assume that x mod m is a genuine integer if
// x or m-x is less than m/10^6.
int poly_integer_reconstruction(poly_t Q, const poly_t R, const mpz_t m) {
  int i;
  mpz_t aux;
  mpz_init(aux);
  for (i=0; i <= R->deg; ++i) {
    mpz_mul_ui(aux, R->coeff[i], 1000000);
    if (mpz_cmp(aux, m) <= 0)
      poly_setcoeff(Q, i, R->coeff[i]);
    else {
      mpz_sub(aux, m, R->coeff[i]);
      mpz_mul_ui(aux, aux, 1000000);
      if (mpz_cmp(aux, m) > 0)
	return 0;
      else {
	mpz_sub(aux, m, R->coeff[i]);
	mpz_neg(aux, aux);
	poly_setcoeff(Q, i, aux);
      }
    }
  }
  return 1;
}


// compute Sqrt(A) mod F, using p-adiv lifting, at prime p.
void polymodF_sqrt(polymodF_t res, polymodF_t AA, poly_t F, unsigned long p) {
  poly_t A;
  int v;
  int d = F->deg;

  poly_alloc(A, d-1);
  // Clean up the mess with denominator: if it is an odd power of fd,
  // then multiply num and denom by fd to make it even.
  if (((AA->v)&1) == 0) {
    v = AA->v / 2;
    poly_copy(A, AA->p);
  } else {
    v = (1+AA->v) / 2;
    poly_mul_mpz(A, AA->p, F->coeff[d]);
  }

  // Now, we just have to take the square root of A (without denom) and
  // divide by fd^v.

  // Variables for the lifted values
  poly_t invsqrtA, sqrtA;
  // variables for A and F modulo pk
  poly_t a, f;
  poly_alloc(invsqrtA, d-1);
  poly_alloc(sqrtA, d-1);
  poly_alloc(a, d-1);
  poly_alloc(f, d);
  // variable for the current pk
  mpz_t pk;
  mpz_init(pk);

  // Initialize things modulo p:
  mpz_set_ui(pk, p);
  poly_reduce_makemonic_mod_mpz(f, F, pk);
  poly_reduce_mod_mpz(a, A, pk);

  // First compute the inverse square root modulo p
  { 
    mpz_t q, aux;
    mpz_init(q);
    mpz_init(aux);

    // compute (q-2)(q+1)/4   (assume q == 3 mod 4, here !!!!!)
    // where q = p^d, the size of the finite field extension.
    // since we work mod q-1, this gives (3*q-5)/4
    mpz_ui_pow_ui(q, p, (unsigned long)d);
    mpz_mul_ui(aux, q, 3);
    mpz_sub_ui(aux, aux, 5);
    mpz_divexact_ui(aux, aux, 4);		        // aux := (3q-5)/4
    poly_power_mod_f_mod_ui(invsqrtA, a, f, aux, p);	

    mpz_clear(aux);
    mpz_clear(q);
  }

  // Now, the lift begins
  // When entering the loop, invsqrtA contains the inverse square root
  // of A computed modulo pk.
  
  poly_t tmp, tmp2;
  poly_alloc(tmp, 2*d-1);
  poly_alloc(tmp2, 2*d-1);
  int expo = 1;
  do {
    mpz_mul(pk, pk, pk);	// double the current precision
    expo <<= 1;
    fprintf(stderr, "lifting mod p^%d\n", expo);
    // compute F,A modulo the new modulus.
    poly_reduce_makemonic_mod_mpz(f, F, pk);  
    poly_reduce_mod_mpz(a, A, pk);

    // now, do the Newton operation x <- 1/2(3*x-a*x^3)
    poly_sqr_mod_f_mod_mpz(tmp, invsqrtA, f, pk);
    poly_mul_mod_f_mod_mpz(tmp, tmp, invsqrtA, f, pk);
    poly_mul_mod_f_mod_mpz(tmp, tmp, a, f, pk);
    poly_mul_ui(tmp2, invsqrtA, 3);
    poly_sub_mod_mpz(tmp, tmp2, tmp, pk);
    poly_div_ui_mod_mpz(invsqrtA, tmp, 2, pk); 

    // multiply by a, to check if rational reconstruction succeeds
    poly_mul_mod_f_mod_mpz(tmp, invsqrtA, a, f, pk);
  } while (!poly_integer_reconstruction(tmp, tmp, pk));

  poly_copy(res->p, tmp);
  res->v = v;


  mpz_clear(pk);
  poly_free(tmp);
  poly_free(tmp2);
  poly_free(f);
}

unsigned long FindSuitableModP(poly_t F) {
  unsigned long p = 2;
  int dF = F->deg;
  
  long_poly_t fp;
  long_poly_init(fp, dF);
  while (1) {
    int d;
    // select prime congruent to 3 mod 4 to have easy sqrt.
    do {
      p = getprime(p);
    } while ((p%4)!= 3);
    d = long_poly_set_mod (fp, F->coeff, dF, p);
    if (d!=dF)
      continue;
    if (isirreducible_mod_long(fp, p))
      break;
  }
  long_poly_clear(fp);
  return p;
}

#define THRESHOLD 2 /* must be >= 2 */

/* accumulate up to THRESHOLD products in prd[0], 2^i*THRESHOLD in prd[i].
   nprd is the number of already accumulated values: if nprd = n0 + 
   n1 * THRESHOLD + n2 * THRESHOLD^2 + ..., then prd[0] has n0 entries,
   prd[1] has n1*THRESHOLD entries, and so on.
*/
// Products are computed modulo the polynomial F.
polymodF_t*
accumulate_fast (polymodF_t *prd, const polymodF_t a, const poly_t F, unsigned long *lprd,
    unsigned long nprd)
{
  unsigned long i;

  polymodF_mul(prd[0], prd[0], a, F);
  nprd ++;

  for (i = 0; nprd % THRESHOLD == 0; i++, nprd /= THRESHOLD)
    {
      /* need to access prd[i + 1], thus i+2 entries */
      if (i + 2 > *lprd)
        {
          lprd[0] ++;
          prd = (polymodF_t*) realloc (prd, *lprd * sizeof (polymodF_t));
	  poly_alloc(prd[i+1]->p, F->deg);
          mpz_set_ui(prd[i + 1]->p->coeff[0], 1);
	  prd[i+1]->p->deg = 0;
	  prd[i+1]->v = 0;
        }
      polymodF_mul(prd[i+1], prd[i+1], prd[i], F);
      mpz_set_ui(prd[i]->p->coeff[0], 1);
      prd[i]->p->deg = 0;
      prd[i]->v = 0;
    }

  return prd;
}

/* prd[0] <- prd[0] * prd[1] * ... * prd[lprd-1] */
void
accumulate_fast_end (polymodF_t *prd, const poly_t F, unsigned long lprd)
{
  unsigned long i;

  for (i = 1; i < lprd; i++)
    polymodF_mul(prd[0], prd[0], prd[i], F);
}

int main(int argc, char **argv) {
  FILE * depfile;
  cado_poly pol;
  poly_t F;
  polymodF_t prd, tmp;
  long a;
  unsigned long b;
  int ret, i;
  unsigned long p;

  if (argc != 3) {
    fprintf(stderr, "usage: %s depfile polyfile\n", argv[0]);
    exit(1);
  }

  depfile = fopen(argv[1], "r");
  ASSERT_ALWAYS(depfile != NULL);
  ret = read_polynomial(pol, argv[2]);
  ASSERT_ALWAYS(ret == 1);

  // Init F to be the algebraic polynomial
  poly_alloc(F, pol->degree);
  for (i = pol->degree; i >= 0; --i)
    poly_setcoeff(F, i, pol->f[i]);

  // Init prd to 1.
  poly_alloc(prd->p, pol->degree);
  mpz_set_ui(prd->p->coeff[0], 1);
  prd->p->deg = 0;
  prd->v = 0;

  // Allocate tmp
  poly_alloc(tmp->p, 1);

  // Accumulate product
  int nab = 0;
#if 0
  // Naive version, without subproduct tree
  while(fscanf(depfile, "%ld %lu", &a, &b) != EOF){
    if(!(nab % 100000))
      fprintf(stderr, "# Reading ab pair #%d at %2.2lf\n",nab,seconds());
    if((a == 0) && (b == 0))
      break;
    polymodF_from_ab(tmp, a, b);
    polymodF_mul(prd, prd, tmp, F);
    nab++;
  }
#else
  {
    polymodF_t *prd_tab;
    unsigned long lprd = 1; /* number of elements in prd_tab[] */
    unsigned long nprd = 0; /* number of accumulated products in prd_tab[] */
    prd_tab = (polymodF_t*) malloc (lprd * sizeof (polymodF_t));
    poly_alloc(prd_tab[0]->p, F->deg);
    mpz_set_ui(prd_tab[0]->p->coeff[0], 1);
    prd_tab[0]->p->deg = 0;
    prd_tab[0]->v = 0;
    while(fscanf(depfile, "%ld %lu", &a, &b) != EOF){
      if(!(nab % 100000))
	fprintf(stderr, "# Reading ab pair #%d at %2.2lf\n",nab,seconds());
      if((a == 0) && (b == 0))
	break;
      polymodF_from_ab(tmp, a, b);
      prd_tab = accumulate_fast (prd_tab, tmp, F, &lprd, nprd++);
      nab++;
    }
    accumulate_fast_end (prd_tab, F, lprd);

    poly_copy(prd->p, prd_tab[0]->p);
    prd->v = prd_tab[0]->v;
    for (i = 0; i < (long)lprd; ++i)
      poly_free(prd_tab[i]->p);
    free(prd_tab);
  }
#endif

  fprintf(stderr, "Finished accumulating the product in %2.2lf\n", seconds());
  fprintf(stderr, "v = %d\n", prd->v);
  fprintf(stderr, "sizeinbit of cst term = %ld\n", mpz_sizeinbase(prd->p->coeff[0], 2));
  fprintf(stderr, "sizeinbit of leading term = %ld\n", mpz_sizeinbase(prd->p->coeff[pol->degree-1], 2));

  p = FindSuitableModP(F);
  fprintf(stderr, "Using p=%lu for lifting\n", p);

  double tm = seconds();
  polymodF_sqrt(prd, prd, F, p);
  fprintf(stderr, "Square root lifted in %2.2lf\n", seconds()-tm);
  
  mpz_t algsqrt, aux;
  mpz_init(algsqrt);
  mpz_init(aux);
  poly_eval_mod_mpz(algsqrt, prd->p, pol->m, pol->n);
  mpz_invert(aux, F->coeff[F->deg], pol->n);  // 1/fd mod n
  mpz_powm_ui(aux, aux, prd->v, pol->n);      // 1/fd^v mod n
  mpz_mul(algsqrt, algsqrt, aux);
  mpz_mod(algsqrt, algsqrt, pol->n);

  gmp_printf("%Zd\n", algsqrt);

  mpz_clear(aux);
  mpz_clear(algsqrt);
  poly_free(F);
  poly_free(prd->p);
  poly_free(tmp->p);
  return 0;
}

