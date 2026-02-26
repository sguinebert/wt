#include "uploaded_file.hpp"

#ifndef _WIN32
#include <unistd.h>
#else
#include <io.h>
#define unlink _unlink
#endif

namespace Wt {
namespace http {

UploadedFile::UploadedFile() = default;

UploadedFile::UploadedFile(const std::string& spoolName,
                           const std::string& clientFileName,
                           const std::string& contentType)
{
    fileInfo_.reset(new Impl());
    fileInfo_->spoolFileName = spoolName;
    fileInfo_->clientFileName = clientFileName;
    fileInfo_->contentType = contentType;
    fileInfo_->isStolen = false;
}

void UploadedFile::Impl::cleanup()
{
    if (!isStolen)
        unlink(spoolFileName.c_str());
}

UploadedFile::Impl::~Impl()
{
    cleanup();
}

const std::string& UploadedFile::spoolFileName() const
{
    return fileInfo_->spoolFileName;
}

const std::string& UploadedFile::clientFileName() const
{
    return fileInfo_->clientFileName;
}

const std::string& UploadedFile::contentType() const
{
    return fileInfo_->contentType;
}

void UploadedFile::stealSpoolFile() const
{
    fileInfo_->isStolen = true;
}

} // namespace http
} // namespace Wt
