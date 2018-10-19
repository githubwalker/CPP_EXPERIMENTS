#pragma once
#include <string>

namespace sorter
{
    using ITEM_TYPE = std::size_t;
    void SortLargeFile(const std::string& input_filename, const std::string& output_filename);
}
