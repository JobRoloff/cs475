## Part 1

Sequential fibonacci sequence

```bash
blowfish:~/cs475/lab5$ ./Fib_count
fib(30) = 832040
OK
elapsed time = 0.001904 (sec)
blowfish:~/cs475/lab5$ ./Fib_count 35
fib(35) = 9227465
OK
elapsed time = 0.026096 (sec)
blowfish:~/cs475/lab5$ ./Fib_count 40
fib(40) = 102334155
OK
elapsed time = 0.237053 (sec)
blowfish:~/cs475/lab5$ ./Fib_count 44
fib(44) = 701408733
OK
elapsed time = 1.636161 (sec)
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count "1" 5 25
threads  median       reps
1        0.000230     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count "1" 5 30
threads  median       reps
1        0.002349     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count "1" 5 35
threads  median       reps
1        0.023264     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count "1" 5 40
threads  median       reps
1        0.237779     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count "1" 5 44
threads  median       reps
1        1.638965     5
```

### Timing table

Median of 5 runs (`run_sweep.sh`). Every run printed OK.

| Version | n | Median (s) |
|---|---|---|
| Fib_count (serial) | 25 | 0.000230 |
| Fib_count (serial) | 30 | 0.002349 |
| Fib_count (serial) | 35 | 0.023264 |
| Fib_count (serial) | 40 | 0.237779 |
| Fib_count (serial) | 44 | 1.638965 |

## Part 2

Parallel fibonacci sequence using naive tasks

naive task implementation is where recursive summation creates a new task per child

```bash
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=4 ./Fib_count_race 30; done
fib(30) = 832040
result[0] = 514163, expected 514229
result[1] = 830961, expected 832040
result[2] = 514002, expected 514229
result[3] = 317759, expected 317811
result[4] = 196401, expected 196418
result[5] = 121376, expected 121393
result[6] = 75007, expected 75025
result[7] = 46363, expected 46368
result[8] = 28656, expected 28657
total = 2691055, expected 2692537
WRONG
elapsed time = 1.391759 (sec)
fib(30) = 832040
result[0] = 514191, expected 514229
result[1] = 829462, expected 832040
result[2] = 514203, expected 514229
result[3] = 317717, expected 317811
result[4] = 196262, expected 196418
result[5] = 121365, expected 121393
result[6] = 75011, expected 75025
result[7] = 46362, expected 46368
result[8] = 28656, expected 28657
total = 2689596, expected 2692537
WRONG
elapsed time = 1.306763 (sec)
fib(30) = 832040
result[0] = 514196, expected 514229
result[1] = 831511, expected 832040
result[2] = 514153, expected 514229
result[3] = 317777, expected 317811
result[4] = 196388, expected 196418
result[5] = 121341, expected 121393
result[6] = 75011, expected 75025
result[7] = 46361, expected 46368
result[8] = 28653, expected 28657
result[9] = 17707, expected 17711
total = 2691754, expected 2692537
WRONG
elapsed time = 1.332943 (sec)
fib(30) = 832040
result[0] = 514047, expected 514229
result[1] = 830261, expected 832040
result[2] = 513981, expected 514229
result[3] = 317536, expected 317811
result[4] = 196292, expected 196418
result[5] = 121338, expected 121393
result[6] = 74999, expected 75025
result[7] = 46360, expected 46368
result[8] = 28654, expected 28657
total = 2689835, expected 2692537
WRONG
elapsed time = 1.214029 (sec)
fib(30) = 832040
result[0] = 514070, expected 514229
result[1] = 828891, expected 832040
result[2] = 514135, expected 514229
result[3] = 317680, expected 317811
result[4] = 196382, expected 196418
result[5] = 121359, expected 121393
result[6] = 75006, expected 75025
result[7] = 46357, expected 46368
result[8] = 28650, expected 28657
result[10] = 10945, expected 10946
result[12] = 4180, expected 4181
total = 2688895, expected 2692537
WRONG
elapsed time = 1.302715 (sec)
```

