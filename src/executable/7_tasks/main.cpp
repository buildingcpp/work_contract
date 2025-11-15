#include "./example_1.h"
#include "./example_2.h"


int main
(
    int argc,
    char ** argv
) 
{
    if (argc != 4)
    {
        std::cout << "usage: ./7_tasks [path/to/test/directory/] [char_to_count] [number_of_threads]\n";
        std::cout << "example: (count how many times the letter 'a' appears in each file of the directory ~/my_test_files/ using 3 worker threads)\n";
        std::cout << "\t./7_tasks ~/my_test_files/ a 3\n";
        return -1;
    }
    example_1::run(argv[1], argv[2][0], std::atoi(argv[3]));
    example_2::run(argv[1], argv[2][0], std::atoi(argv[3]));

    return 0;
}