
## Sequential Merge Sort

```bash
./run_sweep.sh ./Merge_sort "1" 5
threads  median       reps
1        14.579418    5

./run_sweep.sh ./Merge_sort
threads  median       reps
1        14.630219    5
```

## Parallel

```bash
./run_sweep.sh ./Merge_sort_parallel "1 2 3 4 5 6
 7 8" 5
threads  median       reps
1        14.430897    5
2        7.506944     5
3        7.522874     5
4        5.839655     5
5        5.579330     5
6        4.196249     5
7        4.203095     5
8        3.988248     5
```