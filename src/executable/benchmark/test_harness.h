#pragma once

#include <tbb/concurrent_queue.h>
#include <include/boost/lockfree/queue.hpp>
#include <concurrentqueue.h>
#include <mpmc_queue.h>
#include <library/work_contract.h>


enum class algorithm {boost_lockfree, tbb, moody_camel, es, work_contract};


template <algorithm, typename>
struct container;


template <typename T> 
struct container<algorithm::boost_lockfree, T>
{
    using task_type = T;
    container(std::size_t capacity):queue_(capacity * 2){}
    void push(std::int32_t value){while (!queue_.push(value));}
    auto pop(){std::int32_t result; while (!queue_.pop(result)); return result;}
    boost::lockfree::queue<std::int32_t> queue_;
};


template <typename T> 
struct container<algorithm::tbb, T>
{
    using task_type = T;
    container(std::size_t capacity):queue_(){}
    void push(std::int32_t value){queue_.push(value);}
    auto pop(){std::int32_t result; while (!queue_.try_pop(result)); return result;}
    tbb::concurrent_queue<std::int32_t> queue_;
};


template <typename T> 
struct container<algorithm::moody_camel, T>
{
    using task_type = T;
    container(std::size_t capacity):queue_(capacity * 2){}
    void push(std::int32_t value){while (!queue_.enqueue(value));}
    auto pop(){std::int32_t result; while (!queue_.try_dequeue(result)); return result;}
    moodycamel::ConcurrentQueue<std::int32_t> queue_;
};


template <typename T> 
struct container<algorithm::es, T>
{
    using task_type = T;
    container(std::size_t capacity):queue_(capacity * 2){}
    void push(std::int32_t value){while (!queue_.push(value));}
    auto pop(){std::int32_t result; while (!queue_.pop(result)); return result;}
    es::lockfree::mpmc_queue<std::int32_t> queue_;
};


template <typename T> 
struct container<algorithm::work_contract, T>
{
    using task_type = bcpp::work_contract;
    container(std::size_t capacity):workContractGroup_(capacity * 4){}
    auto create_contract(auto && task){return workContractGroup_.create_contract(task, bcpp::work_contract::initial_state::scheduled);}
    auto execute_next_contract(){return workContractGroup_.execute_next_contract();}
    bcpp::work_contract_group workContractGroup_;
};



template <algorithm T, typename T_>
class test_harness : private container<T, T_>
{
public:

    static auto constexpr is_queue = (T != algorithm::work_contract);
    using task_type = typename container<T, T_>::task_type;

    test_harness(std::size_t capacity) : container<T, T_>(capacity){}

    void add_task
    (
        std::invocable auto && task
    )
    {
        auto taskId = tasks_.size();
        if constexpr (is_queue)
        {
            // mpmc queues store simply the task id. 
            // the task itself is not stored in the queue for efficiency.
            tasks_.push_back(task);
            this->push(taskId);
        }
        else
        {
            // work contracts are not queues and therefore the task is actaully
            // stored within the work contract itself.
            tasks_.push_back(this->create_contract(
                [task, taskId](auto & token)
                {
                    task();                         // execute the task
                    token.schedule();               // reschedule this contract (like pushing to back of work queue again)
                    tlsExecutionCount[taskId]++;
                }));
        }
    }

    void process_next_task()
    {
        if constexpr (is_queue)
        {
            // all the mpmc based containers
            auto taskIndex = this->pop(); 
            tasks_[taskIndex](); 
            this->push(taskIndex);
            tlsExecutionCount[taskIndex]++;
        }
        else
        {
            // the work contract based container
            this->execute_next_contract();
        }
    }

private:

    std::vector<task_type>  tasks_;
};