```bash
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_race 30; done
fib(30) = 832040
result[0] = 514223, expected 514229
result[1] = 832005, expected 832040
result[2] = 514190, expected 514229
result[3] = 317788, expected 317811
result[4] = 196408, expected 196418
result[5] = 121386, expected 121393
result[6] = 75023, expected 75025
result[7] = 46366, expected 46368
total = 2692413, expected 2692537
WRONG
elapsed time = 1.925411 (sec)
fib(30) = 832040
result[0] = 514216, expected 514229
result[1] = 831956, expected 832040
result[2] = 514121, expected 514229
result[3] = 317636, expected 317811
result[4] = 196334, expected 196418
result[5] = 121353, expected 121393
result[6] = 75009, expected 75025
result[7] = 46361, expected 46368
result[9] = 17708, expected 17711
total = 2692007, expected 2692537
WRONG
elapsed time = 1.948186 (sec)
fib(30) = 832040
result[0] = 514216, expected 514229
result[1] = 831964, expected 832040
result[2] = 514173, expected 514229
result[3] = 317793, expected 317811
result[4] = 196407, expected 196418
result[5] = 121392, expected 121393
result[6] = 75024, expected 75025
result[7] = 46367, expected 46368
result[8] = 28656, expected 28657
result[9] = 17709, expected 17711
result[10] = 10945, expected 10946
total = 2692356, expected 2692537
WRONG
elapsed time = 1.834439 (sec)
fib(30) = 832040
result[0] = 514220, expected 514229
result[1] = 831994, expected 832040
result[2] = 514161, expected 514229
result[3] = 317724, expected 317811
result[4] = 196386, expected 196418
result[5] = 121375, expected 121393
result[6] = 75021, expected 75025
result[7] = 46367, expected 46368
total = 2692272, expected 2692537
WRONG
elapsed time = 1.928585 (sec)
fib(30) = 832040
result[0] = 514208, expected 514229
result[1] = 831838, expected 832040
result[2] = 514144, expected 514229
result[3] = 317756, expected 317811
result[4] = 196399, expected 196418
result[5] = 121378, expected 121393
result[6] = 75023, expected 75025
result[7] = 46367, expected 46368
result[8] = 28656, expected 28657
result[9] = 17710, expected 17711
result[10] = 10945, expected 10946
total = 2692134, expected 2692537
WRONG
elapsed time = 1.936787 (sec)
```

### Timing table

Median of the 5 runs above. Speedup is relative to the serial `Fib_count` median at n = 30 (0.002349 s).

| Version | n | Threads | Median (s) | Min (s) | Max (s) | Speedup vs serial | Failed runs |
|---|---|---|---|---|---|---|---|
| Fib_count_race | 30 | 4 | 1.306763 | 1.214029 | 1.391759 | 0.0018 | 5 / 5 |
| Fib_count_race | 30 | 8 | 1.928585 | 1.834439 | 1.948186 | 0.0012 | 5 / 5 |

## Part 3

Parallel fibonacci sequence but with poor cache locality / thrashing

```bash
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=4 ./Fib_count_atomic 30; done
fib(30) = 832040
OK
elapsed time = 1.240868 (sec)
fib(30) = 832040
OK
elapsed time = 1.274892 (sec)
fib(30) = 832040
OK
elapsed time = 1.347820 (sec)
fib(30) = 832040
OK
elapsed time = 1.324616 (sec)
fib(30) = 832040
OK
elapsed time = 1.240609 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_atomic 30; done
fib(30) = 832040
OK
elapsed time = 1.923859 (sec)
fib(30) = 832040
OK
elapsed time = 2.089403 (sec)
fib(30) = 832040
OK
elapsed time = 1.948571 (sec)
fib(30) = 832040
OK
elapsed time = 1.924072 (sec)
fib(30) = 832040
OK
elapsed time = 2.018433 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=4 ./Fib_count_atomic 25; done
fib(25) = 75025
OK
elapsed time = 0.122175 (sec)
fib(25) = 75025
OK
elapsed time = 0.135034 (sec)
fib(25) = 75025
OK
elapsed time = 0.126276 (sec)
fib(25) = 75025
OK
elapsed time = 0.122030 (sec)
fib(25) = 75025
OK
elapsed time = 0.128309 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_atomic 25; done
fib(25) = 75025
OK
elapsed time = 0.169247 (sec)
fib(25) = 75025
OK
elapsed time = 0.178302 (sec)
fib(25) = 75025
OK
elapsed time = 0.208379 (sec)
fib(25) = 75025
OK
elapsed time = 0.193432 (sec)
fib(25) = 75025
OK
elapsed time = 0.183637 (sec)
```

