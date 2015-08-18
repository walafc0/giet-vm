/*
 * ====================================================
 * Copyright (C) 1993 by Sun Microsystems, Inc. All rights reserved.
 *
 * Developed at SunPro, a Sun Microsystems, Inc. business.
 * Permission to use, copy, modify, and distribute this
 * software is freely granted, provided that this notice
 * is preserved.
 * ====================================================
 */

/* Modified for GIET-VM static OS at UPMC, France 2015.
 */

/*
 * finite(x) returns 1 is x is finite, else 0;
 * no branching!
 */

#include "../math.h"
#include "math_private.h"

int isfinite(double x)
{
	u_int32_t hx;

	GET_HIGH_WORD(hx, x);
	/* Finite numbers have at least one zero bit in exponent. */
	/* All other numbers will result in 0xffffffff after OR: */
	return (hx | 0x800fffff) != 0xffffffff;
}

