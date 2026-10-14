#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

#define MAXN 50

long result[MAXN+1];   // result[x] = number of times fib was called with argument x

long fib(long n) {
 long x, y;
 if(n<2) return n;
 x=fib(n-1);
 result[n-1]++;        //the call fib(n-1) has returned
 y=fib(n-2);
 result[n-2]++;        //the call fib(n-2) has returned
 return x+y;
}

int main(int argc, char **argv) {
 long n, v, i;
 double time;

 n=30;
 // Command line argument: n
 if ( argc > 1 ) n = atol(argv[1]);
 if ( n < 0 || n > MAXN ) {
  printf("n must be between 0 and %d\n", MAXN); exit(0);
 }

  initialize_timer ();  //We are just timing the computation
  start_timer();
  v=fib(n);
  result[n]++;          //the top-level call fib(n) has returned
  stop_timer();
  time=elapsed_time();

  printf("fib(%ld) = %ld\n", n, v);

#ifdef DEBUG
  printf("call counts:\n");
  for (i = n; i >= 0; i--) printf("result[%ld] = %ld\n", i, result[i]);
#endif

  printf("elapsed time = %lf (sec)\n", time);
  return 0;
}