```bash
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic "1 2 4 8" 5 25
threads  median       reps
1        0.020698     5
2        0.081727     5
4        0.130893     5
8        0.188622     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic "1 2 4 8" 5 30
threads  median       reps
1        0.202690     5
2        0.874583     5
4        1.262899     5
8        1.892442     5
```

### Timing table

Median of 5 runs (`run_sweep.sh`); every run printed OK. Speedup is serial `Fib_count` time / parallel time (serial: n = 25 0.000230 s, n = 30 0.002349 s).

| Version | n | Threads | Median (s) | Speedup vs serial |
|---|---|---|---|---|
| Fib_count_atomic | 25 | 1 | 0.020698 | 0.0111 |
| Fib_count_atomic | 25 | 2 | 0.081727 | 0.0028 |
| Fib_count_atomic | 25 | 4 | 0.130893 | 0.0018 |
| Fib_count_atomic | 25 | 8 | 0.188622 | 0.0012 |
| Fib_count_atomic | 30 | 1 | 0.202690 | 0.0116 |
| Fib_count_atomic | 30 | 2 | 0.874583 | 0.0027 |
| Fib_count_atomic | 30 | 4 | 1.262899 | 0.0019 |
| Fib_count_atomic | 30 | 8 | 1.892442 | 0.0012 |

## Part 4

Parallel fibonacci sequence but with private data copies to mitigate thrashing of shared data 

```bash
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=4 ./Fib_count_red 25; done
fib(25) = 75025
OK
elapsed time = 0.162939 (sec)
fib(25) = 75025
OK
elapsed time = 0.161830 (sec)
fib(25) = 75025
OK
elapsed time = 0.157143 (sec)
fib(25) = 75025
OK
elapsed time = 0.161383 (sec)
fib(25) = 75025
OK
elapsed time = 0.161741 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_red 25; done
fib(25) = 75025
OK
elapsed time = 0.209401 (sec)
fib(25) = 75025
OK
elapsed time = 0.203702 (sec)
fib(25) = 75025
OK
elapsed time = 0.201792 (sec)
fib(25) = 75025
OK
elapsed time = 0.215442 (sec)
fib(25) = 75025
OK
elapsed time = 0.201086 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=4 ./Fib_count_red 30; done
fib(30) = 832040
OK
elapsed time = 1.538794 (sec)
fib(30) = 832040
OK
elapsed time = 1.680647 (sec)
fib(30) = 832040
OK
elapsed time = 1.780944 (sec)
fib(30) = 832040
OK
elapsed time = 1.660916 (sec)
fib(30) = 832040
OK
elapsed time = 1.879712 (sec)
blowfish:~/cs475/lab5$ for i in 1 2 3 4 5; do OMP_NUM_THREADS=8 ./Fib_count_red 30; done
fib(30) = 832040
OK
elapsed time = 2.117489 (sec)
fib(30) = 832040
OK
elapsed time = 2.337715 (sec)
fib(30) = 832040
OK
elapsed time = 2.149266 (sec)
fib(30) = 832040
OK
elapsed time = 2.199955 (sec)
fib(30) = 832040
OK
elapsed time = 2.128151 (sec)
```

```bash
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red "1 2 4 8" 5 25
threads  median       reps
1        0.018971     5
2        0.092366     5
4        0.160408     5
8        0.212990     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red "1 2 4 8" 5 30
threads  median       reps
1        0.209671     5
2        1.003100     5
4        1.649378     5
8        2.116696     5
```

### Timing table

Median of 5 runs (`run_sweep.sh`); every run printed OK. Speedup is serial `Fib_count` time / parallel time (serial: n = 25 0.000230 s, n = 30 0.002349 s). "Speedup vs atomic" is atomic time / reduction time, so values below 1 mean the reduction is slower.

