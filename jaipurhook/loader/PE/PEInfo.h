/*
PEInfo.h
*/

#pragma once
#include <Windows.h>
#include <iostream>
#include <string>
#include <thread>
#include <tlhelp32.h>
#include <fstream>
#include <vector>

#include "../../imgui/imgui.h"
#include "../globals.h"


struct ImportedFunction {
	std::string		name;
	DWORD			rva; 
	size_t			fileOffset; 
};

struct ImportedDLL {
	std::string						name;
	std::vector<ImportedFunction>	functions;
};

struct ExportedFunction {
	std::string		name;
	DWORD			rva;
	WORD			ordinal;
	size_t			fileOffset; 
};


enum class Architecture {
	X86,
	X64,
	ARM,
	ARM64,
	Unknown
};


class fileInfo
{
public:
	std::vector<BYTE>					data;
	IMAGE_DOS_HEADER*					pDosHeader = nullptr; 
	IMAGE_NT_HEADERS*					pNtHeader = nullptr; 

	//32bit and 64bit because that was causing errors
	IMAGE_OPTIONAL_HEADER32*			pOptHeader32 = nullptr;
	IMAGE_OPTIONAL_HEADER64*			pOptHeader64 = nullptr;
	IMAGE_DATA_DIRECTORY*				pDataDirectories = nullptr;

	IMAGE_FILE_HEADER*					pFileHeader = nullptr;
	std::vector<IMAGE_SECTION_HEADER>	pSectionHeaders; 

	size_t								fileSize = 0;
	std::string							fileName;
	bool is64Bit =						false; 

	std::vector<ImportedDLL>			imports;
	std::vector<ExportedFunction>		exports;


	Architecture						arch = Architecture::Unknown;

	
	const char*							getArchString() const;
	const char*							getFileTypeString() const;
};



namespace Info
{
	extern									fileInfo targetFile; 
			std::unique_ptr<std::ifstream>	getFile(const char* theDllPath);
			std::pair<bool, uintptr_t>		checkFileSize(std::unique_ptr<std::ifstream>& dll);
			bool							loaddll(std::unique_ptr<std::ifstream>& dll);
			Architecture					getArchitecture();
			void							getHeaders();
			size_t							RvaToOffset(DWORD rva);
			void							ParseDataDirectories();
			void							ParseImports();
			void							ParseExports();
			void							getSectionHeaders(); 
}
