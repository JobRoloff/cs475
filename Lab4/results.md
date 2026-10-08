
## Sequential Merge Sort

```bash
./run_sweep.sh ./Merge_sort "1" 5
threads  median       reps
1        14.579418    5

./run_sweep.sh ./Merge_sort
threads  median       reps
1        14.630219    5
```

## Parallel - No Prune

./run_sweep.sh ./Merge_sort_parallelNP "1 2 3 4 5 6 7 8" 5
threads  median       reps
1        27.268791    5
2        63.832078    5
3        76.835962    5
4        109.542903   5
5        117.038030   5
6        133.709971   5
7        137.992916   5
8        148.775355   5

The above results show as number of threads increase, median time to complete the merge sort program on a list of 100,000 numbers increase.

I hypothesize the reason for performance degredation is due to the performance cost of sorting many small child arrays.

If branch depth increase in a resursive algorithm, more tasks will be created. At some point the overhead cost of creating individual tasks will out weigh the benefit of parallelization because each task will have a negligible amount of work to be done.

To improve performance, we can introduce the concept of a cutoff / pruning which reduces depth of the recursive tree, thereby ensuring a meaningful amount of work can be done within each task.

## Parallel - Prune

```bash
./run_sweep.sh ./Merge_sortP "1 2 3 4 5 6 7 8" 5
threads  median       reps
1        14.372151    5
2        7.481613     5
3        7.516801     5
4        6.157566     5
5        5.830491     5
6        4.189696     5
7        4.188811     5
8        4.103068     5
```

As shown in the above results, reducing tree depth improved latency as more threads were created.