| Version | n | Threads | Median (s) | Speedup vs serial | Speedup vs atomic |
|---|---|---|---|---|---|
| Fib_count_red | 25 | 1 | 0.018971 | 0.0121 | 1.09 |
| Fib_count_red | 25 | 2 | 0.092366 | 0.0025 | 0.88 |
| Fib_count_red | 25 | 4 | 0.160408 | 0.0014 | 0.82 |
| Fib_count_red | 25 | 8 | 0.212990 | 0.0011 | 0.89 |
| Fib_count_red | 30 | 1 | 0.209671 | 0.0112 | 0.97 |
| Fib_count_red | 30 | 2 | 1.003100 | 0.0023 | 0.87 |
| Fib_count_red | 30 | 4 | 1.649378 | 0.0014 | 0.77 |
| Fib_count_red | 30 | 8 | 2.116696 | 0.0011 | 0.89 |

## Part 5

Parallel fibonacci sequence but with limited minimum amount of work done within a task

Earlier runs:

```bash
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 4 8" 5 40 25
threads  median       reps
1        0.387904     5
2        0.251452     5
4        0.165484     5
8        0.100336     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 4 8" 5 40 30
threads  median       reps
1        0.391254     5
2        0.236425     5
4        0.184019     5
8        0.101380     5

blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 4 8" 5 40 25
threads  median       reps
1        2.148771     5
2        3.553783     5
4        4.132785     5
8        4.593216     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 4 8" 5 40 30
threads  median       reps
1        2.141452     5
2        3.707472     5
4        4.207123     5
8        4.564564     5

```

Full sweep:

```bash
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 35 15
threads  median       reps
1        0.035679     5
2        0.041606     5
3        0.045040     5
4        0.046988     5
5        0.048878     5
6        0.052562     5
7        0.053005     5
8        0.057091     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 35 20
threads  median       reps
1        0.034721     5
2        0.023149     5
3        0.023269     5
4        0.018415     5
5        0.017172     5
6        0.012770     5
7        0.012428     5
8        0.010923     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 35 25
threads  median       reps
1        0.035316     5
2        0.022111     5
3        0.022295     5
4        0.015241     5
5        0.011955     5
6        0.010939     5
7        0.010825     5
8        0.009812     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 35 30
threads  median       reps
1        0.034553     5
2        0.021485     5
3        0.021555     5
4        0.015119     5
5        0.011683     5
6        0.010630     5
7        0.010635     5
8        0.009599     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 40 15
threads  median       reps
1        0.386967     5
2        0.444924     5
3        0.470607     5
4        0.487069     5
5        0.496734     5
6        0.551024     5
7        0.519160     5
8        0.548596     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 40 20
threads  median       reps
1        0.382123     5
2        0.253440     5
3        0.252839     5
4        0.175030     5
5        0.162230     5
6        0.131344     5
7        0.131031     5
8        0.112377     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 40 25
threads  median       reps
1        0.387540     5
2        0.251731     5
3        0.214133     5
4        0.185283     5
5        0.150091     5
6        0.115767     5
7        0.112524     5
8        0.104826     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 40 30
threads  median       reps
1        0.381439     5
2        0.236624     5
3        0.204899     5
4        0.184435     5
5        0.160516     5
6        0.120840     5
7        0.117277     5
8        0.096044     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 44 15
threads  median       reps
1        2.648494     5
2        3.030518     5
3        3.193510     5
4        3.227522     5
5        3.203077     5
6        3.652154     5
7        3.503748     5
8        3.780387     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 44 20
threads  median       reps
1        2.618046     5
2        1.728400     5
3        1.519352     5
4        1.337698     5
5        1.018715     5
6        0.895726     5
7        0.884961     5
8        0.759395     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 44 25
threads  median       reps
1        2.655896     5
2        1.657670     5
3        1.423330     5
4        1.193265     5
5        1.126571     5
6        0.822343     5
7        0.820123     5
8        0.706610     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_red_cutoff "1 2 3 4 5 6 7 8" 5 44 30
threads  median       reps
1        2.612507     5
2        1.677972     5
3        1.509009     5
4        1.011523     5
5        1.145199     5
6        0.813068     5
7        0.777961     5
8        0.688893     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 35 15
threads  median       reps
1        0.198567     5
2        0.322402     5
3        0.338629     5
4        0.379032     5
5        0.372694     5
6        0.417736     5
7        0.420690     5
8        0.438623     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 35 20
threads  median       reps
1        0.201159     5
2        0.328288     5
3        0.363273     5
4        0.378293     5
5        0.399068     5
6        0.403086     5
7        0.432531     5
8        0.448832     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 35 25
threads  median       reps
1        0.196790     5
2        0.353492     5
3        0.344665     5
4        0.394327     5
5        0.421268     5
6        0.432552     5
7        0.408947     5
8        0.414777     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 35 30
threads  median       reps
1        0.195459     5
2        0.348530     5
3        0.338706     5
4        0.391581     5
5        0.408810     5
6        0.421192     5
7        0.429431     5
8        0.415744     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 40 15
threads  median       reps
1        2.172881     5
2        3.485497     5
3        3.653497     5
4        4.350428     5
5        4.264607     5
6        4.285540     5
7        4.702841     5
8        4.508836     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 40 20
threads  median       reps
1        2.144281     5
2        3.627142     5
3        3.549646     5
4        3.996284     5
5        4.256079     5
6        4.506774     5
7        4.696426     5
8        4.599792     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 40 25
threads  median       reps
1        2.147785     5
2        3.703230     5
3        3.913175     5
4        4.473242     5
5        4.175907     5
6        4.459741     5
7        4.481926     5
8        4.616015     5
blowfish:~/cs475/lab5$ ./run_sweep.sh ./Fib_count_atomic_cutoff "1 2 3 4 5 6 7 8" 5 40 30
threads  median       reps
1        2.148983     5
2        3.564857     5
3        3.811537     5
4        4.296197     5
5        4.171239     5
6        4.653254     5
7        4.874862     5
8        4.642821     5
```

