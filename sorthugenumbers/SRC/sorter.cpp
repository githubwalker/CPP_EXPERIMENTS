#include "./sorter.h"
#include "thread_pool.h"

#include <fstream>
#include <utility>
#include <vector>
#include <queue>
#include <boost/lockfree/queue.hpp>
#include <boost/filesystem.hpp>

#include "sysutils.h"


namespace sorter
{
    std::vector<ITEM_TYPE> ReadFromFile(std::ifstream& stm, std::size_t nitems2read)
    {
        std::vector<ITEM_TYPE> buffer;
        buffer.reserve(nitems2read);
        constexpr std::size_t single_read_items_count = 1024;
        std::size_t nbytes_total = 0;

        do
        {
            auto bytes2read = std::min(single_read_items_count * sizeof(ITEM_TYPE),  nitems2read * sizeof(ITEM_TYPE) - nbytes_total);
            auto items2read = (bytes2read + sizeof(ITEM_TYPE) - 1) / sizeof(ITEM_TYPE);

            buffer.resize(buffer.size() + items2read);
            stm.read((char *)&buffer[buffer.size() - items2read], bytes2read);
            nbytes_total += stm.gcount();
        }
        while(stm && nbytes_total < nitems2read * sizeof(ITEM_TYPE));

        auto nitems_got_total = (nbytes_total + sizeof(ITEM_TYPE) - 1) / sizeof(ITEM_TYPE);
        buffer.resize(nitems_got_total);

        return std::move(buffer);
    }

    std::ifstream::pos_type FileSize(const std::string& filename)
    {
        std::ifstream in(filename, std::ifstream::ate | std::ifstream::binary);
        return in.tellg();
    }

    struct EnsureFilesRemoved
    {
        const std::vector<std::string>& fnames_;

        EnsureFilesRemoved(const std::vector<std::string>& fnames)
            : fnames_(fnames)
        {}

        ~EnsureFilesRemoved()
        {
            for(const auto& fname : fnames_ )
            {
                try
                {
                    boost::filesystem::remove(fname);
                }
                catch(...)
                {}
            }
        }
    };

    class TempFileReader
    {
        std::string fname_;
        std::shared_ptr<std::ifstream> pstm_;
        boost::optional<ITEM_TYPE> current_;
    public:
        TempFileReader(const std::string& fname)
            : fname_(fname), pstm_(std::make_shared<std::ifstream>())
        {
            pstm_->open(fname, std::ios::in | std::ios::binary);
            if (!pstm_->is_open())
                throw std::runtime_error(std::string("failed to open temp file ") + fname_);
        }

        bool ReadNext()
        {
            ITEM_TYPE itm;
            pstm_->read((char *)&itm, sizeof(itm));

            if (pstm_->gcount() != sizeof(itm) && pstm_->gcount() != 0)
                throw std::runtime_error( "failed to read from file " + fname_);
            bool bok = pstm_->gcount() == sizeof(itm);

            if (bok)
                current_ = itm;
            else
                current_ = boost::none;
            return bok;
        }

        bool HasItem() const
        {
            return current_.is_initialized();
        }

        ITEM_TYPE GetItem() const
        {
            return *current_;
        }

        const ITEM_TYPE& GetItemRef() const
        {
            return *current_;
        }

    };

    void OutputToTemp(boost::lockfree::queue<std::vector<ITEM_TYPE>*>& output_queue, const boost::filesystem::path& tmpdir, std::vector<std::string>& tmpfilenames)
    {
        std::vector<ITEM_TYPE> * poutitem = nullptr;
        while(output_queue.pop(poutitem))
        {
            auto fetched_item = std::unique_ptr<std::vector<ITEM_TYPE>>(poutitem);
            auto temp_filename = tmpdir / boost::filesystem::unique_path();
            std::ofstream temp;
            temp.open(temp_filename.native(), std::ios::out | std::ios::binary);
            tmpfilenames.push_back(temp_filename.native());

            if (!temp.is_open())
                throw std::runtime_error(std::string("failed to open temp file ") + temp_filename.native());
            temp.write((char *)&(*fetched_item.get())[0], fetched_item.get()->size() * sizeof(ITEM_TYPE));
        }
    }

