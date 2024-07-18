# Work Contracts


A novel lock free, _(mostly)_ wait free approach to async/parallel task management with significantly superior performance compared to task queues.

- Lock free architecture
- Wait free for task scheduling, _(mostly)_ wait free for task selection
- Scalable architecture supporting limitless number of scheduled contracts
- Highly deterministic contract scheduling measuring in the low single digit nanoseconds
- Exceptionally fair contract _(task)_ selection 

Documentation: https://buildingcpp.com/work_contract.htm

# Quantifying Performance:
Comparing async task scheduling, selection and execution: Work Contracts vs highly performant traditional "task queues."  _(TBB, Boost Lock Free Queue, and Moody Camel)_

**Latency is not the same thing as throughput**.  There is a significant difference between being able to perform tasks at a high rate and executing a single task as quickly as possible.  In a lot of environments highly deterministic latency for execution of a single task is far more important than being able to execute as many tasks as is possible within a certain time frame.  For instance, trading engines need to be able to execute infrequent trades as quickly as possible, rather than executing as many trades as possible.

This benchmark is intended to measure both for throughput as well as latency.  The "Low Contention" benchmark more closely represents **latency** for infrequent tasks whereas "High Contention" more closely measures **throughput**, with the "Medium Contention" benchmark attempting to measure a reasonable combination of the two.  

The "Maximum Content" benchmark more strongly represents the latency involved with queueing and dequeing.

Across each category the results indicate that the "Work Contract" performs significantly better than traditional "Lockless/Concurrent Task Queue" approach.  Most notably, due to the exceptionally low latency involved in scheduling and selecting tasks in the Work Contract solution, we see that the overall amount of tasks completed continues to increase as the number of threads increases.  Whereas, for all queue based solutions, the overall amount of tasks completed tends to peak at somewhere between 2 and 4 threads with additional threads failing to increase the overall amount of work completed entirely.


# Testing Environment:

 - Benchmark is run on a _"13th Gen Intel(R) i9-13900HX"_  
 - For the sake of consistency only the _"efficient"_ CPUs where used (as there are more of them than there are _"Performance"_ cores).
 - CPU isolation was used to isolate each of the "efficient" cores and hyperthreading was disabled.  
 - During the tests each thread was bound to a unqiue, isolated core.
 - I **did not** bother to schedule interrupt affinities nor enable _"Huge Pages"_. 


# Benchmark:

The benchmark consists of four general tests.  Each test involves a simple task of calculating prime numbers from 2 to N using the "Seive of Eratosthenes".  By increasing N each task takes longer to complete which results in fewer tasks completed overall and reducing the contention placed on the underlying task queue (or task tree in the case of Work Contracts).  This task was also selected because it eliminates contention for non-task related resources such as cache/memory etc between worker threads and therefore helps to eliminate noise in the results which are not a direct measurement of the performance of the task management system itself.

The four tests are:
  - **Maximum Contention:**  N = 0. Effectively creating an empty task and generating the maximum contention.  Used to determine how much overhead each solution introduces in order to provide _"lock free"_ concurrency/parallelism.
  - **High Contention:**  N = 100. Creates a task which was measured to take ~190ns.  Contention is reduced _somewhat_ but is still fairly high.  Used to determine which solutions are sufficiently scalable for use with fairly low duration tasks.
  - **Medium Contention:** N = 300. Tasks were measured to take ~700ns.  Contention was reduced to what _might_ be considered an average task duration in a typical usage case.
  - **Low Contention:** N = 1000. Tasks were measured to take ~3.1us.  Contention was reduced further to an almost insignificant amount compared to the average task duration.

During the tests a fixed number of tasks were created (1024 in this case) and enqueued/scheduled for execution.  Worker threads then processed the tasks and immediately re-enqueued/re-scheduled the task again before moving on to process the next task. This results in a steady flow of tasks for the duration of the test.  The process looks something like:

```
while (!exitTest)
{
    auto task = select_task();
    task();
    schedule_task(task);
}
```


Each of the four tests was run repeatedly with the number of worker threads increasing from 2 to 16.  The test then measures:
  - **Average tasks/per thread/per second** 
  - **Coefficient of variation**:
    - **Individual Task Execution** - Measures the fairness of task selection _(i.e. were tasks executed evenly or were some tasks executed more/less frequently than the others?)_.
    - **Individual Thread Execution** - Measures how evenly the work was distributed across the cores _(i.e. was work spread evenly across the cores or were some cores doing more/less work than others?)_.
