#ifndef MATH_H
#define MATH_H

// This will be our version of the math class for calculating the priority, real cpu, load avg...which are all needed for our implementation of the advanced schdular...

//this math class will inlude all of the following functions//

/*
Convert n to fixed point: 			n * f
Convert x to integer (rounding toward zero): 	x / f
Convert x to integer (rounding to nearest): 	(x + f / 2) / f if x >= 0,
						(x - f / 2) / f if x <= 0.
Add x and y: 					x + y
Subtract y from x: 				x - y
Add x and n: 					x + n * f
Subtract n from x: 				x - n * f
Multiply x by y: 				((int64_t) x) * y / f
Multiply x by n: 				x * n
Divide x by y: 					((int64_t) x) * f / y
Divide x by n: 					x / n
*/

#define P 18
#define Q 13
#define FRACTION 1 << (Q)

/* Fixed-point real arithmetic */
/* Here x and y are fixed-point number, n is an integer */

#define CONVERT_TO_FP(n) (n) * (FRACTION)
#define CONVERT_TO_INT_ZERO(x) (x) / (FRACTION)
#define CONVERT_TO_INT_NEAREST(x) ((x) >= 0 ? ((x) + (FRACTION) / 2)\
				/ (FRACTION) : ((x) - (FRACTION) / 2)\
				/ (FRACTION))
#define ADD(x, y) (x) + (y)
#define SUB(x, y) (x) - (y)
#define ADD_INT(x, n) (x) + (n) * (FRACTION)
#define SUB_INT(x, n) (x) - (n) * (FRACTION)
#define MULTIPLE(x, y) ((int64_t)(x)) * (y) / (FRACTION)
#define MULT_INT(x, n) (x) * (n)
#define DIVIDE(x, y) ((int64_t)(x)) * (FRACTION) / (y)
#define DIV_INT(x, n) (x) / (n)

#endif