    void SortLargeFile(const std::string& input_filename, const std::string& output_filename)
    {
        const int64_t max_bytes_per_chunck = 128 * 1024 * 1024;

        std::ifstream in(input_filename, std::ifstream::in | std::ifstream::binary);
        std::ofstream out(output_filename, std::ifstream::out | std::ifstream::binary);

        if (!in.is_open())
            throw std::runtime_error("failed to open " + input_filename);

        if (!out.is_open())
            throw std::runtime_error("failed to open " + output_filename);

        auto file_size = FileSize(input_filename);

        // items total by file size
        auto nitems = (static_cast<std::size_t>(file_size)+sizeof(ITEM_TYPE)-1) / sizeof(ITEM_TYPE);
        // threads total to run
        auto nthreads = boost::thread::hardware_concurrency();
        // items per thread
        auto nitems4thread = static_cast<int64_t>((nitems + nthreads - 1) / nthreads);
        // max items in chunk by free mem (divided by 2)
        auto items_in_chunk_max_by_free_mem = static_cast<int64_t>((sysutils::GetMemAvailable() * 3 / 4) / (nthreads * sizeof(ITEM_TYPE)));
        //
        auto max_items_per_chunk = static_cast<int64_t>((max_bytes_per_chunck + sizeof(ITEM_TYPE) - 1) / sizeof(ITEM_TYPE));

        auto nitems_in_chunk = std::min(nitems4thread, max_items_per_chunk);
        nitems_in_chunk = std::min(nitems_in_chunk, items_in_chunk_max_by_free_mem);

        std::vector<std::string> tmpfilenames;
        EnsureFilesRemoved tmpfilesremover(tmpfilenames);

        const auto tmpdir = boost::filesystem::temp_directory_path() / "bigsort";

        if (!boost::filesystem::is_directory(tmpdir))
        {
            boost::system::error_code returnedError;
            boost::filesystem::create_directories(tmpdir, returnedError);
            if (returnedError)
                throw std::runtime_error( std::string("failed to create directory ") + tmpdir.native() );
        }

        boost::lockfree::queue<std::vector<ITEM_TYPE>*> output_queue(nthreads * 2);

        // sort chunks
        {
            thread_pool::thread_pool pool(nthreads);
            std::vector<ITEM_TYPE> current_buffer;

            while(!(current_buffer = ReadFromFile(in, nitems_in_chunk)).empty())
            {
                auto pvector = new std::vector<ITEM_TYPE>(std::move(current_buffer));
                pool.run_task( [pvector, &output_queue]() {
                    std::sort( pvector->begin(), pvector->end());
                    while (!output_queue.push(pvector));
                });

                OutputToTemp(output_queue, tmpdir, tmpfilenames);
            }
        }

        OutputToTemp(output_queue, tmpdir, tmpfilenames);

        // merge chunks
        {
            auto comparator = [](const TempFileReader& a, const TempFileReader& b) { return a.GetItem() > b.GetItem(); };

            std::vector<TempFileReader> readers;
            for(auto tmpfname : tmpfilenames)
            {
                TempFileReader reader(tmpfname);
                reader.ReadNext();
                if (reader.HasItem())
                    readers.push_back(reader);
            }

            while(!readers.empty())
            {
                std::pop_heap(readers.begin(), readers.end(), comparator);
                out.write((char *)&readers.back().GetItemRef(), sizeof(ITEM_TYPE));
                if (!readers.back().ReadNext())
                    readers.pop_back();
                else
                    std::push_heap(readers.begin(), readers.end(), comparator);
            }
        }
    }
}