### Timing tables

Median of 5 runs (`run_sweep.sh`) in seconds; the fastest entry per table is in bold. Every run printed OK.

**Fib_count_red_cutoff, n = 35** (serial `Fib_count`: 0.023264 s)

| Cutoff | 1 thr | 2 thr | 3 thr | 4 thr | 5 thr | 6 thr | 7 thr | 8 thr |
|---|---|---|---|---|---|---|---|---|
| 15 | 0.035679 | 0.041606 | 0.045040 | 0.046988 | 0.048878 | 0.052562 | 0.053005 | 0.057091 |
| 20 | 0.034721 | 0.023149 | 0.023269 | 0.018415 | 0.017172 | 0.012770 | 0.012428 | 0.010923 |
| 25 | 0.035316 | 0.022111 | 0.022295 | 0.015241 | 0.011955 | 0.010939 | 0.010825 | 0.009812 |
| 30 | 0.034553 | 0.021485 | 0.021555 | 0.015119 | 0.011683 | 0.010630 | 0.010635 | **0.009599** |

**Fib_count_red_cutoff, n = 40** (serial `Fib_count`: 0.237779 s)

| Cutoff | 1 thr | 2 thr | 3 thr | 4 thr | 5 thr | 6 thr | 7 thr | 8 thr |
|---|---|---|---|---|---|---|---|---|
| 15 | 0.386967 | 0.444924 | 0.470607 | 0.487069 | 0.496734 | 0.551024 | 0.519160 | 0.548596 |
| 20 | 0.382123 | 0.253440 | 0.252839 | 0.175030 | 0.162230 | 0.131344 | 0.131031 | 0.112377 |
| 25 | 0.387540 | 0.251731 | 0.214133 | 0.185283 | 0.150091 | 0.115767 | 0.112524 | 0.104826 |
| 30 | 0.381439 | 0.236624 | 0.204899 | 0.184435 | 0.160516 | 0.120840 | 0.117277 | **0.096044** |

**Fib_count_red_cutoff, n = 44** (serial `Fib_count`: 1.638965 s)

