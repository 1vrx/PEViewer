#include "PEInfo.h"

namespace Info
{
    fileInfo targetFile;

    std::unique_ptr<std::ifstream> getFile(const char* theDllPath)
    {
        if (GetFileAttributesA(theDllPath) == INVALID_FILE_ATTRIBUTES)
        {
            std::cout << "\nNo file found\n";
            return nullptr;
        }

        auto file = std::make_unique<std::ifstream>(theDllPath, std::ios::binary | std::ios::ate);

        if (!file->is_open())
        {
            std::cout << "\nFailed to open file\n";
            return nullptr;
        }

        return file;
    }

    std::pair<bool, uintptr_t> checkFileSize(std::unique_ptr<std::ifstream>& dll)
    {
        auto filesize = dll->tellg();
        if (filesize < 0x1000)
        {
            std::cout << "\ninvalid file size  || size < (0x1000)";
            return { FALSE, 0 };
        }

        std::cout << "\ndll check complete";
        return { TRUE, static_cast<uintptr_t>(filesize) };
    }

    bool loaddll(std::unique_ptr<std::ifstream>& dll)
    {
        targetFile.data.resize(targetFile.fileSize);
        dll->seekg(0, std::ios::beg);
        dll->read(reinterpret_cast<char*>(targetFile.data.data()), targetFile.fileSize);
        dll->close();
        return TRUE;
    }

    void getHeaders()
    {
        targetFile.pDosHeader = nullptr;
        targetFile.pNtHeader = nullptr;
        targetFile.pOptHeader32 = nullptr;
        targetFile.pOptHeader64 = nullptr;
        targetFile.pDataDirectories = nullptr;
        targetFile.pFileHeader = nullptr;
        targetFile.pSectionHeaders.clear();
        targetFile.imports.clear();
        targetFile.exports.clear();
        targetFile.arch = Architecture::Unknown;
        targetFile.is64Bit = false;

        if (targetFile.data.empty()) return;

        PEParser parser(targetFile.data.data(), targetFile.fileSize);

        if (parser.Parse() != PEError::Success) {
            std::cout << "\nFailed to parse PE file structure safely.\n";
            return;
        }

        targetFile.pDosHeader = parser.GetDosHeader();
        targetFile.pNtHeader = parser.GetNtHeaders();
        targetFile.pFileHeader = parser.GetFileHeader();
        targetFile.pOptHeader32 = parser.GetOptHeader32();
        targetFile.pOptHeader64 = parser.GetOptHeader64();
        targetFile.pDataDirectories = parser.GetDataDirectories();
        targetFile.pSectionHeaders = parser.GetSections();
        targetFile.is64Bit = parser.Is64Bit();
        targetFile.arch = parser.GetArchitecture();

        auto exports_res = parser.GetExports();
        if (exports_res.is_valid()) targetFile.exports = exports_res.value;

        auto imports_res = parser.GetImports();
        if (imports_res.is_valid()) targetFile.imports = imports_res.value;
    }
}

PEParser::PEParser(const uint8_t* data, size_t size)
    : buffer_(data), size_(size) {
}

std::string PEParser::SafeReadString(size_t offset) const {
    if (offset >= size_) return "";
    size_t max_len = size_ - offset;
    const char* str = reinterpret_cast<const char*>(buffer_ + offset);
    return std::string(str, strnlen(str, max_len));
}

PEError PEParser::Parse() {
    if (size_ < sizeof(IMAGE_DOS_HEADER)) return PEError::FileTooSmall;

    dos_header_ = ReadAt<IMAGE_DOS_HEADER>(0);
    if (!dos_header_ || dos_header_->e_magic != IMAGE_DOS_SIGNATURE) {
        return PEError::InvalidDosHeader;
    }

    size_t nt_offset = dos_header_->e_lfanew;
    nt_headers_ = ReadAt<IMAGE_NT_HEADERS>(nt_offset);
    if (!nt_headers_ || nt_headers_->Signature != IMAGE_NT_SIGNATURE) {
        return PEError::InvalidNtHeader;
    }

    file_header_ = &nt_headers_->FileHeader;

    uint16_t magic = nt_headers_->OptionalHeader.Magic;
    if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        is_64bit_ = false;
        opt_header_32_ = reinterpret_cast<IMAGE_OPTIONAL_HEADER32*>(&nt_headers_->OptionalHeader);
        data_dirs_ = opt_header_32_->DataDirectory;
    }
    else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        is_64bit_ = true;
        opt_header_64_ = reinterpret_cast<IMAGE_OPTIONAL_HEADER64*>(&nt_headers_->OptionalHeader);
        data_dirs_ = opt_header_64_->DataDirectory;
    }
    else {
        return PEError::InvalidOptionalHeader;
    }

    size_t section_offset = nt_offset + offsetof(IMAGE_NT_HEADERS, OptionalHeader) + file_header_->SizeOfOptionalHeader;

    if (section_offset + (file_header_->NumberOfSections * sizeof(IMAGE_SECTION_HEADER)) > size_) {
        return PEError::SectionTableOutOfBounds;
    }

    sections_.reserve(file_header_->NumberOfSections);
    for (int i = 0; i < file_header_->NumberOfSections; ++i) {
        if (auto* sec = ReadAt<IMAGE_SECTION_HEADER>(section_offset + (i * sizeof(IMAGE_SECTION_HEADER)))) {
            sections_.push_back(*sec);
        }
    }

    return PEError::Success;
}

