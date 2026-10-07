# Lab 4 OMP Tasks: Merge Sort

Download and untar the provided tasks.tar file. This is the starting point for your experiments.

If needed be remind yourself what merge sort is with these fun animations

1. Time sequential code

Start by running the sequential version on a quiet fish machine


Run it a few times, and get a median run time for this. (./run_sweep.sh ./Merge_sort "1" 5 does this for you.)
Test your code by running it with a small input size, e.g., Merge_sort_debug 100. For all timing experiments run your codes with the default (no explicit size on the command line) size of 100,000,000.

```bash
make
chmod u+x run_sweep.sh
Merge_sort_debug 100
```

2. Make it parallel

Copy the sequential Merge_sort.c code into Merge_sort_parallel.c.
Use OMP task parallelism to parallelize Merge_sort_parallel.c (leave Merge_sort.c alone for reference).
Use an OMP parallel pragma to spawn threads in the main program. This is similar to the nfib(n) example given in the OpenMP tasks reference posted with this lab.
Spawn two new tasks, one for each recursive mergesort call.
Adjust your Makefile. Make sure that you use -O3 flag to compile all the files
Keep in mind: the pragma parallel should only be executed once. Also, don't merge the left and right halves of the array until they are sorted, i.e. you need a taskwait.

3. First parallel version: no pruning

Do not prune the task tree yet.
Call this code Merge_sortNP (for not pruned).
Run the parallel code for 1, 2, 3, 4, 5, 6, 7, 8 threads.
See how the run times compare to the sequential version.
Why do you think you are getting the results you are?
Think about how many tasks are spawned.

4. Second parallel version: pruning

Prune your code by only spawning sufficiently large tasks.
Call this code Merge_sortP.
Run the parallel code for 1, 2, 3, 4, 5, 6, 7, 8 threads.
Experiment with tasks sizes and create a speedup curve for your preferred task size.
See how the run times compare to the sequential and unpruned versions.
Think about how many tasks are spawned now.

5. Third parallel version: spawning only one task

Change your code to spawn only one task for one of the recursive calls, and let the other recursive call be done by the parent task.
Call this code Merge_sortS.
Experiment with it.
What are your observations?

6. Fourth parallel version

For the sake of completeness, it would be useful to gather data for the version where you spawn only one task, and do no pruning.
Call this code Merge_sortSNP.

7. Write up and submit

Summarize your observations on OMP tasks in a report with your run times and your speedup curve.
Submit the report together with your four source files and your Makefile, as a single .zip or .tar file.

## Questions you may consider in your write-up

How many tasks does each version create, and how much work does one of them do?
Did adding threads do what you expected in each version?
What did changing the task size change, and over what range?
Where does your speedup flatten out, and what is still running sequentially there?
The four versions differ in two independent ways. Does each change matter as much in one combination as in the other?
How does your parallel code running on a single thread compare with the sequential version, and why?
Which of your versions would you keep if you had to pick one, and what would you measure next if you had more time?