| Cutoff | 1 thr | 2 thr | 3 thr | 4 thr | 5 thr | 6 thr | 7 thr | 8 thr |
|---|---|---|---|---|---|---|---|---|
| 15 | 2.648494 | 3.030518 | 3.193510 | 3.227522 | 3.203077 | 3.652154 | 3.503748 | 3.780387 |
| 20 | 2.618046 | 1.728400 | 1.519352 | 1.337698 | 1.018715 | 0.895726 | 0.884961 | 0.759395 |
| 25 | 2.655896 | 1.657670 | 1.423330 | 1.193265 | 1.126571 | 0.822343 | 0.820123 | 0.706610 |
| 30 | 2.612507 | 1.677972 | 1.509009 | 1.011523 | 1.145199 | 0.813068 | 0.777961 | **0.688893** |

**Fib_count_atomic_cutoff, n = 35** (serial `Fib_count`: 0.023264 s)

| Cutoff | 1 thr | 2 thr | 3 thr | 4 thr | 5 thr | 6 thr | 7 thr | 8 thr |
|---|---|---|---|---|---|---|---|---|
| 15 | 0.198567 | 0.322402 | 0.338629 | 0.379032 | 0.372694 | 0.417736 | 0.420690 | 0.438623 |
| 20 | 0.201159 | 0.328288 | 0.363273 | 0.378293 | 0.399068 | 0.403086 | 0.432531 | 0.448832 |
| 25 | 0.196790 | 0.353492 | 0.344665 | 0.394327 | 0.421268 | 0.432552 | 0.408947 | 0.414777 |
| 30 | **0.195459** | 0.348530 | 0.338706 | 0.391581 | 0.408810 | 0.421192 | 0.429431 | 0.415744 |

**Fib_count_atomic_cutoff, n = 40** (serial `Fib_count`: 0.237779 s)

| Cutoff | 1 thr | 2 thr | 3 thr | 4 thr | 5 thr | 6 thr | 7 thr | 8 thr |
|---|---|---|---|---|---|---|---|---|
| 15 | 2.172881 | 3.485497 | 3.653497 | 4.350428 | 4.264607 | 4.285540 | 4.702841 | 4.508836 |
| 20 | **2.144281** | 3.627142 | 3.549646 | 3.996284 | 4.256079 | 4.506774 | 4.696426 | 4.599792 |
| 25 | 2.147785 | 3.703230 | 3.913175 | 4.473242 | 4.175907 | 4.459741 | 4.481926 | 4.616015 |
| 30 | 2.148983 | 3.564857 | 3.811537 | 4.296197 | 4.171239 | 4.653254 | 4.874862 | 4.642821 |

### Speedup for the best cutoff (30)

Cutoff 30 gives the fastest 8-thread time for n = 35, 40 and 44. Speedup is serial `Fib_count` time / parallel time.

| Version | n | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|---|
| Fib_count_red_cutoff | 35 | 0.67 | 1.08 | 1.08 | 1.54 | 1.99 | 2.19 | 2.19 | 2.42 |
| Fib_count_red_cutoff | 40 | 0.62 | 1.00 | 1.16 | 1.29 | 1.48 | 1.97 | 2.03 | 2.48 |
| Fib_count_red_cutoff | 44 | 0.63 | 0.98 | 1.09 | 1.62 | 1.43 | 2.02 | 2.11 | 2.38 |
| Fib_count_atomic_cutoff | 35 | 0.12 | 0.07 | 0.07 | 0.06 | 0.06 | 0.06 | 0.05 | 0.06 |
| Fib_count_atomic_cutoff | 40 | 0.11 | 0.07 | 0.06 | 0.06 | 0.06 | 0.05 | 0.05 | 0.05 |

Observations:

- The reduction version with cutoff 30 reaches a speedup of 2.38 over serial at n = 44 on 8 threads. On 1 thread it is about 1.6× slower than serial `Fib_count`, which caps the speedup well below 8.
- Cutoffs 20–30 behave similarly. Cutoff 15 gets slower as threads are added: it creates many more small tasks, each with its own private copy of `result` to set up and combine.
- The atomic version gets slower with more threads at every cutoff, because every increment is an atomic update on the same shared array. At 8 threads with cutoff 30 it is 43× (n = 35) and 48× (n = 40) slower than the reduction version.