Architecture PEParser::GetArchitecture() const {
    if (!file_header_) return Architecture::Unknown;
    switch (file_header_->Machine) {
    case IMAGE_FILE_MACHINE_I386:  return Architecture::X86;
    case IMAGE_FILE_MACHINE_AMD64: return Architecture::X64;
    case IMAGE_FILE_MACHINE_ARM:   return Architecture::ARM;
    case IMAGE_FILE_MACHINE_ARM64: return Architecture::ARM64;
    default:                       return Architecture::Unknown;
    }
}

Result<uint32_t> PEParser::RvaToOffset(uint32_t rva) const {
    for (const auto& section : sections_) {
        uint32_t virtual_size = section.Misc.VirtualSize ? section.Misc.VirtualSize : section.SizeOfRawData;
        if (rva >= section.VirtualAddress && rva < section.VirtualAddress + virtual_size) {
            uint32_t offset = section.PointerToRawData + (rva - section.VirtualAddress);
            if (offset >= size_) return { 0, PEError::RvaOutOfBounds };
            return { offset, PEError::Success };
        }
    }
    return { 0, PEError::RvaOutOfBounds };
}

Result<std::vector<ExportedFunction>> PEParser::GetExports() const {
    if (!data_dirs_) return { {}, PEError::InvalidNtHeader };

    auto& export_dir_data = data_dirs_[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (export_dir_data.Size == 0) return { {}, PEError::Success };

    auto offset_res = RvaToOffset(export_dir_data.VirtualAddress);
    if (!offset_res.is_valid()) return { {}, offset_res.error };

    auto* dir = ReadAt<IMAGE_EXPORT_DIRECTORY>(offset_res.value);
    if (!dir) return { {}, PEError::DirectoryInvalid };

    auto name_rvas_res = RvaToOffset(dir->AddressOfNames);
    auto func_rvas_res = RvaToOffset(dir->AddressOfFunctions);
    auto ordinals_res = RvaToOffset(dir->AddressOfNameOrdinals);

    if (!name_rvas_res.is_valid() || !func_rvas_res.is_valid() || !ordinals_res.is_valid()) {
        return { {}, PEError::DirectoryInvalid };
    }

    uint32_t* name_rvas = ReadAt<uint32_t>(name_rvas_res.value);
    uint32_t* func_rvas = ReadAt<uint32_t>(func_rvas_res.value);
    uint16_t* ordinals = ReadAt<uint16_t>(ordinals_res.value);

    if (!name_rvas || !func_rvas || !ordinals) return { {}, PEError::DirectoryInvalid };

    std::vector<ExportedFunction> exports;
    size_t func_table_offset = func_rvas_res.value;

    for (DWORD i = 0; i < dir->NumberOfNames; ++i) {
        if ((i * sizeof(uint32_t)) + name_rvas_res.value >= size_) break;
        if ((i * sizeof(uint16_t)) + ordinals_res.value >= size_) break;

        ExportedFunction func;
        func.ordinal = ordinals[i];

        if ((func.ordinal * sizeof(uint32_t)) + func_table_offset >= size_) continue;

        func.rva = func_rvas[func.ordinal];
        func.fileOffset = func_table_offset + (func.ordinal * sizeof(DWORD));

        auto name_off = RvaToOffset(name_rvas[i]);
        if (name_off.is_valid()) {
            func.name = SafeReadString(name_off.value);
        }
        else {
            func.name = "[Unknown Export]";
        }

        if (func.rva != 0) exports.push_back(func);
    }

    return { exports, PEError::Success };
}

Result<std::vector<ImportedDLL>> PEParser::GetImports() const {
    if (!data_dirs_) return { {}, PEError::InvalidNtHeader };

    auto& import_dir = data_dirs_[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir.Size == 0) return { {}, PEError::Success };

    auto offset_res = RvaToOffset(import_dir.VirtualAddress);
    if (!offset_res.is_valid()) return { {}, offset_res.error };

    std::vector<ImportedDLL> imports;
    size_t current_offset = offset_res.value;

    while (true) {
        auto* desc = ReadAt<IMAGE_IMPORT_DESCRIPTOR>(current_offset);
        if (!desc || desc->Name == 0) break;

        ImportedDLL dll;
        auto dll_name_off = RvaToOffset(desc->Name);
        if (dll_name_off.is_valid()) {
            dll.name = SafeReadString(dll_name_off.value);
        }
        else {
            dll.name = "[Unknown DLL]";
        }

        DWORD thunk_rva = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        auto thunk_offset = RvaToOffset(thunk_rva);

        if (thunk_offset.is_valid()) {
            size_t t_off = thunk_offset.value;
            DWORD curr_thunk_rva = thunk_rva;

            if (is_64bit_) {
                while (auto* thunk = ReadAt<IMAGE_THUNK_DATA64>(t_off)) {
                    if (thunk->u1.AddressOfData == 0) break;

                    ImportedFunction func;
                    func.rva = curr_thunk_rva;
                    func.fileOffset = t_off;

                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64) {
                        func.name = "Ordinal #" + std::to_string(thunk->u1.Ordinal & 0xFFFF);
                    }
                    else {
                        auto byname_off = RvaToOffset((DWORD)thunk->u1.AddressOfData);
                        if (byname_off.is_valid()) {
                            func.name = SafeReadString(byname_off.value + sizeof(WORD));
                        }
                        else {
                            func.name = "[Unknown Import]";
                        }
                    }
                    dll.functions.push_back(func);
                    t_off += sizeof(IMAGE_THUNK_DATA64);
                    curr_thunk_rva += sizeof(IMAGE_THUNK_DATA64);
                }
            }
            else {
                while (auto* thunk = ReadAt<IMAGE_THUNK_DATA32>(t_off)) {
                    if (thunk->u1.AddressOfData == 0) break;

                    ImportedFunction func;
                    func.rva = curr_thunk_rva;
                    func.fileOffset = t_off;

                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32) {
                        func.name = "Ordinal #" + std::to_string(thunk->u1.Ordinal & 0xFFFF);
                    }
                    else {
                        auto byname_off = RvaToOffset(thunk->u1.AddressOfData);
                        if (byname_off.is_valid()) {
                            func.name = SafeReadString(byname_off.value + sizeof(WORD));
                        }
                        else {
                            func.name = "[Unknown Import]";
                        }
                    }
                    dll.functions.push_back(func);
                    t_off += sizeof(IMAGE_THUNK_DATA32);
                    curr_thunk_rva += sizeof(IMAGE_THUNK_DATA32);
                }
            }
        }

        if (!dll.functions.empty()) imports.push_back(dll);
        current_offset += sizeof(IMAGE_IMPORT_DESCRIPTOR);
    }

    return { imports, PEError::Success };
}

