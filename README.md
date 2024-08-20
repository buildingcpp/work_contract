**Creating a `work_contract_tree`:**

`work_contract_tree` can be either blocking or non-blocking and `work_contract`s associated with that `work_contract_tree` will be of that same type.

    namespace bcpp
    {

        enum class synchronization_mode : std::uint32_t
        {
            synchronous     = 0,
            blocking        = synchronous,
            sync            = synchronous,
            asynchronous    = 1,
            non_blocking    = asynchronous,
            async           = asynchronous
        };

    } // namespace bcpp

'work_contract_tree' is specialized by a `synchronization_mode` and the constructor takes a single argument which is the capacity (number of `work_contract`s
that the instance of the `work_contract_tree` can contain).

    bcpp::work_contract_tree<>
