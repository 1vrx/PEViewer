/*
main.cpp
*/

#include "gui.h"
#include <thread>
#include "../imgui/imgui.h"
#include "globals.h"	
#include "PE/PEInfo.h"




// --- App State ---
static char		filePathBuffer[512]		= { 0 }; // inputbuffer
static int		highlightSelection		= 0; // 0-NONE, 1-DOS, 2-NT, 3-SECTIONS, 4&Above-INDIVIDUAL_SECTIONS
static size_t	highlightStart			= 0;
static size_t	highlightEnd			= 0;
static size_t	g_scrollToOffset		= (size_t)-1; // scroll

int __stdcall wWinMain(
	HINSTANCE instance,
	HINSTANCE prevInstance,
	PWSTR args,
	int commandShow)
{
	gui::CreateHWindow("PEViewer - github.com/1vrx", "gui-1");
	gui::CreateDevice();
	gui::CreateImGui();

	while (gui::exit)
	{
		gui::BeginRender();

		//check if we dropped a file on this iteration
		if (gui::fileWasDropped)
		{
			strncpy_s(filePathBuffer, gui::droppedFilePath, sizeof(filePathBuffer));


			auto file = Info::getFile(filePathBuffer);
			if (file)
			{
				auto [success, size] = Info::checkFileSize(file);
				if (success)
				{
					Info::targetFile.fileSize = size;
					Info::loaddll(file);
					Info::getHeaders();

					std::string pathStr(filePathBuffer);
					size_t lastSlash = pathStr.find_last_of("\\/");
					Info::targetFile.fileName = (lastSlash == std::string::npos) ? pathStr : pathStr.substr(lastSlash + 1);

					highlightSelection = 0; 
				}
			}

			gui::fileWasDropped = false;
			memset(gui::droppedFilePath, 0, sizeof(gui::droppedFilePath));
		}


		//start
		ImGui::SetNextWindowPos({ 0,0 });
		ImGui::SetNextWindowSize({ (float)gui::width, (float)gui::height });
		ImGui::Begin("PEViewer", &gui::exit, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

		ImGui::Text("PEViewer - github.com/1vrx");
		ImGui::SameLine(ImGui::GetWindowWidth() - 50);
		if (ImGui::Button("Exit")) { gui::exit = false; }
		ImGui::Separator();


		if (ImGui::Button("Main", { 340, 25 }))
		{
			menu::tab = 0;
		}
		ImGui::SameLine();
		if (ImGui::Button("Info", { 340, 25 }))
		{
			menu::tab = 1;
		}
		ImGui::Separator();


		if (menu::tab == 0)	//main
		{
			ImGui::InputText("File Path", filePathBuffer, IM_ARRAYSIZE(filePathBuffer));
			ImGui::SameLine();
			if (ImGui::Button("Load File", { 100, 20 }))
			{
				auto file = Info::getFile(filePathBuffer);
				if (file)
				{
					auto [success, size] = Info::checkFileSize(file);
					if (success)
					{
						Info::targetFile.fileSize = size;
						Info::loaddll(file);
						Info::getHeaders();

						std::string pathStr(filePathBuffer);
						size_t lastSlash = pathStr.find_last_of("\\/");
						Info::targetFile.fileName = (lastSlash == std::string::npos) ? pathStr : pathStr.substr(lastSlash + 1);

						//needed
						highlightSelection = 0;
					}
				}
			}

			ImGui::SameLine();
			ImGui::SetNextItemWidth(50);

			std::vector<std::string> highlightItems;
			highlightItems.push_back("Highlight: None");
			highlightItems.push_back("DOS Header");
			highlightItems.push_back("NT Headers");
			highlightItems.push_back("Section Headers");
			for (const auto& sec : Info::targetFile.pSectionHeaders)
			{
				highlightItems.push_back((char*)sec.Name);
			}

			std::vector<const char*> c_items;
			for (const auto& s : highlightItems)
			{
				c_items.push_back(s.c_str());
			}

			if (ImGui::Combo("selectHighlight", &highlightSelection, c_items.data(), c_items.size()))
			{
				//only run on selection
				if (Info::targetFile.pDosHeader)
				{

					//none
					if (highlightSelection == 0) 
					{
						highlightStart = 0;
						highlightEnd = 0;
					}
					//dos
					else if (highlightSelection == 1) 
					{
						highlightStart = 0;
						highlightEnd = sizeof(IMAGE_DOS_HEADER);
					}
					//nt
					else if (highlightSelection == 2 && Info::targetFile.pNtHeader) 
					{
						highlightStart = Info::targetFile.pDosHeader->e_lfanew;
						highlightEnd = highlightStart + sizeof(IMAGE_NT_HEADERS); // add optional header size diff...
					}
					//section
					else if (highlightSelection == 3 && Info::targetFile.pNtHeader) 
					{
						highlightStart = Info::targetFile.pDosHeader->e_lfanew + sizeof(IMAGE_NT_HEADERS);
						highlightEnd = highlightStart + (Info::targetFile.pFileHeader->NumberOfSections * sizeof(IMAGE_SECTION_HEADER));
					}
					//individual
					else if (highlightSelection > 3) 
					{
						int secIndex = highlightSelection - 4; 
						if (secIndex < Info::targetFile.pSectionHeaders.size())
						{
							auto& sec = Info::targetFile.pSectionHeaders[secIndex];
							highlightStart = sec.PointerToRawData;
							highlightEnd = highlightStart + sec.SizeOfRawData;

							g_scrollToOffset = highlightStart; //remove if you dont want auto scroll
						}
					}
				}
			}

			ImGui::Separator();



			ImGui::BeginChild("HexView", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);


			if (g_scrollToOffset != (size_t)-1)
			{
				const int bytesPerLine = 16;
				float lineNum = (float)g_scrollToOffset / bytesPerLine;
				float lineHeight = ImGui::GetTextLineHeightWithSpacing();
				ImGui::SetScrollY(lineNum * lineHeight);
				g_scrollToOffset = (size_t)-1; 
			}

			gui::DrawHex(highlightStart, highlightEnd); 
			ImGui::EndChild();
		}


		//file info
		if (menu::tab == 1)	
		{
			if (Info::targetFile.fileSize == 0 || !Info::targetFile.pNtHeader)
			{
				ImGui::Text("No valid PE file loaded.");
			}
			else
			{
				ImGui::Text("File Info:");
				ImGui::Text("Name: %s", Info::targetFile.fileName.c_str());
				ImGui::Text("Size: %zu bytes", Info::targetFile.fileSize);
				ImGui::Text("Machine: %s", Info::targetFile.getArchString());
				ImGui::Text("File Type: %s", Info::targetFile.getFileTypeString());

				ImGui::NewLine();
				ImGui::Text("PE Info:");

				unsigned long long imageBase = Info::targetFile.is64Bit ? Info::targetFile.pOptHeader64->ImageBase : Info::targetFile.pOptHeader32->ImageBase;
				DWORD sizeOfImage = Info::targetFile.is64Bit ? Info::targetFile.pOptHeader64->SizeOfImage : Info::targetFile.pOptHeader32->SizeOfImage;
				DWORD entryPoint = Info::targetFile.is64Bit ? Info::targetFile.pOptHeader64->AddressOfEntryPoint : Info::targetFile.pOptHeader32->AddressOfEntryPoint;

				ImGui::Text("ImageBase: 0x%llX", imageBase);
				ImGui::Text("SizeOfImage: %u bytes", sizeOfImage);
				ImGui::Text("AddressOfEntryPoint: 0x%X", entryPoint);
				ImGui::Text("NumberOfSections: %u", Info::targetFile.pFileHeader->NumberOfSections);

				ImGui::NewLine();
				ImGui::Text("Sections:");

				if (ImGui::BeginTable("SectionsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
				{

					ImGui::TableSetupColumn("Name");
					ImGui::TableSetupColumn("VAddress (RVA)");
					ImGui::TableSetupColumn("VSize");
					ImGui::TableSetupColumn("Raw Ptr");
					ImGui::TableSetupColumn("Raw Size");
					ImGui::TableHeadersRow();

					for (const auto& section : Info::targetFile.pSectionHeaders)
					{
						ImGui::TableNextRow();
						ImGui::TableNextColumn(); ImGui::Text("%s", (char*)section.Name);
						ImGui::TableNextColumn(); ImGui::Text("0x%X", section.VirtualAddress);
						ImGui::TableNextColumn(); ImGui::Text("%u", section.Misc.VirtualSize);
						ImGui::TableNextColumn(); ImGui::Text("0x%X", section.PointerToRawData);
						ImGui::TableNextColumn(); ImGui::Text("%u", section.SizeOfRawData);
					}
					ImGui::EndTable();
				}


				ImGui::NewLine();

				ImGui::BeginChild("DataDirColumnLeft", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, -1), false, ImGuiWindowFlags_NoScrollbar);
				ImGui::Text("Imports (%zu DLLs)", Info::targetFile.imports.size());
				ImGui::Separator();
				if (ImGui::BeginChild("ImportsChild", ImVec2(0, 0), true))
				{
					for (const auto& dll : Info::targetFile.imports)
					{
						if (ImGui::TreeNode(dll.name.c_str()))
						{
							for (const auto& func : dll.functions)
							{
								if (ImGui::Selectable(func.name.c_str()))
								{
									if (ImGui::GetIO().KeyCtrl)
									{
										//if clicked jump to its addr
										g_scrollToOffset = func.fileOffset;
										menu::tab = 0; 
									}
								}
								if (ImGui::IsItemHovered())
									ImGui::SetTooltip("Ctrl+Click to jump to file offset 0x%X", (unsigned int)func.fileOffset);
							}
							ImGui::TreePop();
						}
					}
				}
				ImGui::EndChild(); 
				ImGui::EndChild(); 

				ImGui::SameLine();

				ImGui::BeginChild("DataDirColumnRight", ImVec2(0, -1), false, ImGuiWindowFlags_NoScrollbar);
				ImGui::Text("Exports (%zu)", Info::targetFile.exports.size());
				ImGui::Separator();
				if (ImGui::BeginChild("ExportsChild", ImVec2(0, 0), true))
				{
					for (const auto& func : Info::targetFile.exports)
					{
						char label[256];
						sprintf_s(label, "[%u] %s", func.ordinal + Info::targetFile.pDataDirectories[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress, func.name.c_str());
						if (ImGui::Selectable(label))
						{
							if (ImGui::GetIO().KeyCtrl)
							{
								g_scrollToOffset = func.fileOffset;
								menu::tab = 0; 
							}
						}
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Ctrl+Click to jump to file offset 0x%X", (unsigned int)func.fileOffset);
					}
				}
				ImGui::EndChild(); 
				ImGui::EndChild(); 
			}
		}

		ImGui::End(); 
		gui::EndRender();

		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}

	gui::DestroyImGui();
	gui::DestroyDevice();
	gui::DestroyWindow();

	return EXIT_SUCCESS;
}

