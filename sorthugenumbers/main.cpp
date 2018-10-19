#include <iostream>
#include <boost/program_options.hpp>
#include "SRC/sorter.h"
#include "SRC/thread_pool.h"
#include "SRC/sysutils.h"

// using namespace std;

namespace
{
    struct ParsedParams
    {
        std::string input_file_name;
        std::string output_file_name;

        bool IsValid() const
        {
            return !input_file_name.empty() && !output_file_name.empty();
        }
    };

    ParsedParams ParseArgs(int argc, char** argv)
    {
        namespace po = boost::program_options;
        po::options_description mainOptions("Main options");
        po::options_description helpOptions("Help options");

        std::string input_fname;
        std::string output_fname;

        mainOptions.add_options()
            ("input-file,i", po::value<std::string>(&input_fname)->required(), "input file name" )
            ("output-file,o", po::value<std::string>(&output_fname)->required(), "output file name" )
            ;

        helpOptions.add_options()
            ("help,h", "Show help");

        po::variables_map vm;

        try
        {
            po::store( po::parse_command_line(argc, argv, mainOptions ), vm );
            po::notify(vm);
        }
        catch ( std::exception& )
        {
            mainOptions.print( std::cerr );
            throw;
        }

        if (vm.count("help"))
        {
            mainOptions.print( std::cerr );
            return ParsedParams{};
        }

        return ParsedParams{input_fname, output_fname};
    }
}

int main(int argc, char** argv)
{
    try
    {
        auto parsed_args = ParseArgs(argc, argv);
        if (!parsed_args.IsValid())
            return 1;
        sorter::SortLargeFile(parsed_args.input_file_name, parsed_args.output_file_name);
    }
    catch (std::exception& ex)
    {
        std::cerr << "Error happened: " << ex.what() << std::endl;
        return 2;
    }

    return 0;
}
