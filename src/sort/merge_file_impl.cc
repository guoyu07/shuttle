#include "sort_file.h"
#include <logging.h>

using baidu::common::Log;
using baidu::common::FATAL;
using baidu::common::INFO;
using baidu::common::WARNING;

namespace baidu {
namespace shuttle {

Status MergeFileReader::Open(const std::vector<std::string>& files, 
                             SortFileReader::Param& param,
                             FileType file_type) {
    if (files.size() == 0) {
        return kInvalidArg;
    }
    Status status = kOk;
    if (file_type != kHdfsFile) {
        status = kNotImplement;
    } else {
        std::vector<std::string>::const_iterator it;
        for (it = files.begin(); it != files.end(); it++) {
            const std::string& file_name = *it;
            Status st;
            SortFileReader* reader = SortFileReader::Create(file_type, &st);
            if (st == kOk) {
                st = reader->Open(file_name, param);
            }
            if (st != kOk) {
                status = st;
                LOG(WARNING, "failed to open %s, status: %s", 
                    file_name.c_str(), Status_Name(st).c_str());
                break;
            }
            readers_.push_back(reader);
        }
    }
    return status;
}

Status MergeFileReader::Close() {
    std::vector<SortFileReader*>::iterator it;
    Status status = kOk;
    for (it = readers_.begin(); it != readers_.end(); it++) {
        Status st = (*it)->Close();
        if (st != kOk) {
            status = st;
            break;
        }
    }
    return status;
}

SortFileReader::Iterator* MergeFileReader::Scan(const std::string& start_key, const std::string& end_key) {
    std::vector<SortFileReader::Iterator*> iters;
    std::vector<SortFileReader*>::iterator it;
    for (it = readers_.begin(); it != readers_.end(); it++) {
        SortFileReader * const& reader = *it;
        iters.push_back(reader->Scan(start_key, end_key));        
    }
    return new MergeIterator(iters);
}

MergeFileReader::MergeIterator::MergeIterator(const std::vector<SortFileReader::Iterator*>& iters) {
    std::vector<SortFileReader::Iterator*>::const_iterator it;
    status_ = kOk;
    int offset = 0;
    for (it = iters.begin(); it != iters.end(); it++) {
        SortFileReader::Iterator * const& reader_it = *it;
        iters_.push_back(reader_it);
        if (!reader_it->Done()) {
            queue_.push(MergeItem(reader_it->Key(), reader_it->Value(), offset));
        }
        if (reader_it->Error() != kOk) {
            status_ = reader_it->Error();
        }
        offset++;
    }
    if (status_ == kOk && !queue_.empty()) {
        key_ = queue_.top().key_;
        value_ = queue_.top().value_;
    }
}

MergeFileReader::MergeIterator::~MergeIterator() {
    std::vector<SortFileReader::Iterator*>::iterator it;
    for (it = iters_.begin(); it != iters_.end(); it++) {
        SortFileReader::Iterator * const& reader_it = *it;
        delete reader_it;
    }
}

bool MergeFileReader::MergeIterator::Done() {
    return queue_.empty();
}

void MergeFileReader::MergeIterator::Next() {
    if (queue_.empty()) {
        return;
    }
    const MergeItem& top = queue_.top();
    int offset = top.it_offset_;
    SortFileReader::Iterator* reader_it = iters_[offset];
    reader_it->Next();
    queue_.pop();
    if (!reader_it->Done()) {
        queue_.push(MergeItem(reader_it->Key(), reader_it->Value(), offset));
    }
    if (reader_it->Error() != kOk && reader_it->Error() != kNoMore) {
        status_ = reader_it->Error();
    } 
    if (!queue_.empty()) {
        key_ = queue_.top().key_;
        value_ = queue_.top().value_;
    }
}

}
}

