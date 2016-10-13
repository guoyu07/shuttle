#include "seq_file.h"

#include "logging.h"

namespace baidu {
namespace shuttle {

bool InfSeqFile::ReadRecord(std::string& key, std::string& value) {
    int key_len, value_len;
    void *raw_key = NULL, *raw_value = NULL;
    int ret = readNextRecordFromSeqFile(fs_, sf_, &raw_key, &key_len, &raw_value, &value_len);
    if (ret != 0 && ret != 1) {
        LOG(WARNING, "fail to read next record: %s", path_.c_str());
        status_ = kReadFileFail;
        return false;
    }
    if (ret == 1) {
        status_ = kNoMore;
        return false;
    }
    key.assign(static_cast<char*>(raw_key), key_len);
    value.assign(static_cast<char*>(raw_value), value_len);
    status_ = kOk;
    return true;
}

bool InfSeqFile::WriteRecord(const std::string& key, const std::string& value) {
    int ret = writeRecordIntoSeqFile(fs_, sf_, key.data(), key.size(),
            value.data(), value.size());
    if (ret != 0) {
        LOG(WARNING, "write next record fail: %s", path_.c_str());
        status_ = kWriteFileFail;
        return false;
    }
    status_ = kOk;
    return true;
}

bool InfSeqFile::Seek(int64_t offset) {
    int64_t ret = syncSeqFile(sf_, offset);
    if (ret < 0) {
        LOG(WARNING, "seek to %ld fail: %s", offset, path_.c_str());
        status_ = kReadFileFail;
        return false;
    }
    status_ = kOk;
    return true;
}

int64_t InfSeqFile::Tell() {
    return getSeqFilePos(sf_);
}

bool InfSeqFile::Open(const std::string& path, OpenMode mode, const File::Param& /*param*/) {
    path_ = path;
    if (mode == kReadFile) {
        sf_ = readSequenceFile(fs_, path.c_str());
        if (!sf_) {
            LOG(WARNING, "fail to read: %s", path.c_str());
            status_ = kOpenFileFail;
            return false;
        }
    } else if (mode == kWriteFile) {
        sf_ = writeSequenceFile(fs_, path.c_str(),
                "BLOCK", "org.apache.hadoop.io.compress.LzoCodec");
        if (!sf_) {
            LOG(WARNING, "fail to write: %s", path.c_str());
            status_ = kOpenFileFail;
            return false;
        }
    } else {
        LOG(FATAL, "unknown mode: %d", mode);
    }
    status_ = kOk;
    return true;
}

bool InfSeqFile::Close() {
    bool ok = (closeSequenceFile(fs_, sf_) == 0);
    status_ = ok ? kOk : kCloseFileFail;
    return ok;
}

int64_t InfSeqFile::GetSize() {
    hdfsFileInfo* info = NULL;
    info = hdfsGetPathInfo(fs_, path_.c_str());
    if (info == NULL) {
        LOG(WARNING, "failed to get info of %s", path_.c_str());
        return -1;
    }
    int64_t file_size = info->mSize;
    hdfsFreeFileInfo(info, 1);
    return file_size;
}

}
}
