#include <stdio.h>
#include <stdlib.h>
#include "timer.h"

#define MAXN 50

long result[MAXN+1];   // result[x] = number of times fib was called with argument x

long fib(long n) {
 long x, y;
 if(n<2) return n;
 #pragma omp task shared(x)
 { x=fib(n-1);
   #pragma omp atomic
   result[n-1]++; }    //the call fib(n-1) has returned
 #pragma omp task shared(y)
 { y=fib(n-2);
   #pragma omp atomic
   result[n-2]++; }    //the call fib(n-2) has returned
 #pragma omp taskwait
 return x+y;
}

// F(k) for k = -1 .. n+1, using F(-1) = 1, F(0) = 0, F(1) = 1
long F(long k) {
 long a = 1, b = 0, t;   // a = F(-1), b = F(0)
 if (k < 0) return 1;
 for (long i = 0; i < k; i++) { t = a + b; a = b; b = t; }
 return b;
}

// compare result[] against result[x] = F(n-x+1) for x >= 1, result[0] = F(n-1),
// and the total against 2F(n+1) - 1; print OK or WRONG
int check(long n) {
 long x, total = 0, expected;
 int ok = 1;
 for (x = 0; x <= n; x++) {
  expected = (x == 0) ? F(n-1) : F(n-x+1);
  if (result[x] != expected) {
   printf("result[%ld] = %ld, expected %ld\n", x, result[x], expected);
   ok = 0;
  }
  total += result[x];
 }
 expected = 2*F(n+1) - 1;
 if (total != expected) {
  printf("total = %ld, expected %ld\n", total, expected);
  ok = 0;
 }
 printf(ok ? "OK\n" : "WRONG\n");
 return ok;
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
  #pragma omp parallel
  #pragma omp single
  v=fib(n);
  result[n]++;          //the top-level call fib(n) has returned
  stop_timer();
  time=elapsed_time();

  printf("fib(%ld) = %ld\n", n, v);
  check(n);

#ifdef DEBUG
  printf("call counts:\n");
  for (i = n; i >= 0; i--)
   printf("result[%ld] = %ld  (expected %ld)\n", i, result[i], (i == 0) ? F(n-1) : F(n-i+1));
#endif

  printf("elapsed time = %lf (sec)\n", time);
  return 0;
}
