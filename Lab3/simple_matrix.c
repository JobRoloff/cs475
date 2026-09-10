#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main ()
{
    time_t t;
    float **A, **B, **C;
    int N, col1, row2, col2;
    srand ((unsigned) time (&t));
    int i, j, k;
	
	
    N = 2000;
    printf ("\nThe value of N is : %d\n",N);
    
    A = (float **) malloc (sizeof (float *) * N);
    B = (float **) malloc (sizeof (float *) * N);
    C = (float **) malloc (sizeof (float *) * N);

    for (i = 0; i < N; i++)
        A[i] = (float *) malloc (sizeof (float ) * N);
    for (i = 0; i < N; i++)
        B[i] = (float *) malloc (sizeof (float ) * N);
    for (i = 0; i < N; i++)
        C[i] = (float *) malloc (sizeof (float ) * N);

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            A[i][j] = rand ();
        }
    }

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            B[i][j] = rand ();
        }
    }

    for (i = 0; i < N; i++) {
        for (j = 0; j < N; j++) {
            C[i][j] = 0;
        }
    }



    clock_t clk;
    clk = clock();
    for (i = 0; i < N; i++) {
      for (j = 0; j < N; j++) {
	for (k = 0; k < N; k+=1) {
	  C[i][j] += A[i][k] * B[k][j];
 	}
      }
    }
    clk = clock() - clk;
    double time_taken = ((double)clk)/CLOCKS_PER_SEC;
    printf ("Time: %f\n", time_taken);

    return (0);
}