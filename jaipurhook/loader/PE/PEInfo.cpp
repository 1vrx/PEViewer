/*
PEInfo.cpp
*/

#include "PEInfo.h"

namespace Info
{	

    fileInfo targetFile;

   
    
	//returns uniqueptr of ifstream
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

    //returns fileSize too
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

    bool loaddll(std::unique_ptr<std::ifstream>& dll)  //could std::ifstream &dll and when i pass the value i deref.
    {
        targetFile.data.resize(targetFile.fileSize);  

        dll->seekg(0, std::ios::beg);
        dll->read(reinterpret_cast<char*>(targetFile.data.data()), targetFile.fileSize);
        dll->close();


        return TRUE;
    }

    //used ChatGPT to make the maths in this function cleaner
    size_t RvaToOffset(DWORD rva)
    {
        if (!targetFile.pFileHeader || targetFile.pSectionHeaders.empty())
            return 0;

        for (const auto& section : targetFile.pSectionHeaders)
        {
            if (rva >= section.VirtualAddress && rva < (section.VirtualAddress + section.Misc.VirtualSize))
            {
                return (rva - section.VirtualAddress) + section.PointerToRawData;
            }
        }
        return 0; 
    }
    //used ChatGPT to make the maths in this function cleaner
    void ParseDataDirectories()
    {
        if (targetFile.is64Bit && targetFile.pOptHeader64)
        {
            targetFile.pDataDirectories = targetFile.pOptHeader64->DataDirectory;
        }
        else if (!targetFile.is64Bit && targetFile.pOptHeader32)
        {
            targetFile.pDataDirectories = targetFile.pOptHeader32->DataDirectory;
        }
    }

    void ParseImports()
    {
        targetFile.imports.clear();
        if (!targetFile.pDataDirectories) return;

        auto& importDir = targetFile.pDataDirectories[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (importDir.Size == 0) return;

        size_t offset = RvaToOffset(importDir.VirtualAddress);
        if (offset == 0) return;

        IMAGE_IMPORT_DESCRIPTOR* desc = (IMAGE_IMPORT_DESCRIPTOR*)(targetFile.data.data() + offset);

        while (desc->Name != 0)
        {
            ImportedDLL dll;
            size_t dllNameOffset = RvaToOffset(desc->Name);
            if (dllNameOffset != 0)
                dll.name = (char*)(targetFile.data.data() + dllNameOffset);
            else
                dll.name = "[Unknown DLL]";

            DWORD thunkRVA = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
            size_t thunkOffset = RvaToOffset(thunkRVA);

            if (targetFile.is64Bit)
            {
                IMAGE_THUNK_DATA64* thunk = (IMAGE_THUNK_DATA64*)(targetFile.data.data() + thunkOffset);
                while (thunk->u1.AddressOfData != 0)
                {
                    ImportedFunction func;
                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG64)
                    {
                        func.name = "Ordinal #" + std::to_string(thunk->u1.Ordinal & 0xFFFF); //0xFFFF
                    }
                    else
                    {
                        size_t byNameOffset = RvaToOffset((DWORD)thunk->u1.AddressOfData);
                        if (byNameOffset != 0)
                        {
                            IMAGE_IMPORT_BY_NAME* byName = (IMAGE_IMPORT_BY_NAME*)(targetFile.data.data() + byNameOffset);
                            func.name = (char*)byName->Name;
                        }
                        else
                        {
                            func.name = "[Unknown Import]";
                        }
                    }
                    func.rva = thunkRVA;
                    func.fileOffset = thunkOffset;
                    dll.functions.push_back(func);
                    thunk++;
                    thunkRVA += sizeof(IMAGE_THUNK_DATA64);
                    thunkOffset += sizeof(IMAGE_THUNK_DATA64);
                }
            }
            else // 32bit ver
            {
                IMAGE_THUNK_DATA32* thunk = (IMAGE_THUNK_DATA32*)(targetFile.data.data() + thunkOffset);
                while (thunk->u1.AddressOfData != 0)
                {
                    ImportedFunction func;
                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
                    {
                        func.name = "Ordinal #" + std::to_string(thunk->u1.Ordinal & 0xFFFF);
                    }
                    else
                    {
                        size_t byNameOffset = RvaToOffset(thunk->u1.AddressOfData);
                        if (byNameOffset != 0)
                        {
                            IMAGE_IMPORT_BY_NAME* byName = (IMAGE_IMPORT_BY_NAME*)(targetFile.data.data() + byNameOffset);
                            func.name = (char*)byName->Name;
                        }
                        else
                        {
                            func.name = "[Unknown Import]";
                        }
                    }
                    func.rva = thunkRVA;
                    func.fileOffset = thunkOffset;
                    dll.functions.push_back(func);
                    thunk++;
                    thunkRVA += sizeof(IMAGE_THUNK_DATA32);
                    thunkOffset += sizeof(IMAGE_THUNK_DATA32);
                }
            }

            if (!dll.functions.empty())
                targetFile.imports.push_back(dll);
            desc++;
        }
    }

