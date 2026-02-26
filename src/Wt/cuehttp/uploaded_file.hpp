#ifndef CUEHTTP_UPLOADED_FILE_HPP_
#define CUEHTTP_UPLOADED_FILE_HPP_

#include <memory>
#include <string>

#include "WHttpDllDefs.h"

namespace Wt {
namespace http {

class WHTTP_API UploadedFile {
public:
    UploadedFile();

    UploadedFile(const std::string& spoolFileName,
                 const std::string& clientFileName,
                 const std::string& contentType);

    const std::string& spoolFileName() const;
    const std::string& clientFileName() const;
    const std::string& contentType() const;
    void stealSpoolFile() const;

private:
    struct Impl {
        std::string spoolFileName, clientFileName, contentType;
        bool isStolen;
        ~Impl();
        void cleanup();
    };

    std::shared_ptr<Impl> fileInfo_;
};

} // namespace http
} // namespace Wt

#endif // CUEHTTP_UPLOADED_FILE_HPP_
