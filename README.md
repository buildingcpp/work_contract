# Work Contracts was presented at CppCon 2024
See here - https://youtu.be/oj-_vpZNMVw


# Work Contracts

Work Contracts are an alternative to traditional task based concurrency (tasks and task queues).  They are designed to be both easy to use and better suited for use in low latency applications.

README is a work in progress.  For details please see the /doc directory of this repo for slides from the CppCon 2024 presentation and "work_contract_fundamentals.pdf" for details.
See working examples under /src/executable/ for more detailed examples as well.  

</br></br></br>

------------------------------------------------------------------

**There are some great questions posted in the comment section of the youtube video.  Rather than answering them there I think it is better to answer them here in the README. I'll work to refine the answers over time to make them as clear as possible but will attempt to answer quickly in the short term.**


# Performance impact of all the atomics

Someone suggested that they have concerns with performance due to all the atomics.  In reality the number of atomics involved is actually quite low, especially when you compare it to 
lock free queues.  In the talk, I presented the signal tree as a basic binary tree for simplicity.  The actual implementation is a much more compact tree with many nodes packed together 
inside of std::atomic&lt;std::uint64_t&gt; counters and this greatly reduces the depth of the tree as well as the number of atomics involved in any schedule/select of a work contract.

Think about the leaf nodes, where the count is restricted to 0 or 1 (remember the count is the sum of the count of left and right children and at the leaf there is only that one node).
So for every 64 leaf nodes there is a single std::uint64_t representing the count of all 64 leaf nodes (the actual set/reset flags for 64 contracts).  To make matters simpler, at the
leaf node we don't even need a CAS operation to set or reset the count.  Instead we can use a wait free fetch_or and fetch_and to adjust the count of any of the 64 one bit counters.

Similarly, the 'parent' counters for each of these 64 bit 'leaf' counters is, itself, a uint64_t which contains eight sub counters, each of which is 7 bits wide and can each represent
a count from 0 to 64.  So a signal tree of depth two can represent 512 work contracts and only involves a single CAS operation and a single fetch_and when selecting a singal. And a single
atomic increment and a single fetch_or when setting a signal.

The code is capable of optimally packing counters for trees of greater depth as well, however, in practice I find 64 or 512 contracts per tree to be optimal.  If more contracts are needed
then the work_contract_group itself will simply create a larger vector of these 'small' trees.

In the benchmark there are 16K of work contracts which is really a series of small signal trees under the hood.  The results show that many small signal trees scales very well indeed and that 
atomic operations are kept to a minimum regardless of the number of available contracts.

A final point on this is to keep in mind that work contracts are intended to be really long lived 'logic' and not individual instances of a task being invoked.  For example, a socket might
use a work contract (the contract is to call recv when the socket is polled).  That's one contract per socket.  Not one contract per call to recv.  So you can go a long way with a small
number of contracts.  Another example might be a work contract that redraws the screen or recalculates some vector or something when the mouse scroll wheel is turned.  That's a single contract
regardless of how many times it is invoked to repeat that contract.


# Under what circumstances are queues a better choice than work contracts

To clarify, I argued that queues are a terrible choice for 'logic'.  But they are clearly useful for data.  Work Contracts often have an ingress and/or egress queue for the input and output
of the contract.  So queues are still very much needed.  And, since work contracts are guaranteed to be executed in a single threaded fashion (I have plans to allow MT access though), the
queue of choice is often a SPSC lock free queue, and that can greatly increase an application's performance if, without work contracts, the application had to use MPMC queues otherwise.

But to answer the question directly ... I don't know of a case where (if WCs are applicable) a queue would be a better choice.  I'm not saying there isn't such a situation but I haven't
really come across such a case given that queues are used for ingress and egress under the hood anyhow (if they are needed at all).  

I would be curious if someone could come up with examples.  Perhaps I'm seeing all situations as nails because I have a nice hammer I want to use?  (^:


# Code examples are too small and unreadable in the presentation

I know.  It turns out that putting together a presentation is harder than building the thing being presented!  At least for me it is.  (^:
I'll try to use a bigger font size in the future.


# Bias, work contract selection and scalability

Someone asked about scalability and bias related to contract selection.  I'm not quite sure what is being asked but I'll take a stab at answering and hopefully provide clarity.

Bias bits, by default, are used to ensure that each contract is invoked fairly.  As shown in the presentation, the default bias does quite well here.  I did suggest that *if* the
developer would prefer to bias the selection then they have that option by providing bias bits.  This overrides the deafult behavior of 'fair' selection.  But in general, I think
it's a better approach to have multiple work contract groups if you want to have a heavy bias. 

One of the core ideas of WC is to decouple the worker threads from the containers that have the contracts.  This means that you can have some threads servicing one group and
other threads servicing another group.  Then, each group uses the default fairness bias but you can determine which threads service which types of contracts at the level of the thread
group itself.  For example, you might have a work contract group dedicated to contracts that process socket reads and a thread group of 'socket reader threads', and a different 
work contract group for some other type of task like updating some table when parameters are changed and have a dedicated thread group for that purpose that only services that particular
work contract group, etc.

Work Contract Groups are very lightweight and only require N/2 RAM for N contracts.  So having multiple groups for different purposes is a reasonable thing to do and can make it easier
to manage which work contract types get priority or not.


# Isn't there heavy contention on the root node just like the front of a lock free queue?

This was asked by several people at CppCon.  I've been promising to clarify so here goes.

To achieve better performance, some MPMC queues (MoodyCamel in particular) use a series of sub queues which has the affect of spreading the contention across multiple queues.  This can
improve performance as it reduces contention.  But it does come with some serious shortcomings.  For instance, to preseve ordering any one thread must always push to the same sub queue.
This brings with it the challenge of ensuring that threads are evenly distributed across the sub queues or the benefit just doesn't materialize at all.  (I think this is why MoodyCamel
does so poorly on task fairness, by the way).

Work Contract Groups do something similar.  A Work Contract Group doesn't contain a single Signal Tree (or atleast it doesn't have to).  Instead, it contains a vector of smaller signal trees.
So there are many root nodes and this helps to spread the contention and reduce it.  In practice I've found that if there are four signal trees per thread then contention is typically reduced
to virtually zero.  

But the advantages of many signal trees is greater than the similar approach of many sub queues.  For one, a signal tree gives access to all leaf nodes at all times whereas the queue gives 
access only to the front.  More importantly, threads can access every subtree (scanning the root nodes like iterating over a vector) as they look for scheduled contracts.  But as pointed out
above, the same is not true for multiple subqueues.  Mostly because for tasks the ordering must be preserved in the queue whereas with work contracts, the ordering must be preserved only if an
individual work contract has an ingree or egress queue - which removes the problem of causing contention on the signal tree root node entirely.

I hope this answer clarifies things.  But I plan on giving a more detailed answer to this question when I have the time.  

But the TL;DR; of this question is that the differences in between queues and trees makes reducing the contention on the head vs the root very different problems.  And its much easier to address in the case of trees than it does for queues.
