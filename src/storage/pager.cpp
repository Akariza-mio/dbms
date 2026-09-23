#include "storage/pager.hpp"
#include <stdexcept>
#include <system_error>

namespace minidb{
    Pager::Pager(const std::filesystem::path& path)
        : path_(path){
            std::error_code error;
            const bool is_exist = std::filesystem::exists(path_,error);
            if(error){
                throw std::runtime_error("Failed to check file existence: " + error.message());
            }
            //creator is only used to create the file
            if(!is_exist){
                std::ofstream creator(path_,std::ios::binary);
                if(!creator.is_open()){
                    throw std::runtime_error("Failed to create file: " + path_.string());
                }
            }
            fs.open(path_,std::ios::out | std::ios::in | std::ios::binary);
            if(!fs.is_open()){
                throw std::runtime_error("Failed to open pager file: " + path_.string());
            }
            error.clear();
            const std::uintmax_t file_size = std::filesystem::file_size(path_,error);
            if(error){
                throw std::runtime_error("failed to get the size of file: " + path_.string());
            }
            //file size need to be a multiple of 4096
            if(file_size % PAGESIZE != 0){
                throw std::runtime_error("pager file size is not a multiple of page size");
            }
            page_cnt = static_cast<PageId>(file_size / PAGESIZE);
        }
    Pager::PageId Pager::get_page_cnt() const noexcept{
        return page_cnt;
    }
    Pager::PageId Pager::allocate_page(){
        PageId new_id = page_cnt;
        Page empty_page{};
        //move the file pointer to the end
        fs.seekp(0,std::ios::end);
        if(!fs){
            throw std::runtime_error("Failed to seek to end of file");
        }
        //write 4096 bytes of empty_page to the file with binary mode
        fs.write(reinterpret_cast<const char*>(empty_page.data()),static_cast<std::streamsize>(empty_page.size()));
        fs.flush();
        if(!fs){
            throw std::runtime_error("Failed to allocate new page");
        }
        ++page_cnt;
        return new_id;
    }
    Pager::Page Pager::read_page(PageId page_id){
        if(page_id >= page_cnt){
            throw std::out_of_range("Page ID is out of range");
        }
        //compute the offset of the target page
        const std::streamoff offset = static_cast<std::streamoff>(page_id * PAGESIZE);
        fs.seekg(offset,std::ios::beg);
        if(!fs){
            throw std::runtime_error("Failed to seek to offset of file: " + path_.string());
        }
        Page page{};
        fs.read(reinterpret_cast<char*>(page.data()),static_cast<std::streamsize>(page.size()));
        //check if actually read 4096 bytes
        if(!fs || fs.gcount() != static_cast<std::streamsize>(page.size())){
            throw std::runtime_error("Failed to read page from file: " + path_.string());
        }
        return page;
    }
    void Pager::write_page(PageId page_id,const Pager::Page& page){
        if(page_id >= page_cnt){
            throw std::out_of_range("Page ID is out of range");
        }
        //compute the offset of the target page
        const std::streamoff offset = static_cast<std::streamoff>(page_id * PAGESIZE);
        fs.seekp(offset,std::ios::beg);
        if(!fs){
            throw std::runtime_error("Failed to seek to offset of file: " + path_.string());
        }
        //write the page to the file
        fs.write(reinterpret_cast<const char*>(page.data()),static_cast<std::streamsize>(page.size()));
        fs.flush();
        if(!fs){
            throw std::runtime_error("Failed to write page to file: " + path_.string());
        }
    }
}