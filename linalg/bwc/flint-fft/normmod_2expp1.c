#ifdef  __GNUC__
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-parameter"
/* flint uses unprotected openmp pragmas every so often */
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif
/* 
 * Copyright (C) 2009, 2011 William Hart
 * 
 * This file is part of FLINT.
 * 
 * FLINT is free software: you can redistribute it and/or modify it under the 
 * terms of the GNU Lesser General Public License (LGPL) as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.  See <http://www.gnu.org/licenses/>. */

#include "gmp.h"
#include "flint.h"
#include "fft.h"

/*
 * Given ``t`` a signed integer of ``limbs + 1`` limbs in twos
 * complement format, reduce ``t`` to the corresponding value modulo the
 * generalised Fermat number ``B^limbs + 1``, where
 * ``B = 2^FLINT_BITS``.
 * 
 */
void mpn_normmod_2expp1(mp_limb_t * t, mp_size_t limbs)
{
    mp_limb_signed_t hi = t[limbs];

    if (hi) {
	t[limbs] = 0;

	mpn_addmod_2expp1_1(t, limbs, -hi);

	/* hi will now be in [-1,1] */
	if ((hi = t[limbs])) {
	    t[limbs] = 0;

	    mpn_addmod_2expp1_1(t, limbs, -hi);

	    if (t[limbs] == ~(mp_limb_signed_t) 0) {	/* if we now have -1
							 * (very unlikely) */
		t[limbs] = 0;
		mpn_addmod_2expp1_1(t, limbs, 1);
	    }
	}
    }
}
