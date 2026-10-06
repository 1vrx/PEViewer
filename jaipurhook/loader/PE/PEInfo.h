#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <fstream>
#include <vector>
#include <memory>
#include "../../imgui/imgui.h"
#include "../globals.h"

struct ImportedFunction {
    std::string     name;
    DWORD           rva;
    size_t          fileOffset;
};

struct ImportedDLL {
    std::string                     name;
    std::vector<ImportedFunction>   functions;
};

struct ExportedFunction {
    std::string     name;
    DWORD           rva;
    WORD            ordinal;
    size_t          fileOffset;
};

enum class Architecture {
    X86,
    X64,
    ARM,
    ARM64,
    Unknown
};

enum class PEError {
    Success,
    FileTooSmall,
    InvalidDosHeader,
    InvalidNtHeader,
    InvalidOptionalHeader,
    SectionTableOutOfBounds,
    RvaOutOfBounds,
    DirectoryInvalid
};

template <typename T>
struct Result {
    T value;
    PEError error;
    bool is_valid() const { return error == PEError::Success; }
};

class PEParser {
public:
    PEParser(const uint8_t* data, size_t size);

    PEError Parse();
    Result<uint32_t> RvaToOffset(uint32_t rva) const;

    Result<std::vector<ExportedFunction>> GetExports() const;
    Result<std::vector<ImportedDLL>> GetImports() const;

    IMAGE_DOS_HEADER* GetDosHeader() const { return dos_header_; }
    IMAGE_NT_HEADERS* GetNtHeaders() const { return nt_headers_; }
    IMAGE_FILE_HEADER* GetFileHeader() const { return file_header_; }
    IMAGE_OPTIONAL_HEADER32* GetOptHeader32() const { return opt_header_32_; }
    IMAGE_OPTIONAL_HEADER64* GetOptHeader64() const { return opt_header_64_; }
    IMAGE_DATA_DIRECTORY* GetDataDirectories() const { return data_dirs_; }
    std::vector<IMAGE_SECTION_HEADER> GetSections() const { return sections_; }

    bool Is64Bit() const { return is_64bit_; }
    Architecture GetArchitecture() const;

private:
    const uint8_t* buffer_;
    size_t size_;

    IMAGE_DOS_HEADER* dos_header_ = nullptr;
    IMAGE_NT_HEADERS* nt_headers_ = nullptr;
    IMAGE_FILE_HEADER* file_header_ = nullptr;
    IMAGE_OPTIONAL_HEADER32* opt_header_32_ = nullptr;
    IMAGE_OPTIONAL_HEADER64* opt_header_64_ = nullptr;
    IMAGE_DATA_DIRECTORY* data_dirs_ = nullptr;
    std::vector<IMAGE_SECTION_HEADER> sections_;

    bool is_64bit_ = false;

    template <typename T>
    T* ReadAt(size_t offset) const {
        if (offset + sizeof(T) > size_) return nullptr;
        return reinterpret_cast<T*>(const_cast<uint8_t*>(buffer_ + offset));
    }

    std::string SafeReadString(size_t offset) const;
};

class fileInfo
{
public:
    std::vector<BYTE>                   data;
    IMAGE_DOS_HEADER* pDosHeader = nullptr;
    IMAGE_NT_HEADERS* pNtHeader = nullptr;
    IMAGE_OPTIONAL_HEADER32* pOptHeader32 = nullptr;
    IMAGE_OPTIONAL_HEADER64* pOptHeader64 = nullptr;
    IMAGE_DATA_DIRECTORY* pDataDirectories = nullptr;
    IMAGE_FILE_HEADER* pFileHeader = nullptr;
    std::vector<IMAGE_SECTION_HEADER>   pSectionHeaders;

    size_t                              fileSize = 0;
    std::string                         fileName;
    bool                                is64Bit = false;

    std::vector<ImportedDLL>            imports;
    std::vector<ExportedFunction>       exports;
    Architecture                        arch = Architecture::Unknown;

    const char* getArchString() const;
    const char* getFileTypeString() const;
};

namespace Info
{
    extern                                  fileInfo targetFile;
    std::unique_ptr<std::ifstream>  getFile(const char* theDllPath);
    std::pair<bool, uintptr_t>      checkFileSize(std::unique_ptr<std::ifstream>& dll);
    bool                            loaddll(std::unique_ptr<std::ifstream>& dll);
    void                            getHeaders(); // Now drives PEParser
}