# Questions

- Given an unbalanced workload, would dynamic scheduling with many small chunks always run faster than running a static schedule with many small chunks? Im assuming static is where tasks are pre allocated to threads and threads that finish their task set does not help other threads. As for dynamic, threads all work on a pool of tasks of decreasing level of work (what about quantifying work of an io bound task rather than computational based task?).

- What's the benifit to being able to choose a scheduling method at runtime?


- if your task to run in parallel is represented as a linked list, where might the critical section be for ensuring threads work on different parts of the task?
    - id choose to make the read & write operation pair a critical section. This pair would show up whever a program checks if ( a task remains && a thread is available to do work)

- Why might a thread want to know how many other threads there are? What about a thread's number out of thread pool? If i had a parallel for loop, wouldnt only the loop or its containing context care to know? Is the high level concept of data / task parallelism be a good place to start when thinking about this?

- using the "for" vs "parallel for" pragma?
- why is it called a "single" pragma instead of something closer to "critical section" pragma? One argument for the "single" verbiage is that only a single thread can execute the following at a time.

- Given a pool of threads working on a set of tasks, when might I want an available thread to work on the next piece of code rather than wait for the other threads to finish up their tasks / the last of the set of tasks for the thread pool?
    - if a pool of threads don't share any state, would this be a potential time saving opportunity (nowait clause)?

