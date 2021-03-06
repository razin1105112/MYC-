/*
 BigNumbers - Arbitrary precision arithmetic
 Copyright 2000-2010, Ib�n Cereijo Gra�a <ibancg at gmail dot com>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>

// Multiplication algorithms
//
//          AAAA.AAAA  The multiplication result has the double of digits than
//		  * BBBB.BBBB  the operands. In a fixed-point system like this, as
//  -----------------  digits as assigned decimal figures will be lost (by the
//  CCCCCCCC.CCCCCCCC  least significant side). In the most signiticant side,
//					   other digits are also dropped, but this is not considered
//					   as a loss of information, since if any of them were
//					   different to 0, an overflow error may happened.

#include "mul.h"
#include "fft.h"
#include "constant.h"
// Classical algorithm.
void mulLMA(BigNumber* A, BigNumber* B, BigNumber* C) {
	int i, j;
	unsigned long int r, c;
    bcd_t* R = (bcd_t*)malloc( sizeof(bcd_t)*(N_DIGIT + F_DIGIT));

	memset(R, 0,
			(N_DIGIT + F_DIGIT) * sizeof(bcd_t));

	for (i = 0; i < N_DIGIT; i++) {
		for (j = 0, c = 0;
				(j < N_DIGIT)
						&& ((j + i)
								< (N_DIGIT
										+ F_DIGIT)); j++) {

			r = R[j + i] + c + A->digits[j] * B->digits[i];
			c = (r / 10);
			R[j + i] = (r - 10 * c);
		}

		// decimal adjustment
		for (; (j + i) < (N_DIGIT + F_DIGIT);
				j++) {
			r = R[j + i] + c;
			c = (r / 10);
			R[j + i] = (r - 10 * c);
		}
	}

	C->isPositive = !(A->isPositive ^ B->isPositive);
	memcpy(C->digits, &R[F_DIGIT],
			N_DIGIT * sizeof(bcd_t));

//	delete R;
}

// The former algorithm has order of n^2 time complexity, so it presents a
// scalability problem for big numbers.
//
// Let us consider A and B sequences as polynomial coefficients. Then the
// convolution operation of both signals will represent the polynomial
// product. Knowing that none of the coefficients can be greather than their
// base, if we do a decimal adjustement consisting in propagate the carry
// values towards higher weight digits, the final sequence will be the
// multiplication of both numbers. Thus, the multiplication can be performed
// with a convolution operation with BCD adjustment.
//
// A convolution operation has also order of n^2 time complexity, but is well
// known that it's transformed in an elementwise multiplication in frequency
// domain. So, we can transform the signals, multiply them in frequency, and
// apply an inverse transform.
//
// An efficient DFT calculus can be achieved with a FFT (Fast Fourier Transform)
// algorithm, which reduces the complexity from O(n^2) to O(n*log2(n)).
//
// Optimization:
//
// The DFT signal will be complex even if the input signal does not, and we know
// that the input signals (numbers to be multiplied) are real signals. Thus, we
// can build an unique singal containing the information of both signals in
// their real and imaginary parts.
//
// x[n] = x1 + i*x2[n] --> FFT --> X[n]
//
// Using FFT properties, we can extract:
//
// X1[n] =     1/2*(X[n] + conj(X[-n mod N]))
// X2[n] = 1/(2*i)*(X[n] - conj(X[-n mod N]))
//
// So, we need to compute only one FFT instead of 2.

// FFT-based multiplication imlpementation
void mulFFT(BigNumber* A, BigNumber* B, BigNumber* C) {
	 Complex* BC1 = (Complex*)malloc(sizeof(Complex)*(N_DIGIT << 1));
	 Complex* BC2 = (Complex*)malloc(sizeof(Complex)*(N_DIGIT << 1));
	register int i;
	Complex Xi, Xmi, X1, X2, X3;

	// step 1: building a complex signal with the information of both signals.
	for (i = 0; i < N_DIGIT; i++) {
		BC1[i].r = A->digits[i]; // real part
		BC1[i].i = B->digits[i]; // imaginary part
	}

	// cleans the higher section.
	memset(&BC1[N_DIGIT], 0, N_DIGIT * sizeof(Complex));

	// step 2: transform.
	fft(BC1, BC2, (N_DIGIT << 1),0,0,1);

	// step 3: point-wise multiplication in frequency domain.
	for (i = 0; i < (N_DIGIT << 1); i++) {

		// we need to extract the individual transformed signals from the
		// composited one.
		Xi = BC2[i];
		Xmi = BC2[(-i) & ((N_DIGIT << 1) - 1)];
		Xmi.i = -Xmi.i; // conjugate

		X1.r = Xi.r + Xmi.r;
		X1.i = Xi.i + Xmi.i; // X1 = Xi + Xmi
		X2.r = Xi.r - Xmi.r;
		X2.i = Xi.i - Xmi.i; // X2 = Xi - Xmi

		// now let us multiply sample by sample.
		X3.r = X1.r * X2.r - X1.i * X2.i;
		X3.i = X1.r * X2.i + X1.i * X2.r; // X3 = X1*X2;

		BC1[i].r = 0.25 * X3.i;
		BC1[i].i = -0.25 * X3.r;
	}

	// step 4: inverse transform.
	ifft(BC1, BC2, (N_DIGIT << 1),0,0,1);

	unsigned long int c, ci;
	flt_t x;

	// step 5: cleaning and BCD adjust.
	for (i = 0, c = 0; i < N_DIGIT + F_DIGIT;
			i++) {

		x = BC2[i].r; // drops imaginary part.

		// rounding
		ci = (unsigned long int) (c + round(x));

		c = (ci / 10); // carry propagation
		if (i >= F_DIGIT)
			C->digits[i - F_DIGIT] = (ci - 10 * c); // ci % 10
	}

	C->isPositive = !(A->isPositive ^ B->isPositive);
}

void fastMul(BigNumber* A, BigNumber* B, BigNumber* C){
    createPhaseFactors();
    mulFFT(A,B,C);
    destroyPhaseFactors();

}
void product(char* a, char* b)
 {
    int result[4000];
    int i,j;
    for( i = strlen(a) - 1; i >= 0; i-- )
    {
        for( j = strlen(b) - 1; j >= 0; j-- )
        {
            result[ i + j + 1 ] += ( b[ j ] - '0') * ( a[ i ] - '0' ); //single array to store intermediate values
        }
    }
    for(  i = strlen(a) + strlen(b); i >= 0; i-- ){
        if( result[ i ] >= 10 ){
            result[ i - 1 ] +=result[ i ] / 10;result[ i ] %= 10;
        }
    }
    printf( " %s * %s = ",a,b);
    for( i = 0; i < strlen(a) + strlen(b); i++ ){
        printf("%d",result[i]);
    }
    printf("\n");
}