    void ParseExports()
    {
        targetFile.exports.clear();
        if (!targetFile.pDataDirectories) return;

        auto& exportDirData = targetFile.pDataDirectories[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (exportDirData.Size == 0) return;

        size_t offset = RvaToOffset(exportDirData.VirtualAddress);
        if (offset == 0) return;

        IMAGE_EXPORT_DIRECTORY* dir = (IMAGE_EXPORT_DIRECTORY*)(targetFile.data.data() + offset);

        DWORD* nameRVAs = (DWORD*)(targetFile.data.data() + RvaToOffset(dir->AddressOfNames));
        DWORD* funcRVAs = (DWORD*)(targetFile.data.data() + RvaToOffset(dir->AddressOfFunctions));
        WORD* ordinals = (WORD*)(targetFile.data.data() + RvaToOffset(dir->AddressOfNameOrdinals));

        size_t funcTableOffset = RvaToOffset(dir->AddressOfFunctions);

        for (DWORD i = 0; i < dir->NumberOfNames; ++i)
        {
            ExportedFunction func;

            size_t nameOffset = RvaToOffset(nameRVAs[i]);
            if (nameOffset != 0)
                func.name = (char*)(targetFile.data.data() + nameOffset);
            else
                func.name = "[Unknown Export]";

            func.ordinal = ordinals[i];
            func.rva = funcRVAs[func.ordinal];
            
            //store in func table
            func.fileOffset = funcTableOffset + (func.ordinal * sizeof(DWORD));

            if (func.rva != 0)
                targetFile.exports.push_back(func);
        }
    }

    void getHeaders()
    {
        //reset on every call, otherwise loading a new file will mess it up
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

        targetFile.pDosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(targetFile.data.data());

        if (targetFile.pDosHeader->e_magic != 0x5A4D) 
        {
            std::cout << "\nThis file format is not supported (failed to locate DOS header)";
            targetFile.pDosHeader = nullptr; 
            return;
        }

        if (targetFile.pDosHeader->e_lfanew > 0 &&
            (targetFile.pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS)) < targetFile.fileSize)
        {
            targetFile.pNtHeader = reinterpret_cast<IMAGE_NT_HEADERS*>(targetFile.data.data() + targetFile.pDosHeader->e_lfanew);
        }
        else
        {
            std::cout << "\nFailed to locate NT header (invalid e_lfanew)";
            return;
        }

        if (targetFile.pNtHeader->Signature != IMAGE_NT_SIGNATURE)
        {
            std::cout << "\nInvalid PE signature";
            targetFile.pNtHeader = nullptr;
            return;
        }


        targetFile.pFileHeader = &targetFile.pNtHeader->FileHeader;


        WORD magic = targetFile.pNtHeader->OptionalHeader.Magic;

        if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        {
            targetFile.is64Bit = false;
            targetFile.pOptHeader32 = (IMAGE_OPTIONAL_HEADER32*)&targetFile.pNtHeader->OptionalHeader;
        }
        else if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        {
            targetFile.is64Bit = true;
            targetFile.pOptHeader64 = (IMAGE_OPTIONAL_HEADER64*)&targetFile.pNtHeader->OptionalHeader;
        }
        else
        {
            std::cout << "\nInvalid Optional Header Magic number";
            targetFile.pNtHeader = nullptr;
            targetFile.pFileHeader = nullptr;
            return;
        }

        getSectionHeaders();
        targetFile.arch = getArchitecture(); 

        ParseDataDirectories();
        ParseImports();
        ParseExports();

        return;

    }

    void getSectionHeaders()
    {
        if (!targetFile.pNtHeader) return;


        IMAGE_SECTION_HEADER* pCurrentSection = IMAGE_FIRST_SECTION(targetFile.pNtHeader);

        for (int i = 0; i < targetFile.pFileHeader->NumberOfSections; ++i)
        {
            if (reinterpret_cast<uintptr_t>(pCurrentSection) + sizeof(IMAGE_SECTION_HEADER) >
                reinterpret_cast<uintptr_t>(targetFile.data.data()) + targetFile.fileSize)
            {
                std::cout << "\nError: Section header " << i << " is out of file bounds.";
                break;
            }
            targetFile.pSectionHeaders.push_back(*pCurrentSection);
            pCurrentSection++;
        }
    }

    
    Architecture getArchitecture()
    {
        switch (targetFile.pFileHeader->Machine)
        {
        case IMAGE_FILE_MACHINE_I386:  return Architecture::X86;
        case IMAGE_FILE_MACHINE_AMD64: return Architecture::X64;
        case IMAGE_FILE_MACHINE_ARM:   return Architecture::ARM;
        case IMAGE_FILE_MACHINE_ARM64: return Architecture::ARM64;
        default:                       return Architecture::Unknown;
        }
    }
   
	
}

const char* fileInfo::getArchString() const
{

    switch (this->arch)
    {
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

    if (subsystem == IMAGE_SUBSYSTEM_NATIVE)
        return "System Driver (.sys)";

    if (this->pFileHeader->Characteristics & IMAGE_FILE_DLL)
        return "Dynamic Link Library (.dll)";

    if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI)
        return "Windows GUI App (.exe)";

    if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
        return "Windows Console App (.exe)";
    
    if (subsystem == IMAGE_SUBSYSTEM_XBOX)
        return "Xbox App (.xbx)";

    if (subsystem == IMAGE_SUBSYSTEM_EFI_APPLICATION)
        return "EFI Application (.efi)";

    if (subsystem == IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER)
        return "EFI Boot Driver (.efi)";

    if (subsystem == IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER)
        return "EFI Runtime Driver (.efi)";

    if (subsystem == IMAGE_SUBSYSTEM_EFI_ROM)
        return "EFI Rom (.efi)";

    if (this->pFileHeader->Characteristics & IMAGE_FILE_EXECUTABLE_IMAGE)
        return "Executable (Unknown Subsystem)";

    return "Unknown PE File";
}