const char* fileInfo::getArchString() const
{
    switch (this->arch) {
    case Architecture::X86: return "x86 (32-bit)";
    case Architecture::X64: return "x64 (64-bit)";
    case Architecture::ARM: return "ARM";
    case Architecture::ARM64: return "ARM64";
    default: return "Unknown";
    }
}

const char* fileInfo::getFileTypeString() const
{
    if (!this->pFileHeader || (!this->pOptHeader32 && !this->pOptHeader64)) return "N/A (PE not parsed)";
    WORD subsystem = this->is64Bit ? this->pOptHeader64->Subsystem : this->pOptHeader32->Subsystem;

    if (subsystem == IMAGE_SUBSYSTEM_NATIVE) return "System Driver (.sys)";
    if (this->pFileHeader->Characteristics & IMAGE_FILE_DLL) return "Dynamic Link Library (.dll)";
    if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI) return "Windows GUI App (.exe)";
    if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI) return "Windows Console App (.exe)";
    if (subsystem == IMAGE_SUBSYSTEM_XBOX) return "Xbox App (.xbx)";
    if (subsystem == IMAGE_SUBSYSTEM_EFI_APPLICATION) return "EFI Application (.efi)";
    if (subsystem == IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER) return "EFI Boot Driver (.efi)";
    if (subsystem == IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER) return "EFI Runtime Driver (.efi)";
    if (subsystem == IMAGE_SUBSYSTEM_EFI_ROM) return "EFI Rom (.efi)";
    if (this->pFileHeader->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE) return "Executable (Unknown Subsystem)";

    return "Unknown PE File";
}