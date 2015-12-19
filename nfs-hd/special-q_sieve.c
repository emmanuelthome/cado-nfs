#include "cado.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <inttypes.h>
#include "utils.h"
#include "makefb.h"
#include "vector.h"
#include "int64_poly.h"
#include "mat_int64.h"
#include "array.h"
#include "uint64_array.h"
#include "utils_mpz.h"
#include "utils_int64.h"
#include "utils_lattice.h"
#include "mat_double.h"
#include "double_poly.h"
#include <time.h>
#include <limits.h>
#include "utils_norm.h"
#include "utils_cofactorisation.h"
#include "ecm/facul.h"

//TODO: change % in test + add or substract.

/*
 * Different mode:
   - TRACE_POS: follow all the operation make in a case of the array;
   - NUMBER_HIT: count the number of hit;
   - MEAN_NORM_BOUND: mean of the initialized norms;
   - MEAN_NORM_BOUND_SIEVE: mean of the log2 of the residual norms;
   - MEAN_NORM: mean of the norm we can expected if the initialisation compute
      the true norms;
   - …
 */

/* ----- Compute Mq, Mqr, Tqr ----- */

/*
 * Build the matrix Mq for an ideal (q, g) with deg(g) = 1. The matrix need Tq
 *  with the good coefficients, i.e. t the coeffecients not reduced modulo q.
 *  Suppose that the matrix is good initialized. Can not verify if ideal->Tq
 *  and matrix has the good size.
 *
 * matrix: the matrix with the good initialization for the number of rows and
 * columns. We can not assert here if the numbers of values for Tq and matrix is
 *  good or not.
 * ideal: the ideal (q, g) with deg(g) = 1.
 */
void build_Mq_ideal_1(mat_Z_ptr matrix, ideal_1_srcptr ideal)
{
  ASSERT(matrix->NumRows == matrix->NumCols);

  //Reinitialize the coefficients.
  for (unsigned int row = 1; row <= matrix->NumRows; row++) {
    for (unsigned int col = 1; col <= matrix->NumCols; col++) {
      mat_Z_set_coeff_uint64(matrix, 0, row, col);
    }
  }
  //Coeffecients of the diagonal.
  mat_Z_set_coeff_uint64(matrix, ideal->ideal->r, 1, 1);
  for (unsigned int row = 2; row <= matrix->NumRows; row++) {
    mat_Z_set_coeff_uint64(matrix, 1, row, row);
  }
  //Coefficients of the first line.
  for (unsigned int col = 2; col <= matrix->NumCols; col++) {
    mat_Z_set_coeff(matrix, ideal->Tr[col - 2], 1, col);
  }
}

/*
 * Build the matrix Mq for an ideal (q, g) with deg(g) > 1. The matrix need Tq
 *  with the good coefficients, i.e. t the coeffecients not reduced modulo q.
 *  Suppose that the matrix is good initialized. Can not verify if ideal->Tq
 *  and matrix has the good size.
 *
 * matrix: Mq.
 * ideal: an ideal (q, g) with deg(g) > 1.
 */
void build_Mq_ideal_u(mat_Z_ptr matrix, ideal_u_srcptr ideal)
{
  ASSERT(matrix->NumRows == matrix->NumCols);

  for (unsigned int row = 1; row <= matrix->NumRows; row++) {
    for (unsigned int col = 1; col <= matrix->NumCols; col++) {
      mat_Z_set_coeff_uint64(matrix, 0, row, col);
    }
  }
  for (int row = 1; row <= ideal->ideal->h->deg; row++) {
    mat_Z_set_coeff_uint64(matrix, ideal->ideal->r, row, row);
  }
  for (int row = ideal->ideal->h->deg + 1; row <= (int)matrix->NumRows;
       row++) {
    mat_Z_set_coeff_uint64(matrix, 1, row, row);
  }
  for (int col = ideal->ideal->h->deg + 1; col <= (int)matrix->NumCols;
       col++) {
    for (int row = 1; row <= ideal->ideal->h->deg; row++) {
      mat_Z_set_coeff(matrix,
          ideal->Tr[row - 1][col - ideal->ideal->h->deg + 1], row, col);
    }
  }
}

/*
 * Build the matrix Mq for an ideal (q, g). The matrix need Tq
 *  with the good coefficients, i.e. t the coeffecients not reduced modulo q.
 *  Suppose that the matrix is good initialized. Can not verify if ideal->Tq
 *  and matrix has the good size.
 *
 * matrix: Mq.
 * ideal: an ideal (q, g) with deg(g) > 1.
 */
void build_Mq_ideal_spq(mat_Z_ptr matrix, ideal_spq_srcptr ideal)
{
  ASSERT(matrix->NumRows == matrix->NumCols);

  if (ideal->type == 0) {
    build_Mq_ideal_1(matrix, ideal->ideal_1);
  } else if (ideal->type == 1) {
    build_Mq_ideal_u(matrix, ideal->ideal_u);
  }
}

void reorganize_MqLLL(mat_Z_ptr matrix)
{
  for (unsigned int col = 1; col <= matrix->NumRows; col++) {
    if (mpz_cmp_ui(matrix->coeff[matrix->NumRows][col], 0) < 0) {
      for (unsigned int row = 1; row <= matrix->NumRows; row++) {
        mpz_mul_si(matrix->coeff[row][col], matrix->coeff[row][col], -1);
      }
    }
  }

  //TODO: seem to be useless, need do to think about that.
  /*mat_Z_sort_last(matrix, matrix);*/
}

/*
 * pseudo_Tqr is the normalised Tqr obtained by compute_Tqr_1. If Tqr[0] != 0,
 *  pseudo_Tqr = [(-Tqr[0])^-1 mod r = a, a * Tqr[1], …].
 *
 * pseudo_Tqr: a matrix (a line here) obtained as describe above.
 * Tqr: the Tqr matrix.
 * t: dimension of the lattice.
 * ideal: the ideal r.
 */
void compute_pseudo_Tqr_1(uint64_t * pseudo_Tqr, uint64_t * Tqr,
    unsigned int t, ideal_1_srcptr ideal)
{
  unsigned int i = 0;
  while (Tqr[i] == 0 && i < t) {
    pseudo_Tqr[i] = 0;
    i++;
  }
  uint64_t inverse = ideal->ideal->r - 1;

  ASSERT(Tqr[i] == 1);
  ASSERT(inverse ==
         invmod_uint64((uint64_t)(-Tqr[i] + (int64_t)ideal->ideal->r),
                       ideal->ideal->r));
  pseudo_Tqr[i] = inverse;
  for (unsigned int j = i + 1; j < t; j++) {
    pseudo_Tqr[j] = (inverse * Tqr[j]) % ((int64_t)ideal->ideal->r);
  }
}

/*
 * Mqr is the a matrix that can generate vector c such that Tqr * c = 0 mod r.
 * Mqr = [[r a 0 … 0]
 *       [0 1 b … 0]
 *       [| | | | |]
 *       [0 0 0 0 1]]
 *
 * Mqr: the Mqr matrix.
 * Tqr: the Tqr matrix.
 * t: dimension of the lattice.
 * ideal: the ideal r.
 */
void compute_Mqr_1(mat_int64_ptr Mqr, uint64_t * Tqr, unsigned int t,
    ideal_1_srcptr ideal)
{
  ASSERT(Mqr->NumRows == t);
  ASSERT(Mqr->NumCols == t);

  unsigned int index = 0;
  while (Tqr[index] == 0) {
    index++;
  }

  mat_int64_set_zero(Mqr);
  for (unsigned int i = 1; i <= t; i++) {
    Mqr->coeff[i][i] = 1;
  }
  Mqr->coeff[index + 1][index + 1] = ideal->ideal->r;
  for (unsigned int col = index + 2; col <= t; col++) {
    if (Tqr[col-1] != 0) {
      Mqr->coeff[index + 1][col] =
        (-(int64_t)Tqr[col-1]) + (int64_t)ideal->ideal->r;
    }
#ifndef NDEBUG
    else {
      Mqr->coeff[index + 1][col] = 0;
    }
#endif // NDEBUG
  }
}

/*
 * Compute Tqr for r an ideal of degree 1 and normalised it.
 *
 * Tqr: the Tqr matrix (Tqr is a line).
 * matrix: the MqLLL matrix.
 * t: dimension of the lattice.
 * ideal: the ideal r.
 */
void compute_Tqr_1(uint64_t * Tqr, mat_Z_srcptr matrix,
    unsigned int t, ideal_1_srcptr ideal)
{
  mpz_t tmp;
  mpz_init(tmp);
  unsigned int i = 0;
  Tqr[i] = 0;
  mpz_t invert;
  mpz_init(invert);

  //Tqr = Mq,1 - Tr * Mq,2.
  for (unsigned int j = 0; j < t; j++) {
    mpz_set(tmp, matrix->coeff[1][j + 1]);
    for (unsigned int k = 0; k < t - 1; k++) {
      mpz_submul(tmp, ideal->Tr[k], matrix->coeff[k + 2][j + 1]);
    }
    if (Tqr[i] == 0) {
      mpz_mod_ui(tmp, tmp, ideal->ideal->r);
      if (mpz_cmp_ui(tmp, 0) != 0) {
        mpz_invert_ui(tmp, tmp, ideal->ideal->r);
        mpz_set(invert, tmp);
        Tqr[j] = 1;
        i = j;
      } else {
        Tqr[j] = 0;
      }
    } else {
      mpz_mul(tmp, tmp, invert);
      mpz_mod_ui(tmp, tmp, ideal->ideal->r);
      Tqr[j] = mpz_get_ui(tmp);
    }
  }

  mpz_clear(tmp);
  mpz_clear(invert);
}

/*
 * Generate the Mqr matrix, r is an ideal of degree 1.
 *
 * Mqr: the matrix.
 * Tqr: the Tqr matrix (Tqr is a line).
 * t: dimension of the lattice.
 * ideal: the ideal r.
 */
void generate_Mqr_1(mat_int64_ptr Mqr, uint64_t * Tqr, ideal_1_srcptr ideal,
    MAYBE_UNUSED unsigned int t)
{
  ASSERT(Tqr[0] != 0);
  ASSERT(t == Mqr->NumCols);
  ASSERT(t == Mqr->NumRows);

#ifndef NDEBUG
  for (unsigned int row = 1; row <= Mqr->NumRows; row++) {
    for (unsigned int col = 1; col <= Mqr->NumCols; col++) {
      ASSERT(Mqr->coeff[row][col] == 0);
    }
  }
#endif // NDEBUG

  Mqr->coeff[1][1] = ideal->ideal->r;
  for (unsigned int col = 2; col <= Mqr->NumRows; col++) {
    if (Tqr[col] == 0) {
      Mqr->coeff[1][col] = 0;
    } else {
      Mqr->coeff[1][col] = (-Tqr[col - 1]) + ideal->ideal->r;
    }
    ASSERT(Mqr->coeff[col][1] < (int64_t)ideal->ideal->r);
  }

  for (unsigned int row = 2; row <= Mqr->NumRows; row++) {
    Mqr->coeff[row][row] = 1;
  }
}

/*
 * WARNING: Need to assert if the function does what it is supposed to do.
 * Compute Tqr for an ideal (r, h) with deg(h) > 1.
 *
 * Tqr: Tqr is a matrix in this case.
 * matrix: MqLLL.
 * t: dimension of the lattice.
 * ideal: (r, h).
 */
void compute_Tqr_u(mpz_t ** Tqr, mat_Z_srcptr matrix, unsigned int t,
    ideal_u_srcptr ideal)
{
  /* Tqr = Mq,1 - Tr * Mq,2. */
  for (int row = 0; row < ideal->ideal->h->deg; row++) {
    for (unsigned int col = 0; col < t; col++) {
      mpz_init(Tqr[row][col]);
      mpz_set(Tqr[row][col], matrix->coeff[row + 1][col + 1]);
      for (int k = 0; k < (int)t - ideal->ideal->h->deg; k++) {
        mpz_submul(Tqr[row][col], ideal->Tr[row][k],
            matrix->coeff[k + ideal->ideal->h->deg + 1][col + 1]);
      }
      mpz_mod_ui(Tqr[row][col], Tqr[row][col], ideal->ideal->r);
    }
  }
}

/* ----- Sieving part ----- */

