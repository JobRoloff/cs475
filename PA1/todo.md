## Background
We're building a program to analyze the preformance impact of incremental changes to a program's OpenMP Schedule Clause usage.

This requires the following parts:
- A Preformance Analyzer 
- A Preformance Reporter

This program will be used as follows:
1. In the CLI, a user passes in the following
    a. output report name
    b. report description
2. The program creates a file containing key performance metrics of the Mandpelbrot.c program

## Implementation Notes

- analysis functions should represent generic performance and NOT be named after the changes made to the program. This looks like using names such as "_calculate_speedup" rather than "_calculate_dynamic_speedup()"

## TODO

[ ] Write analysis methods with non specific naming
