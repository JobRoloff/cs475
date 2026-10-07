#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"

void mergesort(int a[],int temp[], int low, int high);
void merge(int a[],int temp[], int low, int high, int mid);
int check_sort(const int a[], int n, int after);

void mergesort(int a[],int temp[], int low, int high) {
 int mid;
 if(low<high) {
  mid=(low+high)/2;             //find the midpoint
  mergesort(a,temp,low,mid);   //sort the first half
  mergesort(a,temp,mid+1,high); //sort the second half
  merge(a,temp,low,high,mid);  //merge them togeather into one sorted list

 }
}

void merge(int a[],int temp[], int low, int high, int mid){
 int i, j, k;
 i=low; 
 j=mid+1; 
 k=low;
 while((i<=mid)&&(j<=high)){
   if(a[i]<a[j]){
    temp[k]=a[i]; 
    k++; 
    i++;
   } else {
    temp[k]=a[j]; 
    k++; 
    j++;
   }
 }
 while(i<=mid){
   temp[k]=a[i]; 
   k++; 
   i++;
 }
 while(j<=high){
   temp[k]=a[j]; 
   k++; 
   j++;
 }
 for(i=low;i<k;i++) {
  a[i]=temp[i];
 }
}

int main(int argc, char **argv) {
 int LEN;
 double time;

 LEN=100000000;   
 // Command line argument: array length
 if ( argc > 1 ) LEN  = atoi(argv[1]);  
  int i, *x,*temp;
  x=(int *)malloc(sizeof(int)*LEN);
  temp=(int *)malloc(sizeof(int)*LEN);
  if(x==NULL || temp == NULL){
  printf("Out of memory"); exit(0);
  }

  //Fill the array to be sorted with random numbers
  for (i = 0; i < LEN; i++) 
    x[i] = rand() % LEN;

#ifdef DEBUG
  printf("before sort:\n");
  for (i = 0; i < LEN; i++) printf("%d ", x[i]);
  printf("\n");
#endif

  check_sort(x, LEN, 0);

  initialize_timer ();  //We are just timing the sort
  start_timer();
  mergesort(x,temp,0, (LEN-1));
  stop_timer();
  time=elapsed_time();

#ifdef DEBUG
  printf("after sort:\n");
  for (i = 0; i < LEN; i++) printf("%d ", x[i]);
  printf("\n");
#endif

  printf("elapsed time = %lf (sec)\n", time);

  if (!check_sort(x, LEN, 1)) return 1;
  return 0;
}



//Correctness check. Do not modify.
int check_sort(const int a[], int n, int after) {
 static unsigned long long sum0;
 static unsigned int xor0;
 unsigned long long sum = 0;
 unsigned int xr = 0;
 int i;
 for (i = 0; i < n; i++) { sum += (unsigned long long)a[i]; xr ^= (unsigned int)a[i]; }
 if (!after) { sum0 = sum; xor0 = xr; return 1; }
 for (i = 1; i < n; i++) {
  if (a[i-1] > a[i]) {
   printf("FAILED: not sorted at index %d (%d > %d)\n", i, a[i-1], a[i]);
   return 0;
  }
 }
 if (sum != sum0 || xr != xor0) {
  printf("FAILED: elements lost, duplicated or altered by the sort\n");
  return 0;
 }
 return 1;
}