#ifdef ASSERT_SIEVE
/*
 * To assert that the ideal is a factor of a = matrix * c mapped in the number
 *  field defined by f.
 *
 * H: the sieving bound.
 * index: index of c in the array in which we store the norm.
 * number_element: number of element contains in the sieving region.
 * matrix: MqLLL.
 * f: the polynomial that defines the number field.
 * ideal: the ideal involved in the factorisation in ideals.
 * c: the vector corresponding to the ith value of the array in which we store
 *  the norm.
 * pos: 1 if index must increase, 0 otherwise.
 */
void assert_sieve(sieving_bound_srcptr H, uint64_t index,
    uint64_t number_element, mat_Z_srcptr matrix, mpz_poly_srcptr f,
    ideal_1_srcptr ideal, int64_vector_srcptr c, unsigned int pos,
    uint64_t * index_old)
{
  /*
   * Verify if we do not sieve the same index, because we do not sieve the power
   *  of an ideal.
   */
  if (* index_old == 0) {
    ASSERT(* index_old <= index);
  } else {
    if (pos == 1) {
      ASSERT(* index_old < index);
    } else {
      ASSERT(* index_old > index);
    }
  }
  * index_old = index;

  mpz_vector_t v;
  mpz_vector_init(v, H->t);
  mpz_poly_t a;
  mpz_poly_init(a, -1);
  mpz_t res;
  mpz_init(res);

  array_index_mpz_vector(v, index, H, number_element);
  mat_Z_mul_mpz_vector_to_mpz_poly(a, matrix, v);
  norm_poly(res, a, f);
  //Verify if res = r * …
  ASSERT(mpz_congruent_ui_p(res, 0, ideal->ideal->r));

  //Verify c and v are the same.
  if (c != NULL) {
    mpz_vector_t v_tmp;
    mpz_vector_init(v_tmp, c->dim);
    int64_vector_to_mpz_vector(v_tmp, c);
    ASSERT(mpz_vector_cmp(v, v_tmp) == 0);
    mpz_vector_clear(v_tmp);
  }

  //Verify if h divides a mod r.
  if (a->deg != -1) {
    mpz_poly_t r;
    mpz_poly_init(r, -1);
    mpz_poly_t q;
    mpz_poly_init(q, -1);
    mpz_t p;
    mpz_init(p);
    mpz_set_uint64(p, ideal->ideal->r);

    ASSERT(mpz_poly_div_qr(q, r, a, ideal->ideal->h, p));
    ASSERT(r->deg == -1);
    mpz_poly_clear(r);
    mpz_poly_clear(q);
    mpz_clear(p);
  }

  mpz_clear(res);
  mpz_poly_clear(a);
  mpz_vector_clear(v);
}
#endif // ASSERT_SIEVE

#ifdef TRACE_POS
/*
 * Say all the ideal we delete in the ith position.
 *
 * index: index in array we want to follow.
 * ideal: the ideal we delete to the norm.
 * array: the array in which we store the resulting norm.
 */
void trace_pos_sieve(FILE * file_trace_pos, uint64_t index, ideal_1_srcptr ideal,
    array_srcptr array)
{
  if (index == TRACE_POS) {
    fprintf(file_trace_pos, "The ideal is: ");
    ideal_fprintf(file_trace_pos, ideal->ideal);
    fprintf(file_trace_pos, "log: %f\n", ideal->log);
    fprintf(file_trace_pos, "The new value of the norm is %u.\n", array->array[index]);
  }
}
#endif // TRACE_POS

/*
 * All the mode we can active during the sieving step.
 *
 * H: the sieving bound.
 * index: index of the array in which we store the resulting norm.
 * array: the array in which we store the resulting norm.
 * matrix: MqLLL.
 * f: the polynomial that defines the number field.
 * ideal: the ideal we want to delete.
 * c: the vector corresponding to indexth position in array.
 * number_c_l: number of possible c with the same ci, ci+1, …, ct. (cf line_sieve_ci)
 * nbint: say if we have already count the number_c_l or not.
 * pos: 1 if index must increase, 0 otherwise.
 */

static inline void mode_sieve(MAYBE_UNUSED FILE * file_trace_pos,
    MAYBE_UNUSED sieving_bound_srcptr H, MAYBE_UNUSED uint64_t index,
    MAYBE_UNUSED array_srcptr array, MAYBE_UNUSED mat_Z_srcptr matrix,
    MAYBE_UNUSED mpz_poly_srcptr f, MAYBE_UNUSED ideal_1_srcptr ideal,
    MAYBE_UNUSED int64_vector_srcptr c, MAYBE_UNUSED uint64_t number_c_l,
    MAYBE_UNUSED unsigned int nbint, MAYBE_UNUSED unsigned int pos,
    MAYBE_UNUSED uint64_t * nb_hit, MAYBE_UNUSED uint64_t * index_old)
{
#ifdef ASSERT_SIEVE
  assert_sieve(H, index, array->number_element, matrix, f, ideal, c, pos,
      index_old);
#endif // ASSERT_SIEVE

#ifdef NUMBER_HIT
  if (nbint) {
    * nb_hit = * nb_hit + number_c_l;
  }
#endif // NUMBER_HIT

#ifdef TRACE_POS
  trace_pos_sieve(file_trace_pos, index, ideal, array);
#endif // TRACE_POS
}

/* ----- Line sieve algorithm ----- */

/*
 * Sieve for a special-q of degree 1 and Tqr with a zero coefficient at the
 *  first place. This function sieve a c in the q lattice.
 *
 * array: in which we store the norms.
 * c: element of the q lattice.
 * ideal: an ideal with r < q.
 * ci: the possible first coordinate of c to have c in the sieving region.
 * H: the sieving bound.
 * i: index of the first non-zero coefficient in pseudo_Tqr.
 * number_c_l: number of possible c with the same ci, ci+1, …, ct.
 */
