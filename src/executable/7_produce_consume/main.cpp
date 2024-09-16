#include <library/work_contract.h>
#include <include/spsc_fixed_queue.h>

#include <iostream>
#include <thread>


//=============================================================================
template <typename T>
struct producer
{
    using pipe_type = T;

    template <typename ... Ts>
    producer(Ts && ... args):pipe_(std::forward<Ts>(args) ...){}

    bool produce
    (
        auto && value
    )
    {
        if (pipe_.push(std::forward<decltype(value)>(value)))
        {
            if (consumeContract_)
                consumeContract_.schedule();
            return true;
        }
        return false;
    }

    bool attach_consumer
    (
        std::invocable<typename T::type> auto && consumer,
        bcpp::work_contract_group & wcg
    )
    {
        if (consumeContract_)
            return false;
        consumeContract_ = wcg.create_contract(
                [this, consumer]
                (
                    auto & contractToken
                )
                {
                    consumer(pipe_.pop()); 
                    if (!pipe_.empty()) 
                        contractToken.schedule();
                },
                pipe_.empty() ? bcpp::work_contract::initial_state::unscheduled : bcpp::work_contract::initial_state::scheduled);
        return consumeContract_.is_valid();
    }

    bool empty() const{return pipe_.empty();}

private:
    pipe_type pipe_;
    bcpp::work_contract consumeContract_;  // Needs to be an atomic unique ptr to be safe joining while producing
};


template <typename T>
concept producer_concept = std::is_same_v<std::decay_t<T>, producer<typename std::decay_t<T>::pipe_type>>;


//=============================================================================
int main()
{
    bcpp::work_contract_group wcg;
    std::jthread workerThread([&](auto st){while (!st.stop_requested()) wcg.execute_next_contract();});

    using pipe_type = bcpp::spsc_fixed_queue<int>;
    producer<pipe_type> myProducer(1024);

    auto myConsumer = [](auto value){std::cout << "consumed " << value << "\n";};
    myProducer.attach_consumer(myConsumer, wcg);
    
    auto start = std::chrono::system_clock::now();
    for (auto i = 0; i < 16; ++i)
        while (!myProducer.produce(i))
            ;
    while (!myProducer.empty())
        ;
    return 0;
}
