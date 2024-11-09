# Work Contract

**[WIP]**


Work Contract is an alternative to traditional task based concurrency (tasks and task queues).  It is designed to be both easy to use and better suited for use in low latency applications.

Like traditional 'tasks', Work Contracts are callables.  However, unlike tasks, Work Contracts are re-invocable and support an optional 'destructor' task which is invoked when the contract is destroyed.  Both the primary work contract callable, as well as the destructor callable are executed asynchronously.

The Work Contract approach is designed to work with a lock free, often wait free, structure called a Signal Tree, rather than the traditional queue based approach.  Signal Trees are incredibly efficient and easily outperform even the most efficient implemenation of lock free MPMC queues.

Where traditional tasks usually combine the logic (task function) and the data (input for that fuction), Work Contracts decouple the logic and data such that the Work Contract contains only the logic.  Data, if needed, is typically retreived by means of lock free SPSC queues which are captured with the primary work contract callable.