void line_sieve_ci(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    int64_vector_ptr c, ideal_1_srcptr ideal,
    int64_t ci, sieving_bound_srcptr H, unsigned int i, uint64_t number_c_l,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit, MAYBE_UNUSED uint64_t * index_old)
{
#ifndef NDEBUG
  if (i == 0) {
    ASSERT(number_c_l == 1);
  } else {
    ASSERT(number_c_l % 2 == 0);
  }
#endif // NDEBUG

  uint64_t index = 0;

  if (ci < (int64_t)H->h[i]) {
    //Change the ith coordinate of c.
    int64_vector_setcoordinate(c, i, ci);

    index = array_int64_vector_index(c, H, array->number_element);
    array->array[index] = array->array[index] - (unsigned char)ideal->log;

#ifndef MODE_SIEVE_LINE
    mode_sieve(file_trace_pos, H, index, array, matrix, f, ideal, c, number_c_l,
        1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_LINE

    for (uint64_t k = 1; k < number_c_l; k++) {
      array->array[index + k] = array->array[index + k] -
        (unsigned char)ideal->log;

#ifndef MODE_SIEVE_LINE
      mode_sieve(file_trace_pos, H, index + k, array, matrix, f, ideal, NULL,
          number_c_l, 0, 1, nb_hit, index_old);
#endif // MODE_SIEVE_LINE
    }

    int64_t tmp = ci;
    tmp = tmp + (int64_t)ideal->ideal->r;

    while(tmp < (int64_t)H->h[i]) {

      index = index + ideal->ideal->r * number_c_l;

      array->array[index] = array->array[index] - (unsigned char)ideal->log;

#ifndef MODE_SIEVE_LINE
      mode_sieve(file_trace_pos, H, index, array, matrix, f, ideal, NULL,
          number_c_l, 1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_LINE

      for (uint64_t k = 1; k < number_c_l; k++) {
        array->array[index + k] = array->array[index + k] -
          (unsigned char)ideal->log;

#ifndef MODE_SIEVE_LINE
        mode_sieve(file_trace_pos, H, index + k, array, matrix, f, ideal, NULL,
            number_c_l, 1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_LINE
      }

      tmp = tmp + (int64_t)ideal->ideal->r;
    }
  }
}

void compute_ci_1(int64_t * ci, unsigned int i, unsigned int pos,
    uint64_t * pseudo_Tqr, uint64_t ideal_r, sieving_bound_srcptr H)
{
  for (unsigned int j = i + 1; j < pos; j++) {
    * ci = * ci - ((int64_t)pseudo_Tqr[j] * (2 * (int64_t)H->h[j] - 1));
    if (* ci >= (int64_t) ideal_r) {
      while(* ci >= (int64_t)ideal_r) {
        * ci = * ci - (int64_t)ideal_r;
      }
    } else if (* ci < 0) {
      while(* ci < (int64_t)ideal_r) {
        * ci = * ci + (int64_t)ideal_r;
      }
      * ci = * ci - (int64_t)ideal_r;
    }
  }
  * ci = * ci + pseudo_Tqr[pos];
  if (* ci >= (int64_t)ideal_r) {
    * ci = * ci - (int64_t)ideal_r;
  }
  if (* ci < 0) {
    * ci = * ci + (int64_t)ideal_r;
  }
  ASSERT(* ci >= 0 && * ci < (int64_t)ideal_r);
}

/*
 *
 *
 * array: the array in which we store the resulting norms.
 * c: the
 */
void line_sieve_1(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    int64_vector_ptr c, uint64_t * pseudo_Tqr,
    ideal_1_srcptr ideal, sieving_bound_srcptr H, unsigned int i,
    uint64_t number_c_l, int64_t * ci, unsigned int pos,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit, MAYBE_UNUSED uint64_t * index_old)
{
  ASSERT(pos >= i);

  //Recompute ci.
  compute_ci_1(ci, i, pos, pseudo_Tqr, ideal->ideal->r, H);

  //Compute the lowest value such that Tqr*(c[0], …, c[i], …, c[t-1]) = 0 mod r.
  //lb: the minimal accessible value in the sieving region for this coordinate.
  int64_t lb = 0;
  if (i < H->t - 1) {
    lb =  -(int64_t)H->h[i];
  }
  int64_t k = (lb - * ci) / (-(int64_t) ideal->ideal->r);
  if ((lb - * ci) > 0) {
    k--;
  }
  int64_t ci_tmp = -k * (int64_t)ideal->ideal->r + * ci;
  ASSERT(ci_tmp >= lb);

#ifndef NDEBUG
  int64_t tmp_ci = ci_tmp;
  tmp_ci = tmp_ci % (int64_t) ideal->ideal->r;
  if (tmp_ci < 0) {
    tmp_ci = tmp_ci + (int64_t) ideal->ideal->r;
  }
  int64_t tmp = 0;

  for (unsigned int j = i + 1; j < H->t; j++) {
    tmp = tmp + ((int64_t)pseudo_Tqr[j] * c->c[j]);
    tmp = tmp % (int64_t) ideal->ideal->r;
    if (tmp < 0) {
      tmp = tmp + (int64_t) ideal->ideal->r;
    }
  }
  ASSERT(tmp == tmp_ci);
#endif // NDEBUG

  line_sieve_ci(array, file_trace_pos, c, ideal, ci_tmp, H, i, number_c_l,
      matrix, f, nb_hit, index_old);
}

/* ----- Plane sieve algorithm ----- */

/*
 * Perform the plane sieve in all the plane z = v->c[2].
 * Two different signature: if PLANE_SIEVE_STARTING_POINT is set, vs can be
 *  updated if an other vector has an x coordinate closer to zero.
 *
 * array: the array in which we store the norm.
 * vs: starting point vector.
 * e0: a vector of the Franke-Kleinjung algorithm, e0->c[0] < 0.
 * e1: a vector of the Franke-Kleinjung algorithm, e1->c[0] > 0.
 * coord_e0: deplacement in array given by e0.
 * coord_e1: deplacement in array given by e1.
 * H: sieving bound that give the sieving region.
 * r: the ideal we consider.
 * matrix: MqLLL.
 * f: the polynomial that defines the number field.
 */
#ifdef PLANE_SIEVE_STARTING_POINT
void plane_sieve_1_enum_plane(array_ptr array,
    MAYBE_UNUSED FILE * file_trace_pos, int64_vector_ptr vs,
    int64_vector_srcptr e0, int64_vector_srcptr e1, uint64_t coord_e0,
    uint64_t coord_e1, sieving_bound_srcptr H, ideal_1_srcptr r,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit)
#else
void plane_sieve_1_enum_plane(array_ptr array,
    MAYBE_UNUSED FILE * file_trace_pos, int64_vector_srcptr vs,
    int64_vector_srcptr e0, int64_vector_srcptr e1, uint64_t coord_e0,
    uint64_t coord_e1, sieving_bound_srcptr H, ideal_1_srcptr r,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit)
#endif // PLANE_SIEVE_STARTING_POINT
{
  int64_vector_t v;
  int64_vector_init(v, vs->dim);
  int64_vector_set(v, vs);

#ifdef PLANE_SIEVE_STARTING_POINT
  int64_vector_t vs_tmp;
  int64_vector_init(vs_tmp, vs->dim);
  int64_vector_set(vs_tmp, vs);
#endif // PLANE_SIEVE_STARTING_POINT

  unsigned int FK_value = 0;
  //1 if we have v was in the sieving region, 0 otherwise.
  unsigned int flag_sr = 0;
  uint64_t index_v = 0;

  //Perform Franke-Kleinjung enumeration.
  //x increases.

  uint64_t index_old = 0;

  while (v->c[1] < (int64_t)H->h[1]) {
    if (v->c[1] >= -(int64_t)H->h[1]) {
      if (!flag_sr) {
        ASSERT(flag_sr == 0);
        index_v = array_int64_vector_index(v, H, array->number_element);
        flag_sr = 1;
      } else {
        ASSERT(flag_sr == 1);
        if (FK_value == 0) {
          index_v = index_v + coord_e0;
        } else if (FK_value == 1) {
          index_v = index_v + coord_e1;
        } else {
          ASSERT(FK_value == 2);
          index_v = index_v + coord_e0 + coord_e1;
        }
      }
      array->array[index_v] = array->array[index_v] - (unsigned char)r->log;

#ifndef MODE_SIEVE_PLANE
      mode_sieve(file_trace_pos, H, index_v, array, matrix, f, r, v, 1, 1, 1,
          nb_hit, &index_old);
#endif // MODE_SIEVE_PLANE

    }

#ifdef PLANE_SIEVE_STARTING_POINT
    if (ABS(v->c[1]) < ABS(vs_tmp->c[1])) {
      int64_vector_set(vs_tmp, v);
    }
#endif // PLANE_SIEVE_STARTING_POINT

    FK_value = enum_pos_with_FK(v, v, e0, e1, -(int64_t)H->h[0], 2 * (int64_t)
        H->h[0]);
  }

  //x decreases.

  index_old = 0;

  flag_sr = 0;
  int64_vector_set(v, vs);
  FK_value = enum_neg_with_FK(v, v, e0, e1, -(int64_t)H->h[0] + 1, 2 *
      (int64_t)H->h[0]);
  while (v->c[1] >= -(int64_t)H->h[1]) {
    if (v->c[1] < (int64_t)H->h[1]) {
      if (!flag_sr) {
        index_v = array_int64_vector_index(v, H, array->number_element);
        flag_sr = 1;
      } else {
        if (FK_value == 0) {
          index_v = index_v - coord_e0;
        } else if (FK_value == 1) {
          index_v = index_v - coord_e1;
        } else {
          ASSERT(FK_value == 2);
          index_v = index_v - coord_e0 - coord_e1;
        }
      }
      array->array[index_v] = array->array[index_v] - (unsigned char)r->log;

#ifndef MODE_SIEVE_PLANE
      mode_sieve(file_trace_pos, H, index_v, array, matrix, f, r, v, 1, 1, 0,
          nb_hit, &index_old);
#endif // PLANE_SIEVE_STARTING_POINT

    }
#ifdef PLANE_SIEVE_STARTING_POINT
    if (ABS(v->c[1]) < ABS(vs_tmp->c[1])) {
      int64_vector_set(vs_tmp, v);
    }
#endif // PLANE_SIEVE_STARTING_POINT
    FK_value = enum_neg_with_FK(v, v, e0, e1, -(int64_t)H->h[0] + 1, 2 *
        (int64_t) H->h[0]);
  }
#ifdef PLANE_SIEVE_STARTING_POINT
  int64_vector_set(vs, vs_tmp);
  int64_vector_clear(vs_tmp);
#endif // PLANE_SIEVE_STARTING_POINT

  int64_vector_clear(v);
}

void plane_sieve_1_enum_plane_ortho(array_ptr array,
    MAYBE_UNUSED FILE * file_trace_pos, int64_vector_ptr vs,
    uint64_t coord_e1, sieving_bound_srcptr H, ideal_1_srcptr r,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit)
{
  ASSERT(vs->c[1] == 0);

  int64_vector_t v;
  int64_vector_init(v, vs->dim);
  int64_vector_set(v, vs);

  if (v->c[0] >= (int64_t)H->h[0] || v->c[0] < -(int64_t)H->h[0]) {
#ifndef NDEBUG
    if (v->c[0] < -(int64_t)H->h[0]) {
      int64_t k = (int64_t) ceil((-(double)H->h[0] - (double)v->c[0]) /
          (double)r->ideal->r);
      v->c[0] = k * (int64_t)r->ideal->r + v->c[0];

      ASSERT(v->c[0] >= -(int64_t)H->h[0]);
    } else if (v->c[0] >= (int64_t)H->h[0]) {
      int64_t k = (int64_t) floor(((double)H->h[0] - (double)v->c[0]) /
          (double)r->ideal->r);
      v->c[0] = k * (int64_t)r->ideal->r + v->c[0];

      ASSERT(v->c[0] < (int64_t)H->h[0]);
    }
#endif // NDEBUG

    int64_vector_clear(v);
    return;
  }


#ifdef PLANE_SIEVE_STARTING_POINT
  int64_vector_t vs_tmp;
  int64_vector_init(vs_tmp, vs->dim);
  int64_vector_set(vs_tmp, vs);
#endif // PLANE_SIEVE_STARTING_POINT

  uint64_t index_old = 0;

  v->c[1] = -(int64_t) H->h[1];
  uint64_t index_v = array_int64_vector_index(v, H, array->number_element);
  array->array[index_v] = array->array[index_v] - r->log;

#ifndef MODE_SIEVE_PLANE
  mode_sieve(file_trace_pos, H, index_v, array, matrix, f, r, v, 1, 1, 0,
      nb_hit, &index_old);
#endif // MODE_SIEVE_PLANE

  for (int64_t i = -(int64_t) H->h[1] + 1; i < (int64_t) H->h[1]; i++) {
    index_v = index_v + coord_e1;
    v->c[1] = i;
    array->array[index_v] = array->array[index_v] - r->log;
#ifndef MODE_SIEVE_PLANE
    mode_sieve(file_trace_pos, H, index_v, array, matrix, f, r, v, 1, 1, 0,
        nb_hit, &index_old);
#endif // MODE_SIEVE_PLANE
  }

  int64_vector_clear(v);
}

void find_new_vs(int64_vector_ptr vs, list_int64_vector_ptr v_refresh,
    list_int64_vector_t * SV, int64_vector_srcptr e0, int64_vector_srcptr e1,
    sieving_bound_srcptr H, int boolean)
{
#ifndef NDEBUG
  int64_t tmp = 0;
  int64_t tmp2 = 0;
  for (unsigned int j = 2; j < vs->dim; j++) {
    tmp = tmp + vs->c[j];
    tmp2 = tmp2 + H->h[j];
  }
  ASSERT(tmp <= tmp2);
#endif

  unsigned int k = 2;
  while(k < vs->dim) {
    if (vs->c[k] == H->h[k] - 1) {
      int64_vector_set(vs, v_refresh->v[k - 2]);
      k++;
    } else {
      break;
    }
  }
  if (k < vs->dim) {
    plane_sieve_next_plane(vs, SV[k - 2], e0, e1, H, boolean);
    if (k > 2) {
      for (unsigned int i = 0; i < k - 2; i++) {
        int64_vector_set(v_refresh->v[i], vs);
      }
    }
  }

#ifndef NDEBUG
  if (vs->c[vs->dim - 1] == H->h[vs->dim - 1]) {
    for (unsigned int j = 2; j < vs->dim - 1; j++) {
      tmp = vs->c[j];
      ASSERT(tmp == 0);
    }
  } else {
    for (unsigned int j = 2; j < vs->dim - 1; j++) {
      tmp = vs->c[j];
      ASSERT(tmp >= -(int64_t)H->h[j]);
      ASSERT(tmp < (int64_t)H->h[j]);
    }
    tmp = vs->c[vs->dim - 1];
    ASSERT(tmp >= 0);
    ASSERT(tmp < H->h[vs->dim - 1]);
  }
#endif
}

/*
 * Plane sieve.
 *
 * array: the array in which we store the norms.
 * r: the ideal we consider.
 * Mqr: the Mqr matrix.
 * H: the sieving bound that defines the sieving region.
 * matrix: MqLLL.
 * f: polynomial that defines the number field.
 */
void plane_sieve_1(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    ideal_1_srcptr r, mat_int64_srcptr Mqr, sieving_bound_srcptr H,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit)
{
  ASSERT(Mqr->NumRows == Mqr->NumCols);

  //Perform the Franke-Kleinjung algorithm.
  int64_vector_t * vec = malloc(sizeof(int64_vector_t) * Mqr->NumRows);
  for (unsigned int i = 0; i < Mqr->NumCols; i++) {
    int64_vector_init(vec[i], Mqr->NumRows);
    mat_int64_extract_vector(vec[i], Mqr, i);
  }
  int64_vector_t e0;
  int64_vector_t e1;
  int64_vector_init(e0, vec[0]->dim);
  int64_vector_init(e1, vec[1]->dim);
  int boolean = reduce_qlattice(e0, e1, vec[0], vec[1], (int64_t)(2*H->h[0]));

  if (!boolean) {
#ifdef PLANE_SIEVE_ORTHO
    fprintf(stderr, "# Plane sieve does not support this type of Mqr.\n");
    mat_int64_fprintf_comment(stderr, Mqr);
    for (unsigned int i = 0; i < Mqr->NumCols; i++) {
      int64_vector_clear(vec[i]);
    }
    free(vec);
    int64_vector_clear(e0);
    int64_vector_clear(e1);
    return;
#else // PLANE_SIEVE_ORTHO
    ASSERT(boolean == 0);

    int64_vector_set(e0, vec[0]);
    int64_vector_set(e1, vec[1]);
#endif // PLANE_SIEVE_ORTHO
  }

  //Find some short vectors to go from z = d to z = d + 1.
  //TODO: go after boolean, no?
  list_int64_vector_t * SV = (list_int64_vector_t *)
    malloc(sizeof(list_int64_vector_t) * (Mqr->NumCols - 2));
  for (unsigned int i = 2; i < Mqr->NumCols; i++) {
    list_int64_vector_init(SV[i - 2], Mqr->NumCols);
    SV4(SV[i - 2], vec[0], vec[1], vec[i]);
  }


  for (unsigned int i = 0; i < Mqr->NumCols; i++) {
    int64_vector_clear(vec[i]);
  }
  free(vec);

  int64_vector_t vs;
  int64_vector_init(vs, e0->dim);
  int64_vector_set_zero(vs);

  if (H->t >= 3) {
    for (unsigned int i = 2; i < H->t - 1; i++) {
      int64_vector_addmul(vs, vs, SV[i - 2]->v[0], -(int64_t) H->h[i]);
    }
    int64_vector_addmul(vs, vs, SV[0]->v[0], -1);
    plane_sieve_next_plane(vs, SV[0], e0, e1, H, boolean);
  }

  list_int64_vector_t v_refresh;
  list_int64_vector_init(v_refresh, e0->dim);

  uint64_t coord_e0 = 0, coord_e1 = 0;
  if (boolean) {
    coordinate_FK_vector(&coord_e0, &coord_e1, e0, e1, H,
        array->number_element);
  } else {
    coord_e1 = index_vector(e1, H, array->number_element);
  }

  //Iterate from H[2] to H[t - 1].
  //Enumerate the element of the sieving region.
  uint64_t size = 1;

  for (unsigned int i = 0; i < H->t - 2; i++) {
    list_int64_vector_add_int64_vector(v_refresh, vs);
  }

  for (unsigned int i = 2; i < H->t - 1; i++) {
    size = size * (2 * (uint64_t) H->h[i]);
  }
  size = size * ((uint64_t) H->h[H->t - 1]);

  for (uint64_t d = 0; d < size; d++) {
    if (boolean == 0) {
      plane_sieve_1_enum_plane_ortho(array, file_trace_pos, vs, coord_e1, H, r,
          matrix, f, nb_hit);
    } else {
      plane_sieve_1_enum_plane(array, file_trace_pos, vs, e0, e1, coord_e0,
          coord_e1, H, r, matrix, f, nb_hit);
    }

    //TODO: it is probably possible to better write find_new_vs.
    //Jump in the next plane.
    find_new_vs(vs, v_refresh, SV, e0, e1, H, boolean);
  }

  for (unsigned int i = 0; i < Mqr->NumRows - 2; i++) {
    list_int64_vector_clear(SV[i]);
  }
  free(SV);
  int64_vector_clear(vs);
  int64_vector_clear(e0);
  int64_vector_clear(e1);
  list_int64_vector_clear(v_refresh);
}

/* ----- Space sieve algorithm ----- */

void space_sieve_1_plane(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    MAYBE_UNUSED uint64_t * nbhit,
    list_int64_vector_ptr list_s, int64_vector_srcptr s,
    ideal_1_srcptr r, list_int64_vector_index_srcptr list_vec_zero,
    uint64_t index_s, sieving_bound_srcptr H, MAYBE_UNUSED mat_Z_srcptr matrix,
    MAYBE_UNUSED mpz_poly_srcptr f, MAYBE_UNUSED uint64_t * nb_hit,
    MAYBE_UNUSED uint64_t * index_old)
{
  ASSERT(int64_vector_equal(s, list_s->v[0]));

  int64_vector_t v_tmp;
  int64_vector_init(v_tmp, s->dim);

  for (unsigned int i = 0; i < list_vec_zero->length; i++) {
    int64_vector_add(v_tmp, s, list_vec_zero->v[i]->vec);

    uint64_t index_tmp = index_s;

    while (int64_vector_in_sieving_region(v_tmp, H)) {

#ifdef SPACE_SIEVE_CUT_EARLY
      * nbhit = * nbhit + 1;
#endif // SPACE_SIEVE_CUT_EARLY

      list_int64_vector_add_int64_vector(list_s, v_tmp);

      if (list_vec_zero->v[i]->vec->c[1] < 0) {
        index_tmp = index_tmp - list_vec_zero->v[i]->index;
#ifndef MODE_SIEVE_SPACE
        mode_sieve(file_trace_pos, H, index_tmp, array, matrix, f, r, v_tmp,
            1, 1, 0, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE
      } else {
        index_tmp = index_tmp + list_vec_zero->v[i]->index;
#ifndef MODE_SIEVE_SPACE
        mode_sieve(file_trace_pos, H, index_tmp, array, matrix, f, r, v_tmp,
            1, 1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE
      }
      array->array[index_tmp] = array->array[index_tmp] - (unsigned char)r->log;

      int64_vector_add(v_tmp, v_tmp, list_vec_zero->v[i]->vec);
    }
    int64_vector_sub(v_tmp, s, list_vec_zero->v[i]->vec);

    index_tmp = index_s;

    * index_old = index_s;

    while (int64_vector_in_sieving_region(v_tmp, H)) {

#ifdef SPACE_SIEVE_CUT_EARLY
      * nbhit = * nbhit + 1;
#endif // SPACE_SIEVE_CUT_EARLY

      list_int64_vector_add_int64_vector(list_s, v_tmp);

      if (list_vec_zero->v[i]->vec->c[1] < 0) {
        index_tmp = index_tmp + list_vec_zero->v[i]->index;
#ifndef MODE_SIEVE_SPACE
        mode_sieve(file_trace_pos, H, index_tmp, array, matrix, f, r, v_tmp, 1,
            1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE
      } else {
        index_tmp = index_tmp - list_vec_zero->v[i]->index;

#ifndef MODE_SIEVE_SPACE
        mode_sieve(file_trace_pos, H, index_tmp, array, matrix, f, r, v_tmp, 1,
            1, 0, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE
      }
      array->array[index_tmp] = array->array[index_tmp] - (unsigned char)r->log;

      int64_vector_sub(v_tmp, v_tmp, list_vec_zero->v[i]->vec);
    }
  }

  int64_vector_clear(v_tmp);
}

void space_sieve_1_next_plane(array_ptr array, uint64_t * index_s,
    MAYBE_UNUSED FILE * file_trace_pos,
    list_int64_vector_ptr list_s, unsigned int s_change, int64_vector_srcptr s,
    list_int64_vector_index_srcptr list_vec, unsigned int index_vec,
    ideal_1_srcptr r, sieving_bound_srcptr H, MAYBE_UNUSED mat_Z_srcptr matrix,
    MAYBE_UNUSED mpz_poly_srcptr f, MAYBE_UNUSED uint64_t * nb_hit,
    MAYBE_UNUSED uint64_t * index_old)
{
  if (s->c[2] < (int64_t)H->h[2]) {
#ifdef SPACE_SIEVE_CUT_EARLY
    nb_hit++;
#endif // SPACE_SIEVE_CUT_EARLY

    list_int64_vector_delete_elements(list_s);
    list_int64_vector_add_int64_vector(list_s, s);

    if (!s_change) {
      if (list_vec->v[index_vec]->index == 0) {
        list_vec->v[index_vec]->index =
          index_vector(list_vec->v[index_vec]->vec, H,
              array->number_element);
      }
      * index_s = * index_s + list_vec->v[index_vec]->index;
    } else {
      ASSERT(s_change == 1);

      * index_s = array_int64_vector_index(s, H, array->number_element);
    }
    array->array[* index_s] = array->array[* index_s] - (unsigned char)r->log;

#ifndef MODE_SIEVE_SPACE
    mode_sieve(file_trace_pos, H, * index_s, array, matrix, f, r, s, 1,
        1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE
  }
}

void space_sieve_1_plane_sieve(array_ptr array,
    list_int64_vector_ptr list_s,MAYBE_UNUSED uint64_t * nbhit,
    MAYBE_UNUSED unsigned int * entropy, MAYBE_UNUSED FILE * file_trace_pos,
    int64_vector_ptr s,
    uint64_t * index_s, list_int64_vector_index_ptr list_vec,
    MAYBE_UNUSED list_int64_vector_index_ptr list_vec_zero, ideal_1_srcptr r,
    mat_int64_srcptr Mqr, list_int64_vector_srcptr list_SV,
    list_int64_vector_srcptr list_FK, sieving_bound_srcptr H,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit, MAYBE_UNUSED uint64_t * index_old)
{
  int64_vector_t s_out;
  int64_vector_init(s_out, s->dim);
  int64_vector_t v_new;
  int64_vector_init(v_new, s->dim);
  plane_sieve_1_incomplete(s_out, s, Mqr, H, list_FK, list_SV);
  if (int64_vector_in_sieving_region(s_out, H)) {

#ifdef SPACE_SIEVE_CUT_EARLY
    * nbhit = * nbhit + 1;
#endif // SPACE_SIEVE_CUT_EARLY

    list_int64_vector_delete_elements(list_s);
    list_int64_vector_add_int64_vector(list_s, s_out);
    int64_vector_sub(v_new, s_out, s);
    uint64_t index_new = index_vector(v_new, H, array->number_element);
    list_int64_vector_index_add_int64_vector_index(list_vec, v_new,
        index_new);

#ifdef SPACE_SIEVE_ENTROPY
    //TODO: avoid duplicate.
    if (* entropy < SPACE_SIEVE_ENTROPY) {
      space_sieve_generate_new_vectors(list_vec, list_vec_zero, H,
          array->number_element);
      * entropy = * entropy + 1;

#ifdef SPACE_SIEVE_REMOVE_DUPLICATE
      list_int64_vector_index_remove_duplicate_sort(list_vec);
#endif // SPACE_SIEVE_REMOVE_DUPLICATE

    } else {
      list_int64_vector_index_sort_last(list_vec);
    }
#else
    list_int64_vector_index_sort_last(list_vec);
#endif // SPACE_SIEVE_ENTROPY

    * index_s = * index_s + index_new;
    array->array[* index_s] = array->array[* index_s] - (unsigned char)r->log;

#ifndef MODE_SIEVE_SPACE
    mode_sieve(file_trace_pos, H, * index_s, array, matrix, f, r, s_out, 1,
        1, 1, nb_hit, index_old);
#endif // MODE_SIEVE_SPACE

  }

  int64_vector_set(s, s_out);
  int64_vector_clear(s_out);
  int64_vector_clear(v_new);
}

//TODO: in many function, a v_tmp is used, try to have only one.
void space_sieve_1(array_ptr array, FILE * file_trace_pos, ideal_1_srcptr r,
    mat_int64_srcptr Mqr, sieving_bound_srcptr H, FILE * errstd,
    MAYBE_UNUSED mat_Z_srcptr matrix, MAYBE_UNUSED mpz_poly_srcptr f,
    MAYBE_UNUSED uint64_t * nb_hit)
{
  //For SPACE_SIEVE_ENTROPY
  MAYBE_UNUSED unsigned int entropy = 0;
  //For SPACE_SIEVE_CUT_EARLY
  MAYBE_UNUSED uint64_t nbhit = 0;

  uint64_t index_old = 0;

#ifdef SPACE_SIEVE_CUT_EARLY
  double expected_hit = (double)(4 * H->h[0] * H->h[1] * H->h[2]) /
    (double)r->ideal->r;
#endif // SPACE_SIEVE_CUT_EARLY

  /*
   * The two list contain vector in ]-2*H0, 2*H0[x]-2*H1, 2*H1[x[0, H2[.
   * list_vec contains vector with a last non-zero coordinate.
   * list_vec_zero contains vector with a last coordinate equals to zero.
   */
  list_int64_vector_index_t list_vec;
  list_int64_vector_index_init(list_vec, 3);
  list_int64_vector_index_t list_vec_zero;
  list_int64_vector_index_init(list_vec_zero, 3);

  //List that contain the vector for the plane sieve.
  list_int64_vector_t list_FK;
  list_int64_vector_init(list_FK, 3);
  list_int64_vector_t list_SV;
  list_int64_vector_init(list_SV, 3);

  //List that contain the current set of point with the same z coordinate.
  list_int64_vector_t list_s;
  list_int64_vector_init(list_s, 3);

  unsigned int vector_1 = space_sieve_1_init(list_vec, list_vec_zero, r, Mqr,
      H, array->number_element);

  //0 if plane sieve is already done, 1 otherwise.
  int plane_sieve = 0;

  //s = 0, starting point.
  int64_vector_t s;
  int64_vector_init(s, Mqr->NumRows);
  int64_vector_set_zero(s);

#ifdef SPACE_SIEVE_CUT_EARLY
  nbhit++;
#endif // SPACE_SIEVE_CUT_EARLY

  //Compute the index of s in array.
  uint64_t index_s = array_int64_vector_index(s, H, array->number_element);
  //TODO: not necessary to do that.
  array->array[index_s] = array->array[index_s] - (unsigned char)r->log;

#ifndef MODE_SIEVE_SPACE
  mode_sieve(file_trace_pos, H, index_s, array, matrix, f, r, s, 1, 1, 1,
      nb_hit, &index_old);
#endif // MODE_SIEVE_SPACE


  //s is in the list of current point.
  list_int64_vector_add_int64_vector(list_s, s);

  while (s->c[2] < (int64_t)H->h[2]) {

    /*
     * if there are vectors in list_vec_zero, use it to find other point with
     * the same z coordinate as s.
     */
    space_sieve_1_plane(array, file_trace_pos, &nbhit, list_s, s, r,
        list_vec_zero, index_s, H, matrix, f, nb_hit, &index_old);

    //index_vec: index of the vector we used to go to the other plane in list_s
    unsigned int index_vec = 0;
    //s has changed, compare the s in space_sieve_1_plane.
    unsigned int s_change = 0;

    //s_tmp: TODO
    int64_vector_t s_tmp;
    int64_vector_init(s_tmp, s->dim);

    //Find the next element, with the smallest reachable z.
    //hit is set to 1 if an element is reached.
    unsigned int hit = space_sieve_1_next_plane_seek(s_tmp, &index_vec,
        &s_change, list_s, list_vec, H, s);

    if (hit) {
      int64_vector_set(s, s_tmp);
      space_sieve_1_next_plane(array, &index_s, file_trace_pos, list_s,
          s_change, s, list_vec, index_vec, r, H, matrix, f, nb_hit,
          &index_old);
    }
#ifdef SPACE_SIEVE_CUT_EARLY
    double err_rel = ((double)expected_hit - (double)nbhit) / (double)nbhit;
    if (!hit && err_rel >= SPACE_SIEVE_CUT_EARLY && 0 <= err_rel) {
      //}
#else
    if (!hit) {
#endif // SPACE_SIEVE_CUT_EARLY
      ASSERT(hit == 0);

      //Need to do plane sieve to
      if (!plane_sieve) {
        ASSERT(plane_sieve == 0);
        int boolean = space_sieve_1_plane_sieve_init(list_SV, list_FK,
            list_vec, list_vec_zero, r, H, Mqr, vector_1,
            array->number_element);

        if (!boolean) {
          ASSERT(boolean == 0);

          fprintf(errstd,
              "# Plane sieve (called by space sieve) does not support this type of Mqr.\n");
          mat_int64_fprintf_comment(errstd, Mqr);

          int64_vector_clear(s);
          int64_vector_clear(s_tmp);
          list_int64_vector_clear(list_s);
          list_int64_vector_index_clear(list_vec);
          list_int64_vector_index_clear(list_vec_zero);
          list_int64_vector_clear(list_FK);
          list_int64_vector_clear(list_SV);

          return;
        }
        plane_sieve = 1;
      }

      space_sieve_1_plane_sieve(array, list_s, &nbhit, &entropy, file_trace_pos,
          s, &index_s, list_vec, list_vec_zero, r, Mqr, list_SV, list_FK, H,
          matrix, f, nb_hit, &index_old);
    }
#ifdef SPACE_SIEVE_CUT_EARLY
    else if (!hit) {
      ASSERT(err_rel <= SPACE_SIEVE_CUT_EARLY);

      int64_vector_clear(s_tmp);
      break;
    }
#endif // SPACE_SIEVE_CUT_EARLY
    int64_vector_clear(s_tmp);
  }

  int64_vector_clear(s);
  list_int64_vector_clear(list_s);
  list_int64_vector_index_clear(list_vec);
  list_int64_vector_index_clear(list_vec_zero);
  list_int64_vector_clear(list_FK);
  list_int64_vector_clear(list_SV);
}

/* ----- Enumeration algorithm ----- */
//TODO: this part is porbably broken.

/*
 * Compute sum(m[j][i] * x^j, j > i).
 *
 * x: the coefficients of a linear combination of the vector of a basis of the
 *  lattice we enumerate.
 * m: the \mu{i, j} matrix given by Gram-Schmidt orthogonalisation.
 * i: index.
 */
double sum_mi(int64_vector_srcptr x, mat_double_srcptr m, unsigned int i)
{
  double sum = 0;
  for (unsigned int j = i + 1; j < x->dim; j++) {
    sum = sum + (double)x->c[j] * m->coeff[j + 1][i + 1];
  }
  return sum;
}

/*
 * Compute sum(l_j, j >= i.
 *
 * l:
 * i: index.
 */
double sum_li(double_vector_srcptr l, unsigned int i)
{
  double sum = 0;
  for (unsigned int j = i; j < l->dim; j++) {
    sum = sum + l->c[j];
  }
  return sum;
}

/*
 * Construct v from x (the coefficients of the linear combination of the vector
 *  of a basis of the lattice) and list (a basis of the lattice).
 *
 * v: the vector we build.
 * x: the coefficients.
 * list: a basis of the lattice.
 */
void construct_v(int64_vector_ptr v, int64_vector_srcptr x,
    list_int64_vector_srcptr list)
{
  ASSERT(x->dim == list->length);
  ASSERT(v->dim == list->v[0]->dim);

  for (unsigned int i = 0; i < v->dim; i++) {
    for (unsigned int j = 0; j < x->dim; j++) {
      v->c[i] = v->c[i] + x->c[j] * list->v[j]->c[i];
    }
  }
}

/*
 * An enumeration algorithm of the element of the lattice generated by Mqr,
 *  which are in a sphere (see page 164 of "Algorithms for the Shortest and Closest
 *  Lattice Vector Problems" by Guillaume Hanrot, Xavier Pujol, and Damien Stehlé,
 *  IWCC 2011).
 *
 * array: the array in which we store the norms.
 * ideal: the ideal we consider.
 * Mqr: the Mqr matrix.
 * H: the sieving bound that generates a sieving region.
 * f: the polynomial that defines the number field.
 * matrix: MqLLL.
 */
void enum_lattice(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    ideal_1_srcptr ideal,
    mat_int64_srcptr Mqr, sieving_bound_srcptr H,
    MAYBE_UNUSED mpz_poly_srcptr f, MAYBE_UNUSED mat_Z_srcptr matrix,
    MAYBE_UNUSED uint64_t * nb_hit)
{
  ASSERT(Mqr->NumRows == Mqr->NumCols);
  ASSERT(Mqr->NumRows == 3);
  ASSERT(H->t == Mqr->NumRows);

  //Original basis of the lattice.
  list_int64_vector_t b_root;
  list_int64_vector_init(b_root, 3);
  list_int64_vector_extract_mat_int64(b_root, Mqr);

  /* To compute and store the result of Gram-Schmidt. */
  list_double_vector_t list_e;
  list_double_vector_init(list_e);
  list_double_vector_extract_mat_int64(list_e, Mqr);

  //Matrix with the coefficient mu_{i, j}.
  mat_double_t M;
  mat_double_init(M, Mqr->NumRows, Mqr->NumCols);
  mat_double_set_zero(M);

  //Gram Schmidt orthogonalisation.
  list_double_vector_t list;
  list_double_vector_init(list);
  double_vector_gram_schmidt(list, M, list_e);

  /* Compute the square of the L2 norm for all the Gram-Schmidt vectors. */
  double_vector_t b;
  double_vector_init(b, list->length);
  for (unsigned int i = 0; i < b->dim; i++) {
    b->c[i] = double_vector_norml2sqr(list->v[i]);
  }

  /* Center of the cuboid. */
  double_vector_t t;
  double_vector_init(t, Mqr->NumRows);
  double_vector_set_zero(t);
  t->c[t->dim - 1] = round((double) H->h[t->dim - 1] / 2);

  //This A is equal to the A^2 in the paper we cite.
  double A = 0;
  for (unsigned int i = 0; i < t->dim - 1; i++) {
    A = A + (double)(H->h[0] * H->h[0]);
  }
  A = A + (t->c[t->dim - 1] * t->c[t->dim - 1]);

  //Verify if the matrix M has the good properties.
#ifdef NDEBUG
  for (unsigned int j = 0; j < list->v[0]->dim; j++) {
    if (j == 0) {
      ASSERT(0 != list->v[0]->c[j]);
    } else {
      ASSERT(0 == list->v[0]->c[j]);
    }
  }
  for(unsigned int i = 1; i < list->length; i++) {
    for (unsigned int j = 0; j < list->v[i]->dim; j++) {
      if (j == i) {
        ASSERT(1 == list->v[i]->c[j]);
      } else {
        ASSERT(0 == list->v[i]->c[j]);
      }
    }
  }
#endif

  //Coefficient of t in the Gram-Schmidt basis.
  double_vector_t ti;
  double_vector_init(ti, t->dim);
  double_vector_set_zero(ti);
  ti->c[ti->dim - 1] = t->c[ti->dim - 1];

  //The vector in the classical basis.
  int64_vector_t x;
  int64_vector_init(x, list->length);
  int64_vector_set_zero(x);
  x->c[x->dim - 1] = (int64_t) ceil(ti->c[x->dim - 1] -
      sqrt(A) / sqrt(b->c[x->dim - 1]));
  unsigned int i = list->length - 1;

  double_vector_t l;
  double_vector_init(l, x->dim);
  double_vector_set_zero(l);

  //TODO: is it true else if? is it true sqrt(A) in the last condition?
  while (i < list->length) {
    double tmp = (double)x->c[i] - ti->c[i] + sum_mi(x, M, i);
    l->c[i] = (tmp * tmp) * b->c[i];

    if (i == 0 && sum_li(l, 0) <= A) {
      int64_vector_t v_h;
      int64_vector_init(v_h, x->dim);
      int64_vector_set_zero(v_h);
      //x * orig_base.
      construct_v(v_h, x, b_root);
      if (int64_vector_in_sieving_region(v_h, H)) {
        uint64_t index =
          array_int64_vector_index(v_h, H, array->number_element);
        array->array[index] = array->array[index] - (unsigned char)ideal->log;

        uint64_t index_old = 0;
        mode_sieve(file_trace_pos, H, index, array, matrix, f, ideal, v_h, 1, 1,
            1, nb_hit, &index_old);
      }
      int64_vector_clear(v_h);
      x->c[0] = x->c[0] + 1;
    } else if (i != 0 && sum_li(l, i) <= A) {
      i = i - 1;
      x->c[i] = (int64_t)ceil(ti->c[i] -
          sum_mi(x, M, i) - sqrt((A - sum_li(l, i + 1)) / b->c[i]));
    } else if (sum_li(l, i)> sqrt(A)) {
      i = i + 1;
      if (i < list->length) {
        x->c[i] = x->c[i] + 1;
      }
    }
  }

  double_vector_clear(l);
  double_vector_clear(b);
  int64_vector_clear(x);
  double_vector_clear(ti);
  mat_double_clear(M);
  double_vector_clear(t);
  list_double_vector_clear(list);
  list_double_vector_clear(list_e);
  list_int64_vector_clear(b_root);
}

/*
 *
 *
 * array: the array in which we store the resulting norms.
 * matrix: MqLLL.
 * fb: factor base of this side.
 * H: sieving bound.
 * f: the polynomial that defines the number field.
 */
void special_q_sieve(array_ptr array, MAYBE_UNUSED FILE * file_trace_pos,
    mat_Z_srcptr matrix, factor_base_srcptr fb, sieving_bound_srcptr H,
    MAYBE_UNUSED mpz_poly_srcptr f, MAYBE_UNUSED FILE * outstd, FILE * errstd,
    uint64_t sieve_start, MAYBE_UNUSED ideal_spq_srcptr special_q)
{
#ifdef TIME_SIEVES
  double time_line_sieve = 0;
  uint64_t ideal_line_sieve = 0;
  double time_plane_sieve = 0;
  uint64_t ideal_plane_sieve = 0;
  double time_space_sieve = 0;
  uint64_t ideal_space_sieve = 0;
#endif // TIME_SIEVES

  MAYBE_UNUSED uint64_t number_hit = 0;

  ideal_1_t r;
  ideal_1_init(r);

  uint64_t * Tqr = (uint64_t *) malloc(sizeof(uint64_t) * (H->t));

#ifdef TIME_SIEVES
  time_line_sieve = seconds();
#endif // TIME_SIEVES

  uint64_t i = 0;
  while (fb->factor_base_1[i]->ideal->r < sieve_start) {
    i++;
  }

#ifdef Q_BELOW_FBB
  uint64_t q = ideal_spq_get_q(special_q);
  mpz_poly_t g;
  mpz_poly_init(g, ideal_spq_get_deg_g(special_q));
  ideal_spq_get_g(g, special_q);
#endif // Q_BELOW_FBB

  /* --- Line sieve --- */

  uint64_t line_sieve_stop = 2 * (uint64_t) H->h[0];

  while (i < fb->number_element_1 &&
      fb->factor_base_1[i]->ideal->r < line_sieve_stop) {

#ifdef Q_BELOW_FBB
    if (fb->factor_base_1[i]->ideal->r == q) {
      if (!mpz_poly_cmp(g, fb->factor_base_1[i]->ideal->h)) {
        i++;
        continue;
      }
    }
#endif // Q_BELOW_FBB

    ideal_1_set(r, fb->factor_base_1[i], H->t);

#ifdef ASSERT_SIEVE
    printf("# Line sieve.\n");
    printf("# ");
    ideal_fprintf(stdout, r->ideal);
    /*fprintf(stdout, "# Tqr = [");*/
    /*for (unsigned int i = 0; i < H->t - 1; i++) {*/
    /*fprintf(stdout, "%" PRIu64 ", ", Tqr[i]);*/
    /*}*/
    /*fprintf(stdout, "%" PRIu64 "]\n", Tqr[H->t - 1]);*/
#endif // ASSERT_SIEVE

    //Compute the true Tqr
    compute_Tqr_1(Tqr, matrix, H->t, r);

    uint64_t * pseudo_Tqr = (uint64_t *) malloc(sizeof(uint64_t) * (H->t));
    //Compute a usefull pseudo_Tqr for line_sieve.
    compute_pseudo_Tqr_1(pseudo_Tqr, Tqr, H->t, r);

    //Initialise c = (-H[0], -H[1], …, 0).
    int64_vector_t c;
    int64_vector_init(c, H->t);
    for (unsigned int j = 0; j < H->t - 1; j++) {
      int64_vector_setcoordinate(c, j, -(int64_t)H->h[j]);
    }
    int64_vector_setcoordinate(c, H->t - 1, 0);

    unsigned int index = 0;
    while (pseudo_Tqr[index] == 0) {
      index++;
    }

#ifndef NDEBUG
    if (index == 0) {
      ASSERT(pseudo_Tqr[0] != 0);
    }
#endif // NDEBUG

    uint64_t index_old = 0;

    if (index + 1 != c->dim) {

      int64_vector_setcoordinate(c, index + 1, c->c[index + 1] - 1);

      //Compute ci = (-Tqr[index])^(-1) * (Tqr[index + 1]*c[1] + …) mod r.
      //TODO: modify that.
      int64_t ci = 0;

      for (unsigned int j = index + 1; j < H->t; j++) {
        ci = ci + (int64_t)pseudo_Tqr[j] * c->c[j];
        if (ci >= (int64_t) r->ideal->r) {
          while(ci >= (int64_t)r->ideal->r) {
            ci = ci - (int64_t)r->ideal->r;
          }
        } else if (ci < 0) {
          while(ci < (int64_t)r->ideal->r) {
            ci = ci + (int64_t)r->ideal->r;
          }
          ci = ci - (int64_t)r->ideal->r;
        }
      }
      ASSERT(ci >= 0 && ci < (int64_t)r->ideal->r);
      uint64_t number_c = array->number_element;
      uint64_t number_c_l = 1;
      //Number of c with the same c[index + 1], …, c[t-1].
      for (unsigned int i = 0; i < index + 1; i++) {
        number_c = number_c / (2 * H->h[i]);
        number_c_l = number_c_l * (2 * H->h[i]);
      }
      number_c_l = number_c_l / (2 * H->h[index]);

      for (uint64_t j = 0; j < number_c; j++) {
        unsigned int pos = int64_vector_add_one_i(c, index + 1, H);
        line_sieve_1(array, file_trace_pos, c, pseudo_Tqr, r, H, index,
            number_c_l, &ci, pos, matrix, f, &number_hit, &index_old);
      }
    } else {
#ifndef NDEBUG
      for (unsigned int i = 0; i < index; i++) {
        ASSERT(pseudo_Tqr[i] == 0);
      }
      ASSERT(pseudo_Tqr[index] != 0);
#endif // NDEBUG

      uint64_t number_c_l = 1;
      for (unsigned int i = 0; i < index; i++) {
        number_c_l = number_c_l * (2 * H->h[i]);
      }
      line_sieve_ci(array, file_trace_pos, c, r, 0, H, index, number_c_l,
          matrix, f, &number_hit, &index_old);
    }

#ifdef NUMBER_HIT
    printf("# Number of hits: %" PRIu64 " for r: %" PRIu64 ", h: ",
        number_hit, r->ideal->r);
    mpz_poly_fprintf(stdout, r->ideal->h);
    printf("# Estimated number of hits: %u.\n",
        (unsigned int) nearbyint((double) array->number_element /
          (double) r->ideal->r));
    number_hit = 0;
#endif // NUMBER_HIT

    free(pseudo_Tqr);
    int64_vector_clear(c);

    i++;
  }

#ifdef TIME_SIEVES
  time_line_sieve = seconds() - time_line_sieve;
  ideal_line_sieve = i;
#endif // TIME_SIEVES

#ifdef TIME_SIEVES
  time_plane_sieve = seconds();
#endif // TIME_SIEVES

  /* --- Plane sieve --- */

  mat_int64_t Mqr;
  mat_int64_init(Mqr, H->t, H->t);

  uint64_t plane_sieve_stop = fb->number_element_1;
  if (H->t == 3) {
    plane_sieve_stop = 4 * (int64_t)(H->h[0] * H->h[1]);
  }
  while (i < fb->number_element_1 &&
      fb->factor_base_1[i]->ideal->r < plane_sieve_stop) {

#ifdef Q_BELOW_FBB
    if (fb->factor_base_1[i]->ideal->r == q) {
      if (!mpz_poly_cmp(g, fb->factor_base_1[i]->ideal->h)) {
        i++;
        continue;
      }
    }
#endif // Q_BELOW_FBB

    ideal_1_set(r, fb->factor_base_1[i], H->t);

#ifdef ASSERT_SIEVE
    printf("# Plane sieve.\n");
    printf("# ");
    ideal_fprintf(stdout, r->ideal);
    /*fprintf(stdout, "# Tqr = [");*/
    /*for (unsigned int i = 0; i < H->t - 1; i++) {*/
    /*fprintf(stdout, "%" PRIu64 ", ", Tqr[i]);*/
    /*}*/
    /*fprintf(stdout, "%" PRIu64 "]\n", Tqr[H->t - 1]);*/
#endif // ASSERT_SIEVE

    //Compute the true Tqr
    compute_Tqr_1(Tqr, matrix, H->t, r);

#ifdef TEST_MQR
    printf("# Tqr = [");
    for (unsigned int i = 0; i < H->t - 1; i++) {
      printf("%" PRIu64 ", ", Tqr[i]);
    }
    printf("%" PRIu64 "]\n# Mqr =\n", Tqr[H->t - 1]);
    mat_int64_t Mqr_test;
    mat_int64_init(Mqr_test, H->t, H->t);
    compute_Mqr_1(Mqr_test, Tqr, H->t, r);
    mat_int64_fprintf_comment(stdout, Mqr_test);
    mat_int64_clear(Mqr_test);
#endif // TEST_MQR

    compute_Mqr_1(Mqr, Tqr, H->t, r);

    if (Mqr->coeff[1][1] != 1) {
      plane_sieve_1(array, file_trace_pos, r, Mqr, H, matrix, f, &number_hit);
    } else {
      fprintf(errstd, "# Tqr = [");
      for (unsigned int i = 0; i < H->t - 1; i++) {
        fprintf(errstd, "%" PRIu64 ", ", Tqr[i]);
      }
      fprintf(errstd, "%" PRIu64 "]\n", Tqr[H->t - 1]);
      fprintf(errstd, "# Plane sieve does not support this type of Mqr.\n");
      mat_int64_fprintf_comment(errstd, Mqr);
    }

#ifdef NUMBER_HIT
    printf("# Number of hits: %" PRIu64 " for r: %" PRIu64 ", h: ",
        number_hit, r->ideal->r);
    mpz_poly_fprintf(stdout, r->ideal->h);
    printf("# Estimated number of hits: %u.\n",
        (unsigned int) nearbyint((double) array->number_element /
          (double) r->ideal->r));
    number_hit = 0;
#endif // NUMBER_HIT

    i++;
  }

#ifdef TIME_SIEVES
  time_plane_sieve = seconds() - time_plane_sieve;
  ideal_plane_sieve = i - ideal_line_sieve;
#endif // TIME_SIEVES

  /* --- Space sieve --- */

#ifdef TIME_SIEVES
  time_space_sieve = seconds();
#endif // TIME_SIEVES

  while (i < fb->number_element_1) {

#ifdef Q_BELOW_FBB
    if (fb->factor_base_1[i]->ideal->r == q) {
      if (!mpz_poly_cmp(g, fb->factor_base_1[i]->ideal->h)) {
        i++;
        continue;
      }
    }
#endif // Q_BELOW_FBB

    ideal_1_set(r, fb->factor_base_1[i], H->t);

#ifdef ASSERT_SIEVE
    printf("# Space sieve.\n");
    printf("# ");
    ideal_fprintf(stdout, r->ideal);
    /*fprintf(stdout, "# Tqr = [");*/
    /*for (unsigned int i = 0; i < H->t - 1; i++) {*/
    /*fprintf(stdout, "%" PRIu64 ", ", Tqr[i]);*/
    /*}*/
    /*fprintf(stdout, "%" PRIu64 "]\n", Tqr[H->t - 1]);*/
#endif // ASSERT_SIEVE

    //Compute the true Tqr
    compute_Tqr_1(Tqr, matrix, H->t, r);

#ifdef TEST_MQR
    printf("# Tqr = [");
    for (unsigned int i = 0; i < H->t - 1; i++) {
      printf("%" PRIu64 ", ", Tqr[i]);
    }
    printf("%" PRIu64 "]\n# Mqr =\n", Tqr[H->t - 1]);
    mat_int64_t Mqr_test;
    mat_int64_init(Mqr_test, H->t, H->t);
    compute_Mqr_1(Mqr_test, Tqr, H->t, r);
    mat_int64_fprintf_comment(stdout, Mqr_test);
    mat_int64_clear(Mqr_test);
#endif // TEST_MQR

    ASSERT(H->t == 3);

    compute_Mqr_1(Mqr, Tqr, H->t, r);

    if (Mqr->coeff[1][1] != 1) {
      space_sieve_1(array, file_trace_pos, r, Mqr, H, errstd, matrix, f,
          &number_hit);
    } else {
      fprintf(errstd, "# Tqr = [");
      for (unsigned int i = 0; i < H->t - 1; i++) {
        fprintf(errstd, "%" PRIu64 ", ", Tqr[i]);
      }
      fprintf(errstd, "%" PRIu64 "]\n", Tqr[H->t - 1]);
      fprintf(errstd, "# Space sieve does not support this type of Mqr.\n");
      mat_int64_fprintf_comment(errstd, Mqr);
    }

#ifdef NUMBER_HIT
    printf("# Number of hits: %" PRIu64 " for r: %" PRIu64 ", h: ",
        number_hit, r->ideal->r);
    mpz_poly_fprintf(stdout, r->ideal->h);
    printf("# Estimated number of hits: %u.\n",
        (unsigned int) nearbyint((double) array->number_element /
          (double) r->ideal->r));
    number_hit = 0;
#endif // NUMBER_HIT

    i++;
  }

#ifdef TIME_SIEVES
  time_space_sieve = seconds() - time_space_sieve;
  ideal_space_sieve = i - ideal_line_sieve - ideal_space_sieve;
#endif // TIME_SIEVES

  mat_int64_clear(Mqr);
  ideal_1_clear(r, H->t);
  free(Tqr);

#ifdef TIME_SIEVES
  fprintf(outstd, "# Perform line sieve: %fs for %" PRIu64 " ideals, %fs per ideal.\n",
      time_line_sieve, ideal_line_sieve,
      time_line_sieve / (double)ideal_line_sieve);
  double time_per_ideal = 0.0;
  if (ideal_plane_sieve != 0) {
    time_per_ideal = time_plane_sieve / (double)ideal_plane_sieve;
  } else {
    time_per_ideal = 0.0;
  }
  fprintf(outstd, "# Perform plane sieve: %fs for %" PRIu64 " ideals, %fs per ideal.\n",
      time_plane_sieve, ideal_plane_sieve, time_per_ideal);
  if (ideal_space_sieve != 0) {
    time_per_ideal = time_space_sieve / (double)ideal_space_sieve;
  } else {
    time_per_ideal = 0.0;
  }
  fprintf(outstd, "# Perform space sieve: %fs for %" PRIu64 " ideals, %fs per ideal.\n",
      time_space_sieve, ideal_space_sieve, time_per_ideal);
#endif // TIME_SIEVES

#ifdef Q_BELOW_FBB
  mpz_poly_clear(g);
#endif // Q_BELOW_FBB
}

#if 0
#ifdef SIEVE_TQR
    else {
      unsigned int index = 1;
      while (pseudo_Tqr[index] == 0 && index < H->t) {
        index++;
      }

      if (index == H->t - 1) {
        /* uint64_t number_c_u = array->number_element; */
        /* uint64_t number_c_l = 1; */
        /* for (uint64_t j = 0; j < index; j++) { */
        /*   number_c_u = number_c_u / (2 * H->h[j] + 1); */
        /*   number_c_l = number_c_l * (2 * H->h[j] + 1); */
        /* } */

        /* line_sieve(array, c, r, 0, H, index, number_c_l, */
        /*          matrix, f); */
      } else {
        int64_t ci = 0;
        if (index + 1 < H->t - 1) {
          int64_vector_setcoordinate(c, index + 1, c->c[index + 1] - 1);
        } else if (index + 1 == H->t - 1) {
          int64_vector_setcoordinate(c, index + 1, -1);
        }

        if (index < H->t - 1) {
          for (unsigned int j = index + 1; j < H->t; j++) {
            ci = ci + (int64_t)pseudo_Tqr[j] * c->c[j];
            if (ci >= (int64_t)r->ideal->r || ci < 0) {
              ci = ci % (int64_t)r->ideal->r;
            }
          }
          if (ci < 0) {
            ci = ci + (int64_t)r->ideal->r;
          }
          ASSERT(ci >= 0);
        }

        uint64_t number_c_u = array->number_element;
        uint64_t number_c_l = 1;
        for (uint64_t j = 0; j < index; j++) {
          number_c_u = number_c_u / (2 * H->h[j]);
          number_c_l = number_c_l * (2 * H->h[j]);
        }
        number_c_u = number_c_u / (2 * H->h[index]);

        unsigned int pos = 0;
        pos = int64_vector_add_one_i(c, index + 1, H);
        line_sieve_1(array, c, pseudo_Tqr, r, H, index,
                number_c_l, &ci, pos, matrix, f);
        for (uint64_t j = 1; j < number_c_u; j++) {
          pos = int64_vector_add_one_i(c, index + 1, H);
          line_sieve_1(array, c, pseudo_Tqr, r, H, index,
                  number_c_l, &ci, pos, matrix, f);
        }
      }
    }
#endif // SIEVE_TQR

#ifdef SIEVE_U
  for (uint64_t i = 0; i < fb->number_element_u; i++) {
    mpz_t ** Tqr = (mpz_t **)
      malloc(sizeof(mpz_t *) * (fb->factor_base_u[i]->ideal->h->deg));
    for (int j = 0; j < fb->factor_base_u[i]->ideal->h->deg; j++) {
      Tqr[j] = (mpz_t *) malloc(sizeof(mpz_t) * (H->t));
    }

    compute_Tqr_u(Tqr, matrix, H->t, fb->factor_base_u[i]);

    mpz_vector_t c;
    mpz_vector_init(c, H->t);
    for (unsigned int j = 0; j < H->t - 1; j++) {
      mpz_vector_setcoordinate_si(c, j, -H->h[j]);
    }
    mpz_vector_setcoordinate_si(c, H->t - 1, 0);

    sieve_u(array, Tqr, fb->factor_base_u[i], c, H);

    for (uint64_t j = 1; j < array->number_element; j++) {
      mpz_vector_add_one(c, H);
      sieve_u(array, Tqr, fb->factor_base_u[i], c, H);
    }
    mpz_vector_clear(c);
    for (unsigned int col = 0; col < H->t; col++) {
      for (int row = 0; row < fb->factor_base_u[i]->ideal->h->deg;
           row++) {
        mpz_clear(Tqr[row][col]);
      }
    }
    for (int j = 0; j < fb->factor_base_u[i]->ideal->h->deg; j++) {
      free(Tqr[j]);
    }
    free(Tqr);
  }
#endif // SIEVE_U
#endif // Draft for special-q_sieve

/* ----- Collect indices with norm less or equal to threshold ----- */

void find_index(uint64_array_ptr indexes, array_srcptr array,
    unsigned char thresh)
{
  uint64_t ind = 0;
  for (uint64_t i = 0; i < array->number_element; i++) {

    if (array->array[i] <= thresh) {
      ASSERT(ind < indexes->length);
      indexes->array[ind] = i;
      ind++;
    }
  }
  uint64_array_realloc(indexes, ind);
}

#ifdef PRINT_ARRAY_NORM
void number_norm(FILE * file_array_norm, array_srcptr array, unsigned char max)
{
  uint64_t * tab = (uint64_t *) malloc(sizeof(uint64_t) * max);
  memset(tab, 0, sizeof(uint64_t) * max);
  for (uint64_t i = 0; i < array->number_element; i++) {
    tab[array->array[i]] = tab[array->array[i]] + 1;
  }
  for (unsigned int i = 0; i < (unsigned int) max; i++) {
    fprintf(file_array_norm, "%u: %" PRIu64 "\n", i, tab[i]);
  }
  free(tab);
}
#endif // PRINT_ARRAY_NORM

/* ----- Usage and main ----- */

/*
 * Return 0 if Ha is not defined (ie, Ha[i] = 0 for all i), 1 otherwise.
 */
int Ha_defined(sieving_bound_srcptr Ha)
{
  uint64_t tmp = 0;
  for (unsigned int i = 0; i < Ha->t; i++) {
    tmp += Ha->h[i];
  }
  if (tmp == 2 * Ha->t) {
    return 0;
  }
  return 1;
}

void declare_usage(param_list pl)
{
  param_list_decl_usage(pl, "H", "sieving region for c");
  param_list_decl_usage(pl, "V", "number of number field");
  param_list_decl_usage(pl, "fbb", "factor base bounds");
  param_list_decl_usage(pl, "thresh", "thresholds");
  param_list_decl_usage(pl, "lpb", "large prime bounds");
  param_list_decl_usage(pl, "poly", "path to the polynomial file");
  param_list_decl_usage(pl, "q_range", "range of the special-q");
  param_list_decl_usage(pl, "q_side", "side of the special-q");
  param_list_decl_usage(pl, "fb", "path to factor bases");
  param_list_decl_usage(pl, "main", "if MNFS-CM, main side");
  param_list_decl_usage(pl, "out", "path to output file");
  param_list_decl_usage(pl, "err", "path to error file");
  param_list_decl_usage(pl, "start", "value of first ideal r considered");
  param_list_decl_usage(pl, "Ha", "sieving region for a");
  param_list_decl_usage(pl, "base", "specify the bases");
  param_list_decl_usage(pl, "g", "");
}

/*
 * Initialise the parameters of the special-q sieve.
 *
 * f: the V functions to define the number fields.
 * fbb: the V factor base bounds.
 * t: dimension of the lattice.
 * H: sieving bound.
 * q_min: lower bound of the special-q range.
 * q_max: upper bound of the special-q range.
 * thresh: the V threshold.
 * lpb: the V large prime bounds.
 * array: array in which the norms are stored.
 * matrix: the Mq matrix (set with zero coefficients).
 * q_side: side of the special-q.
 * V: number of number fields.
 */
void initialise_parameters(int argc, char * argv[], cado_poly_ptr f,
    uint64_t ** fbb, factor_base_t ** fb, sieving_bound_ptr H,
    uint64_t ** q_range, unsigned char ** thresh, unsigned int ** lpb,
    array_ptr array, mat_Z_ptr matrix, unsigned int * q_side, unsigned int * V,
    int * main_side, double ** log2_base, FILE ** outstd, FILE ** errstd,
    uint64_t ** sieve_start, sieving_bound_ptr Ha, mpz_poly_ptr g)
{
  param_list pl;
  param_list_init(pl);
  declare_usage(pl);
  FILE * fpl;
  char * argv0 = argv[0];

  argv++, argc--;
  for( ; argc ; ) {
    if (param_list_update_cmdline(pl, &argc, &argv)) { continue; }

    /* Could also be a file */
    if ((fpl = fopen(argv[0], "r")) != NULL) {
      param_list_read_stream(pl, fpl, 0);
      fclose(fpl);
      argv++,argc--;
      continue;
    }

    fprintf(stderr, "Unhandled parameter %s\n", argv[0]);
    param_list_print_usage(pl, argv0, stderr);
    exit (EXIT_FAILURE);
  }


  unsigned int t;
  int * r;
  param_list_parse_int_list_size(pl, "H", &r, &t, ".,");
  ASSERT(t > 2);
  sieving_bound_init(H, t);
  for (unsigned int i = 0; i < t; i++) {
    sieving_bound_set_hi(H, i, (unsigned int) 1 << r[i]);
  }

  sieving_bound_init(Ha, t);
  t = 0;
  param_list_parse_int_list_size(pl, "Ha", &r, &t, ".,");
  if (!t) {
    for (unsigned int i = 0; i < H->t; i++) {
      sieving_bound_set_hi(Ha, i, 2);
    }
  } else {
    ASSERT(t == H->t);
    for (unsigned int i = 0; i < t; i++) {
      sieving_bound_set_hi(Ha, i, (unsigned int) 1 << r[i]);
    }
  }
  free(r);

  cado_poly_init(f);

  unsigned int size_path = 1024;
  char path [size_path];
  param_list_parse_string(pl, "poly", path, size_path);
  cado_poly_read(f, path);
  ASSERT(f->nb_polys >= 2);

  * V = (unsigned int) f->nb_polys;

  //TODO: something strange here.
  * fbb = (uint64_t *) malloc(sizeof(uint64_t) * (* V));
  * thresh = (unsigned char *) malloc(sizeof(unsigned char) * (* V));
  * fb = (factor_base_t *) malloc(sizeof(factor_base_t) * (* V));
  * lpb = (unsigned int *) malloc(sizeof(unsigned int) * (* V));
  * q_range = (uint64_t *) malloc(sizeof(uint64_t) * 2);
  * sieve_start = (uint64_t *) malloc(sizeof(uint64_t) * (* V));
  * log2_base = (double *) malloc(sizeof(double) * (* V));

  param_list_parse_uint64_list(pl, "fbb", * fbb, (size_t) * V, ",");

  param_list_parse_uint_list(pl, "lpb", * lpb, (size_t) * V, ",");

  /*for (unsigned int i = 0; i < * V; i++) {*/
  /*ASSERT(mpz_cmp_ui((*lpb)[i], (*fbb)[i]) >= 0);*/
  /*}*/

  path[0] = '\0';
  param_list_parse_string(pl, "out", path, size_path);
  if (path[0] == '\0') {
    * outstd = stdout;
  } else {
    * outstd = fopen(path, "w");
  }

  path[0] = '\0';
  param_list_parse_string(pl, "err", path, size_path);
  if (path[0] == '\0') {
    * errstd = stderr;
  } else {
    * errstd = fopen(path, "w");
  }

  param_list_parse_uchar_list(pl, "thresh", * thresh, (size_t) * V, ",");

  uint64_t number_element = sieving_bound_number_element(H);
  ASSERT(number_element >= 6);

  //TODO: q_side is an unsigned int.
  param_list_parse_int(pl, "q_side", (int *)q_side);
  ASSERT(* q_side < * V);

  param_list_parse_uint64_and_uint64(pl, "q_range", * q_range, ",");

#ifndef Q_BELOW_FBB
  ASSERT((* q_range)[0] > fbb[0][* q_side]);
#else // Q_BELOW_FBB
  fprintf(* outstd,
      "# Special q may be under fbb.\n");
#endif // Q_BELOW_FBB
  ASSERT((* q_range)[0] < (* q_range)[1]);

  mpz_poly_init(g, -1);
  param_list_parse_mpz_poly(pl, "g", g, ",");
  ASSERT(g->deg == -1 || g->deg > 0);

#ifndef NDEBUG
  if (g->deg > 0) {
    ASSERT((* q_range)[1] - (* q_range)[0] == 1);
  }
#endif // NDEBUG

  for (unsigned int j = 0; j < * V; j++) {
    (*log2_base)[j] = 0;
  }
  param_list_parse_double_list(pl, "base", * log2_base, (size_t) * V, ",");
  if ((*log2_base)[0] != 0) {
    for (unsigned int j = 0; j < * V; j++) {
      (*log2_base)[j] = log2((*log2_base)[j]);
    }
  } else {

    //TODO: constant here is hard-coded.
    double max_a = pow(1.075, (double)(H->t - 1)) * pow((double)*q_range[1],
        1.0 / (double)H->t);
#ifdef SKEW_LLL_SPQ
    if (Ha_defined(Ha)) {
      max_a = (double) (*q_range)[1];
    }
#endif // SKEW_LLL_SPQ

    double tmp = 0;
    for (unsigned int j = 0; j < H->t; j++) {
      tmp = tmp + (double)H->h[j];
    }
    max_a = tmp * max_a;

    for (unsigned int j = 0; j < * V; j++) {
      double precompute =
        pow((double)(f->pols[j]->deg + 1), (double)(H->t - 1) / 2.0) *
        pow((double)H->t, (double)f->pols[j]->deg / 2.0) *
        pow(max_a, (double)f->pols[j]->deg);

      mpz_t inf_f;
      mpz_init(inf_f);
      mpz_poly_infinity_norm(inf_f, f->pols[j]);
      (*log2_base)[j] = pow(mpz_get_d(inf_f), (double)(H->t - 1)) * precompute;
      mpz_clear(inf_f);

      ASSERT((*log2_base)[j] > 0.0);

      if (* q_side == j) {
        (*log2_base)[j] = (*log2_base)[j] / (double)(*q_range)[0];
      }

      (*log2_base)[j] = pow((*log2_base)[j], 1 / (double)(UCHAR_MAX - 2));
      (*log2_base)[j] = log2((*log2_base)[j]);
    }
  }

  double sec = seconds();
  FILE * file_r;
  param_list_parse_string(pl, "fb", path, size_path);
  file_r = fopen(path, "r");
  read_factor_base(file_r, *fb, *fbb, *lpb, f, *log2_base, H->t);
  fclose(file_r);
  fprintf(* outstd,
      "# Time to read factor bases: %f.\n", seconds() - sec);

  array_init(array, number_element);

  mat_Z_init(matrix, H->t, H->t);

  * main_side = -1;
  param_list_parse_int(pl, "main", main_side);

  for (unsigned int i = 0; i < * V; i++) {
    (* sieve_start)[i] = 2;
  }
  param_list_parse_uint64_list(pl, "start", * sieve_start, (size_t) * V, ",");

#ifndef NDEBUG
  for (unsigned int i = 0; i < * V; i++) {
    ASSERT((* sieve_start)[i] < (*fbb)[i]);
  }
#endif // NDEBUG


  param_list_clear(pl);
}

/*
   The main.
   */
int main(int argc, char * argv[])
{
  unsigned int V;
  cado_poly f;
  uint64_t * fbb;
  sieving_bound_t H;
  uint64_t * q_range;
  unsigned int q_side;
  unsigned char * thresh;
  unsigned int * lpb;
  array_t array;
  mat_Z_t matrix;
  factor_base_t * fb;
  uint64_t q;
  int main_side;
  double * log2_base;
  FILE * outstd;
  FILE * errstd;
  uint64_t * sieve_start;
  sieving_bound_t Ha;
  mpz_poly_t g;

  initialise_parameters(argc, argv, f, &fbb, &fb, H, &q_range,
      &thresh, &lpb, array, matrix, &q_side, &V, &main_side, &log2_base,
      &outstd, &errstd,
      &sieve_start, Ha, g);

  //Store all the index of array with resulting norm less than thresh.
  uint64_array_t * indexes =
    (uint64_array_t * ) malloc(sizeof(uint64_array_t) * V);

#ifdef OLD_NORM
  double ** pre_compute = (double ** ) malloc(sizeof(double * ) * V);
  for (unsigned int i = 0; i < V; i++) {
    pre_compute[i] = (double * ) malloc((H->t) * sizeof(double));
    pre_computation(pre_compute[i], f->pols[i], H->t);
  }
#else // OLD_NORM
  unsigned char * max_norm = (unsigned char *) malloc(sizeof(unsigned char) *
      V);
#endif // OLD_NORM

  double ** time = (double ** ) malloc(sizeof(double * ) * V);
  for (unsigned int i = 0; i < V; i++) {
    time[i] = (double * ) malloc(sizeof(double) * 3);
  }
  double sec_tot;
  double sec_cofact;
  double sec;

  FILE * file_trace_pos;
#ifdef TRACE_POS
  file_trace_pos = fopen("TRACE_POS.txt", "w+");
  fprintf(file_trace_pos, "TRACE_POS: %d\n", TRACE_POS);
#else
  file_trace_pos = NULL;
#endif // TRACE_POS

#ifdef PRINT_ARRAY_NORM
  FILE * file_array_norm;
  file_array_norm = fopen("ARRAY_NORM.txt", "w+");
  fprintf(file_array_norm, "H: ");
  sieving_bound_fprintf(file_array_norm, H);
#endif // PRINT_ARRAY_NORM

  gmp_randstate_t state;
  mpz_t a;
  mpz_poly_factor_list l;

  mpz_poly_factor_list_init(l);
  gmp_randinit_default(state);
  mpz_init(a);

  uint64_t nb_rel = 0;
  double total_time = 0.0;
  uint64_t spq_tot = 0;

  mpz_vector_t skewness;
  mpz_vector_init(skewness, Ha->t);
  mpz_set_ui(skewness->c[0], 1);
  for (unsigned int i = 1; i < Ha->t; i++) {
    mpz_set_si(skewness->c[i], Ha->h[0] / Ha->h[i]);
  }
  sieving_bound_clear(Ha);

  prime_info pi;
  prime_info_init (pi);
  //Pass all the prime less than q_range[0].
  for (q = 2; q < q_range[0]; q = getprime_mt(pi)) {}

#ifdef SPQ_IDEAL_U
    int deg_bound_factorise = (int)H->t;
#else // SPQ_IDEAL_U
    int deg_bound_factorise = 2;
#endif // SPQ_IDEAL_U

  for ( ; q <= q_range[1]; q = getprime_mt(pi)) {
    ideal_spq_t special_q;
    ideal_spq_init(special_q);
    mpz_set_si(a, q);
    mpz_poly_factor(l, f->pols[q_side], a, state);

    for (int i = 0; i < l->size; i++) {

      if (l->factors[i]->f->deg < deg_bound_factorise) {
        if (l->factors[i]->f->deg == 1) {
#ifdef SPQ_DEFINED
          if (g->deg != -1) {
            if (mpz_poly_cmp(g, l->factors[i]->f)) {
              continue;
            }
          }
#endif // SPQ_DEFINED
          ideal_spq_set_part(special_q, q, l->factors[i]->f, H->t, 0);
        } else {
          ASSERT(l->factors[i]->f->deg > 1);

#ifdef SPQ_DEFINED
          if (g->deg != -1) {
            if (mpz_poly_cmp(g, l->factors[i]->f)) {
              continue;
            }
          }
#endif // SPQ_DEFINED
          ideal_spq_set_part(special_q, q, l->factors[i]->f, H->t, 1);
        }

        fprintf(outstd, "# Special-q: q: %" PRIu64 ", g: ", q);
        mpz_poly_fprintf(outstd, l->factors[i]->f);

#ifdef TRACE_POS
        fprintf(file_trace_pos, "Special-q: q: %" PRIu64 ", g: ", q);
        mpz_poly_fprintf(file_trace_pos, l->factors[i]->f);
        fprintf(file_trace_pos, "Side of the special-q: %u\n", q_side);
#endif // TRACE_POS

        sec = seconds();
        sec_tot = sec;

        /* LLL part */
        build_Mq_ideal_spq(matrix, special_q);

#ifdef TRACE_POS
        fprintf(file_trace_pos, "Mq:\n");
        mat_Z_fprintf(file_trace_pos, matrix);
#endif // TRACE_POS

#ifndef SKEW_LLL_SPQ
        mat_Z_LLL_transpose(matrix, matrix);
#else // SKEW_LLL_SPQ
        mat_Z_skew_LLL(matrix, matrix, skewness);
#endif // SKEW_LLL_SPQ

        /*
         * TODO: continue here.
         * If the last coefficient of the last vector is negative, we do not
         * sieve in the good direction. We therefore take the opposite vector
         * because we want that a = MqLLL * c, with c in the sieving region
         * has the last coordinate positive.
         */

        reorganize_MqLLL(matrix);

#ifdef TRACE_POS
        fprintf(file_trace_pos, "MqLLL:\n");
        mat_Z_fprintf(file_trace_pos, matrix);
#endif // TRACE_POS

#ifdef PRINT_ARRAY_NORM
          fprintf(file_array_norm, "MqLLL:\n");
          mat_Z_fprintf(file_array_norm, matrix);
#endif // PRINT_ARRAY_NORM

#ifndef OLD_NORM
          memset(max_norm, 0, sizeof(unsigned char) * V);
#endif // OLD_NORM

        for (unsigned int j = 0; j < V; j++) {
#ifdef TRACE_POS
          fprintf(file_trace_pos, "Base: %f\n", pow(2.0, log2_base[j]));
#endif // TRACE_POS

          sec = seconds();
          uint64_array_init(indexes[j], array->number_element);
          array_set_all_elements(array, UCHAR_MAX);
#ifndef OLD_NORM
          init_norm(array, max_norm + j, file_trace_pos, H, matrix, f->pols[j],
              ideal_spq_get_log(special_q) / log2_base[j], !(j ^ q_side),
              log2_base[j]);
#else // OLD_NORM
          init_norm(array, file_trace_pos, pre_compute[j], H, matrix,
              f->pols[j], special_q, !(j ^ q_side));
#endif // OLD_NORM

#ifdef PRINT_ARRAY_NORM
          fprintf(file_array_norm, "f%u: ", j);
          mpz_poly_fprintf(file_array_norm, f->pols[j]);
          number_norm(file_array_norm, array, max_norm[j]);
          fprintf(file_array_norm, "********************\n");
#endif // PRINT_ARRAY_NORM

          time[j][0] = seconds() - sec;

#ifdef ASSERT_NORM
          printf("# ASSERT_NORM side %u.\n", j);
          assert_norm(array, H, f->pols[j], matrix, !(j ^ q_side), log2_base[j],
              ideal_spq_get_log(special_q));
#endif // ASSERT_NORM

          sec = seconds();
          special_q_sieve(array, file_trace_pos, matrix, fb[j], H, f->pols[j],
              outstd, errstd, sieve_start[j], special_q);
          time[j][1] = seconds() - sec;
          sec = seconds();
          find_index(indexes[j], array,
              (unsigned char) ceil(thresh[j] / log2_base[j]));
          time[j][2] = seconds() - sec;

#ifdef TRACE_POS
        fprintf(file_trace_pos, "********************\n");
#endif // TRACE_POS
        }

        for (unsigned int j = 0; j < V; j++) {
          fprintf(outstd,
              "# Log 2 of the maximum of the norms %u: %u.\n", j,
              max_norm[j]);
        }

        sec = seconds();
        nb_rel += (uint64_t) find_relations(indexes, array->number_element, lpb,
            matrix, f->pols, H, V, special_q, q_side, main_side,
            outstd);
        sec_cofact = seconds() - sec;

        for (unsigned j = 0; j < V; j++) {

          uint64_array_clear(indexes[j]);
        }

        fprintf(outstd,
            "# Time for this special-q: %fs.\n", seconds() - sec_tot);
        total_time += (seconds() - sec_tot);
        spq_tot++;
        for (unsigned int j = 0; j < V; j++) {
          fprintf(outstd, "# Time to init norm %u: %fs.\n", j,
              time[j][0]);
          fprintf(outstd, "# Time to sieve %u: %fs.\n", j, time[j][1]);
          fprintf(outstd, "# Time to find indexes %u: %fs.\n", j,
              time [j][2]);
        }

        fprintf(outstd, "# Time to factorize: %fs.\n", sec_cofact);

        fprintf(outstd,
            "# ----------------------------------------\n");
        fprintf(errstd,
            "# ----------------------------------------\n");
        fflush(outstd);
        fflush(errstd);

#ifdef TRACE_POS
        fprintf(file_trace_pos, "----------------------------------------\n");
#endif // TRACE_POS

#ifdef PRINT_ARRAY_NORM
        fprintf(file_array_norm, "----------------------------------------\n");
        fflush(file_array_norm);
#endif // PRINT_ARRAY_NORM

        ideal_spq_clear(special_q, H->t);
      }
    }
  }

  fprintf(outstd, "# Total time: %fs.\n", total_time);
  fprintf(outstd, "# Total number of relations: %" PRIu64 ".\n",
      nb_rel);
  fprintf(outstd, "# Total number of special-q: %" PRIu64 ".\n",
      spq_tot);
  fprintf(outstd, "# Time per special-q: %fs.\n",
      total_time / (double)spq_tot);
  fprintf(outstd, "# Time per relation: %fs.\n",
      total_time / (double)nb_rel);
  fprintf(outstd, "# Relations per special-q: %f.\n",
      (double)nb_rel / (double)spq_tot);

  mpz_poly_factor_list_clear(l);
  gmp_randclear(state);
  mpz_clear(a);
  prime_info_clear (pi);

#ifdef TRACE_POS
  fclose(file_trace_pos);
#endif // TRACE_POS

#ifdef PRINT_ARRAY_NORM
  fclose(file_array_norm);
#endif // PRINT_ARRAY_NORM

  mat_Z_clear(matrix);
  for (unsigned int i = 0; i < V; i++) {
    factor_base_clear(fb[i], H->t);
#ifdef OLD_NORM
    free(pre_compute[i]);
#endif // OLD_NORM
    free(time[i]);
  }
  free(time);
  free(indexes);
#ifdef OLD_NORM
  free(pre_compute);
#else
  free(max_norm);
#endif // OLD_NORM
  array_clear(array);
  free(lpb);
  free(fb);
  free(sieve_start);
  sieving_bound_clear(H);
  free(fbb);
  free(thresh);
  free(q_range);
  free(log2_base);
  cado_poly_clear(f);
  free_saved_chains();
  fclose(outstd);
  fclose(errstd);
  mpz_vector_clear(skewness);
  mpz_poly_clear(g);

  return 0;
}
