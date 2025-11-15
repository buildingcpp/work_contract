#pragma once

#include <library/work_contract.h>

#include <syncstream>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>


namespace example_common
{

    using namespace bcpp;


    //=========================================================================
    auto load_paths
    (

        std::filesystem::path directory
    ) -> std::vector<std::filesystem::path>
    {
        // create list of files to process 
        std::vector<std::filesystem::path> paths;
        for (auto const & entry : std::filesystem::directory_iterator{directory})
            if (entry.is_regular_file())
                paths.push_back(entry.path());
        return paths;
    }


    //=========================================================================
    // task which counts the number of occurances of the specified character (the target)
    // Do this in stages, reading up to 256 bytes per pass and then counting the occurances of 
    // the target within that 256 bytes, then reading an additional 256 bytes ... continue
    // until entire file is processed.
    class char_count_task
    {
    public:

        char_count_task
        (
            std::filesystem::path path,
            char target
        ):
            path_(path),
            target_(target),
            stream_(std::make_shared<std::ifstream>(path_, std::ios_base::binary | std::ios_base::in)),
            buffer_(256)
        {
        }

        void operator()
        (
        ) noexcept
        {
            switch (state_)
            {
                case state::load:
                {
                    // read up to 256 bytes
                    stream_->read(buffer_.data(), buffer_.capacity());
                    auto bytesRead = stream_->gcount();
                    bytesRead_ += bytesRead;
                    buffer_.resize(bytesRead);
                    state_ = (buffer_.empty()) ? state::done : state::process;
                    break;
                }
                case state::process:
                {
                    // process the bytes in the buffer
                    for (auto c : buffer_)
                        count_ += (c == target_);
                    state_ = state::load;
                    break;
                }
                case state::done:
                {
                    // end of file
                    std::osyncstream(std::cout) << "Path = " << path_ << ", bytes read = " << bytesRead_ << ", count = " << count_ << "\n";
                    this_contract::release();
                    break;
                }
            }
            // once we complete the current state we schedule again to continue
            // it doesn't matter if we are 'done' and have called bcpp::work_contract::release already
            // as calling schedule on a released contract is idempotent.
            this_contract::schedule();
        }

    private:

        // there are three states for this task
        // load: read up to 256 bytes from file
        // process: process the bytes from the load stage
        // done: eof is reached, all bytes processed, end task
        enum class state
        {
            load,
            process,
            done
        };

        state state_{state::load};
        std::filesystem::path path_;
        char target_;
        std::shared_ptr<std::ifstream> stream_;
        std::vector<char> buffer_;
        std::uint64_t bytesRead_{0};
        std::uint64_t count_{0};
    };

}
