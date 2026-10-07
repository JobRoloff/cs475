#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "timer.h"

// Threshold to prevent over-tasking overhead on small sub-arrays
#define TASK_CUTOFF 10000

void mergesort(int a[], int temp[], int low, int high);
void merge(int a[], int temp[], int low, int high, int mid);
int check_sort(const int a[], int n, int after);

void mergesort(int a[], int temp[], int low, int high) {
    int mid;
    if (low < high) {
        mid = low + (high - low) / 2;

        // Cutoff check to avoid task creation overhead for small arrays
        if (high - low < TASK_CUTOFF) {
            mergesort(a, temp, low, mid);
            mergesort(a, temp, mid + 1, high);
        } else {
            // Task 1: Sort left half
            #pragma omp task firstprivate(a, temp, low, mid)
            mergesort(a, temp, low, mid);

            // Task 2: Sort right half
            #pragma omp task firstprivate(a, temp, mid, high)
            mergesort(a, temp, mid + 1, high);

            // Wait for both subtasks to finish before merging
            #pragma omp taskwait
        }

        merge(a, temp, low, high, mid);
    }
}

void merge(int a[], int temp[], int low, int high, int mid) {
    int i, j, k;
    i = low; j = mid + 1; k = low;
    while ((i <= mid) && (j <= high)) {
        if (a[i] < a[j]) {
            temp[k] = a[i]; k++; i++;
        } else {
            temp[k] = a[j]; k++; j++;
        }
    }
    while (i <= mid) {
        temp[k] = a[i]; k++; i++;
    }
    while (j <= high) {
        temp[k] = a[j]; k++; j++;
    }
    for (i = low; i <= high; i++) {
        a[i] = temp[i];
    }
}

int main(int argc, char **argv) {
    int LEN = 100000000;
    double time;

    if (argc > 1) LEN = atoi(argv[1]);

    int i, *x, *temp;
    x = (int *)malloc(sizeof(int) * LEN);
    temp = (int *)malloc(sizeof(int) * LEN);

    if (x == NULL || temp == NULL) {
        printf("Out of memory\n");
        exit(0);
    }

    for (i = 0; i < LEN; i++)
        x[i] = rand() % LEN;

#ifdef DEBUG
    printf("before sort:\n");
    for (i = 0; i < LEN; i++) printf("%d ", x[i]);
    printf("\n");
#endif

    check_sort(x, LEN, 0);

    initialize_timer();
    start_timer();

    // Spawn thread team once in main; single thread kicks off top task
    #pragma omp parallel
    {
        #pragma omp single
        {
            mergesort(x, temp, 0, (LEN - 1));
        }
    }

    stop_timer();
    time = elapsed_time();

#ifdef DEBUG
    printf("after sort:\n");
    for (i = 0; i < LEN; i++) printf("%d ", x[i]);
    printf("\n");
#endif

    printf("elapsed time = %lf (sec)\n", time);

    if (!check_sort(x, LEN, 1)) return 1;

    free(x);
    free(temp);
    return 0;
}

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