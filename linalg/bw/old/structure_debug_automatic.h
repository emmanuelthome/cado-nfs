/* This file was generated by m4 on Sun Jun 25 22:53:29 CEST 2006 */
/* Do not edit this file. Re-generate instead from the source (using m4) */

#ifndef INCLUDING_STRUCTURE_DEBUG_AUTOMATIC_H_
#error "Do not include this file directly\n"
#endif

#ifndef STRUCTURE_DEBUG_AUTOMATIC_H_
#define STRUCTURE_DEBUG_AUTOMATIC_H_

#ifdef __cplusplus
extern "C" {
#endif

void p_mnrow(bw_mnmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < n_param ; j++) {
		k_print(mnmat_scal(mat,i,j));
		if (magma_display && j+1 < n_param )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_mncol(bw_mnmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			k_print(mnmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<m_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++) {
			printf("[ ");
			k_print(mnmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_mnmat(bw_mnmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			for(j = 0 ; j < n_param ; j++) {
				k_print(mnmat_scal(mat,i,j));
				if (i+1<m_param || j+1<n_param)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<m_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++)
			p_mnrow(mat,i);
	}
}

bw_mnmat h_mncoeff(bw_mnpoly p, int t)
{
	return mnpoly_coeff(p,t);
}

void p_nbrow(bw_nbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		k_print(nbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_nbcol(bw_nbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < n_param ; i++) {
			k_print(nbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<n_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < n_param ; i++) {
			printf("[ ");
			k_print(nbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_nbmat(bw_nbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < n_param ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				k_print(nbmat_scal(mat,i,j));
				if (i+1<n_param || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<n_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < n_param ; i++)
			p_nbrow(mat,i);
	}
}

bw_nbmat h_nbcoeff(bw_nbpoly p, int t)
{
	return nbpoly_coeff(p,t);
}

void p_mbrow(bw_mbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		k_print(mbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_mbcol(bw_mbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			k_print(mbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<m_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++) {
			printf("[ ");
			k_print(mbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_mbmat(bw_mbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				k_print(mbmat_scal(mat,i,j));
				if (i+1<m_param || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<m_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++)
			p_mbrow(mat,i);
	}
}

bw_mbmat h_mbcoeff(bw_mbpoly p, int t)
{
	return mbpoly_coeff(p,t);
}

void p_bbrow(bw_bbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		k_print(bbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_bbcol(bw_bbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < bigdim ; i++) {
			k_print(bbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<bigdim)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < bigdim ; i++) {
			printf("[ ");
			k_print(bbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_bbmat(bw_bbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < bigdim ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				k_print(bbmat_scal(mat,i,j));
				if (i+1<bigdim || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<bigdim)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < bigdim ; i++)
			p_bbrow(mat,i);
	}
}

bw_bbmat h_bbcoeff(bw_bbpoly p, int t)
{
	return bbpoly_coeff(p,t);
}


void p_x_mnrow(bw_x_mnmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < n_param ; j++) {
		l_print(x_mnmat_scal(mat,i,j));
		if (magma_display && j+1 < n_param )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_x_mncol(bw_x_mnmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			l_print(x_mnmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<m_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++) {
			printf("[ ");
			l_print(x_mnmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_x_mnmat(bw_x_mnmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			for(j = 0 ; j < n_param ; j++) {
				l_print(x_mnmat_scal(mat,i,j));
				if (i+1<m_param || j+1<n_param)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<m_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++)
			p_x_mnrow(mat,i);
	}
}

bw_x_mnmat h_x_mncoeff(bw_x_mnpoly p, int t)
{
	return x_mnpoly_coeff(p,t);
}

void p_x_nbrow(bw_x_nbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		l_print(x_nbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_x_nbcol(bw_x_nbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < n_param ; i++) {
			l_print(x_nbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<n_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < n_param ; i++) {
			printf("[ ");
			l_print(x_nbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_x_nbmat(bw_x_nbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < n_param ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				l_print(x_nbmat_scal(mat,i,j));
				if (i+1<n_param || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<n_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < n_param ; i++)
			p_x_nbrow(mat,i);
	}
}

bw_x_nbmat h_x_nbcoeff(bw_x_nbpoly p, int t)
{
	return x_nbpoly_coeff(p,t);
}

void p_x_mbrow(bw_x_mbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		l_print(x_mbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_x_mbcol(bw_x_mbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			l_print(x_mbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<m_param)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++) {
			printf("[ ");
			l_print(x_mbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_x_mbmat(bw_x_mbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < m_param ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				l_print(x_mbmat_scal(mat,i,j));
				if (i+1<m_param || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<m_param)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < m_param ; i++)
			p_x_mbrow(mat,i);
	}
}

bw_x_mbmat h_x_mbcoeff(bw_x_mbpoly p, int t)
{
	return x_mbpoly_coeff(p,t);
}

void p_x_bbrow(bw_x_bbmat mat, int i)
{
	int j;

	printf("[ ");
	for(j = 0 ; j < bigdim ; j++) {
		l_print(x_bbmat_scal(mat,i,j));
		if (magma_display && j+1 < bigdim )
			printf(",%c", (magma_display==2)?0x20:0x09);
		else
			printf("   ");
	}
	printf("]\n");
}

void p_x_bbcol(bw_x_bbmat mat, int j)
{
	int i;

	printf("\n");
	if (magma_display) {
		printf("[ ");
		for(i = 0 ; i < bigdim ; i++) {
			l_print(x_bbmat_scal(mat,i,j));
			if (magma_display !=2 && i+1<bigdim)
				printf(",\n  ");
		}
		printf(" ]\n");
	} else {
		for(i = 0 ; i < bigdim ; i++) {
			printf("[ ");
			l_print(x_bbmat_scal(mat,i,j));
			printf(" ]\n");
		}
	}
}

void p_x_bbmat(bw_x_bbmat mat)
{
	int i;
	if (magma_display) {
		int i,j;

		printf("[ ");
		for(i = 0 ; i < bigdim ; i++) {
			for(j = 0 ; j < bigdim ; j++) {
				l_print(x_bbmat_scal(mat,i,j));
				if (i+1<bigdim || j+1<bigdim)
					printf(",%c",
						(magma_display==2)?0x20:0x09);
			}
			if (magma_display !=2 && i+1<bigdim)
				printf("\n  ");
		}
		printf("    ]\n");
	} else {
		for(i = 0 ; i < bigdim ; i++)
			p_x_bbrow(mat,i);
	}
}

bw_x_bbmat h_x_bbcoeff(bw_x_bbpoly p, int t)
{
	return x_bbpoly_coeff(p,t);
}

#ifdef __cplusplus
}
#endif

#endif
