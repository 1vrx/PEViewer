/*
Gui.cpp
*/

#define _CRT_SECURE_NO_WARNINGS

#include "gui.h"

#include "../imgui/imgui.h"
#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"

#include <string>	
#include <sstream> 
#include "PE/PEInfo.h" 


extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: 
	{
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = LOWORD(longParameter);
			gui::ResetDevice();
		}
	}
	return 0;

	case WM_SYSCOMMAND: 
	{
		if ((wideParameter & 0xfff0) == SC_KEYMENU)	//disable ALT app menu
			return 0;
	}break;

	case WM_DESTROY:
	{
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: 
	{
		gui::position = MAKEPOINTS(longParameter); //move the gui to where we clicked
	}return 0;

	case WM_MOUSEMOVE:
	{
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{};

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 && gui::position.x <= gui::width &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(gui::window, HWND_TOPMOST, rect.left, rect.top, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER);
		}
	}return 0;

	case WM_DROPFILES:
	{
		HDROP hDrop = (HDROP)wideParameter;
		UINT fileCount = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);

		if (fileCount > 0)
		{
			
			if (DragQueryFile(hDrop, 0, gui::droppedFilePath, sizeof(gui::droppedFilePath)))
			{
				gui::fileWasDropped = true;
			}
		}
		DragFinish(hDrop);
		return 0;
	}

	}	//end of switch

	return DefWindowProcW(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(
	const char* windowName,
	const char* className) noexcept
{
	windowClass.cbSize =  sizeof(WNDCLASSEXA);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = (WNDPROC)WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = className;
	windowClass.hIconSm = 0;

	RegisterClassExA(&windowClass);

	window = CreateWindowA(
		className,
		windowName,
		WS_POPUP,
		500,
		500,
		width,
		height,
		0,
		0,
		windowClass.hInstance,
		0
	);

	DragAcceptFiles(window, TRUE);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &presentParameters, &device) < 0) return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}
	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessageA(&message);
	}

	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

void gui::DrawHex(size_t highlightStart, size_t highlightEnd)
{
	if (Info::targetFile.data.empty())
	{
		ImGui::Text("No file loaded.");
		return;
	}

	ImGuiListClipper clipper;
	const int bytesPerLine = 16;
	const int numLines = (Info::targetFile.fileSize + bytesPerLine - 1) / bytesPerLine;
	clipper.Begin(numLines);

	while (clipper.Step())
	{
		for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
		{
			size_t lineStartOffset = line * bytesPerLine;

			ImGui::Text("%08X: ", lineStartOffset);
			ImGui::SameLine();

			for (int i = 0; i < bytesPerLine; ++i)
			{
				size_t currentOffset = lineStartOffset + i;
				if (currentOffset >= Info::targetFile.fileSize)
					break;

				bool highlight = (currentOffset >= highlightStart && currentOffset < highlightEnd);
				if (highlight)
					ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 0, 255)); // Yellow

				char buf[4];
				sprintf(buf, "%02X ", Info::targetFile.data[currentOffset]);
				ImGui::TextUnformatted(buf);

				if (highlight) ImGui::PopStyleColor();

				//check if im clicking the byte
				if (ImGui::IsItemClicked() && ImGui::GetIO().KeyCtrl)
				{
					hexPopupOffset = currentOffset;
					ImGui::OpenPopup("Byte Info");
				}

				if ((i % bytesPerLine) != (bytesPerLine - 1))
					ImGui::SameLine(0, 0);
			}

			ImGui::SameLine(); ImGui::Text("  | "); ImGui::SameLine();

			//draw hex->ascii
			for (int i = 0; i < bytesPerLine; ++i)
			{
				size_t currentOffset = lineStartOffset + i;
				if (currentOffset >= Info::targetFile.fileSize)
					break;

				char c = Info::targetFile.data[currentOffset];
				ImGui::Text("%c", (c >= 32 && c < 127) ? c : '.'); //ensure valid ascii
				if ((i % bytesPerLine) != (bytesPerLine - 1))
					ImGui::SameLine(0, 0);
			}
		}
	}
	clipper.End();

	// if clicked -> popup
	if (ImGui::BeginPopup("Byte Info"))
	{
		ShowByteDetails(hexPopupOffset);
		ImGui::EndPopup();
	}
}

void gui::ShowByteDetails(size_t offset)
{
	if (Info::targetFile.data.empty())
	{
		ImGui::Text("File not loaded.");
		return;
	}

	auto* pDos = Info::targetFile.pDosHeader;
	auto* pNt = Info::targetFile.pNtHeader;

	ImGui::Text("Offset: 0x%X (%zu)", (unsigned int)offset, offset);
	ImGui::Separator();

	if (!pDos)
	{
		ImGui::Text("Not a valid PE file (No DOS Header).");
		return;
	}

	// Check DOS Header
	if (offset < sizeof(IMAGE_DOS_HEADER))
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Location: IMAGE_DOS_HEADER");
		if (offset == offsetof(IMAGE_DOS_HEADER, e_magic)) ImGui::Text("Field: e_magic (Magic number, should be 'MZ')");
		else if (offset == offsetof(IMAGE_DOS_HEADER, e_lfanew)) ImGui::Text("Field: e_lfanew (Offset to new NT/PE header)");
		else ImGui::Text("Field: (Other DOS Header field)");
		return;
	}

	if (!pNt)
	{
		ImGui::Text("Not a valid PE file (No NT Header).");
		return;
	}

	// ntHeader
	size_t ntHeaderStart = pDos->e_lfanew;
	size_t ntHeaderEnd = ntHeaderStart + sizeof(IMAGE_NT_HEADERS);
	if (offset >= ntHeaderStart && offset < ntHeaderEnd)
	{
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "Location: IMAGE_NT_HEADERS");
		if (offset == ntHeaderStart + offsetof(IMAGE_NT_HEADERS, Signature)) ImGui::Text("Field: Signature (Should be 'PE\\0\\0')");

		// fileHeader
		else if (offset >= ntHeaderStart + offsetof(IMAGE_NT_HEADERS, FileHeader) &&
			offset < ntHeaderStart + offsetof(IMAGE_NT_HEADERS, OptionalHeader))
		{
			ImGui::Text("Part of: IMAGE_FILE_HEADER");
			size_t fieldOffset = offset - (ntHeaderStart + offsetof(IMAGE_NT_HEADERS, FileHeader));
			if (fieldOffset == offsetof(IMAGE_FILE_HEADER, Machine)) ImGui::Text("Field: Machine (Architecture)");
			else if (fieldOffset == offsetof(IMAGE_FILE_HEADER, NumberOfSections)) ImGui::Text("Field: NumberOfSections");
			else if (fieldOffset == offsetof(IMAGE_FILE_HEADER, SizeOfOptionalHeader)) ImGui::Text("Field: SizeOfOptionalHeader");
			else if (fieldOffset == offsetof(IMAGE_FILE_HEADER, Characteristics)) ImGui::Text("Field: Characteristics (e.g., DLL, EXE)");
		}
		// optionalHeader
		else if (offset >= ntHeaderStart + offsetof(IMAGE_NT_HEADERS, OptionalHeader) &&
			offset < ntHeaderStart + offsetof(IMAGE_NT_HEADERS, OptionalHeader) + pNt->FileHeader.SizeOfOptionalHeader)
		{
			ImGui::Text("Part of: IMAGE_OPTIONAL_HEADER");
			size_t fieldOffset = offset - (ntHeaderStart + offsetof(IMAGE_NT_HEADERS, OptionalHeader));

			// ensure correct arch
			if (Info::targetFile.is64Bit)
			{
				// 64bit
				if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER64, Magic)) ImGui::Text("Field: Magic (PE32+)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER64, AddressOfEntryPoint)) ImGui::Text("Field: AddressOfEntryPoint (RVA)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER64, ImageBase)) ImGui::Text("Field: ImageBase (Preferred load address)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER64, SizeOfImage)) ImGui::Text("Field: SizeOfImage");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER64, NumberOfRvaAndSizes)) ImGui::Text("Field: NumberOfRvaAndSizes (Data Directories)");
			}
			else
			{
				// 32bit
				if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER32, Magic)) ImGui::Text("Field: Magic (PE32)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER32, AddressOfEntryPoint)) ImGui::Text("Field: AddressOfEntryPoint (RVA)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER32, ImageBase)) ImGui::Text("Field: ImageBase (Preferred load address)");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER32, SizeOfImage)) ImGui::Text("Field: SizeOfImage");
				else if (fieldOffset == offsetof(IMAGE_OPTIONAL_HEADER32, NumberOfRvaAndSizes)) ImGui::Text("Field: NumberOfRvaAndSizes (Data Directories)");
			}
		}
		return;
	}

	// sectionsHeader
	size_t sectionHeadersStart = ntHeaderStart + sizeof(IMAGE_NT_HEADERS);
	size_t sectionHeadersEnd = sectionHeadersStart + (pNt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER));
	if (offset >= sectionHeadersStart && offset < sectionHeadersEnd)
	{
		int sectionIndex = (offset - sectionHeadersStart) / sizeof(IMAGE_SECTION_HEADER);
		auto& section = Info::targetFile.pSectionHeaders[sectionIndex];

		std::stringstream ss;
		ss << "Location: Section Header #" << sectionIndex << " (" << (char*)section.Name << ")";
		ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", ss.str().c_str());

		size_t fieldOffset = (offset - sectionHeadersStart) % sizeof(IMAGE_SECTION_HEADER);
		if (fieldOffset >= offsetof(IMAGE_SECTION_HEADER, Name) && fieldOffset < offsetof(IMAGE_SECTION_HEADER, Name) + 8) ImGui::Text("Field: Name");
		else if (fieldOffset == offsetof(IMAGE_SECTION_HEADER, Misc.VirtualSize)) ImGui::Text("Field: VirtualSize");
		else if (fieldOffset == offsetof(IMAGE_SECTION_HEADER, VirtualAddress)) ImGui::Text("Field: VirtualAddress (RVA)");
		else if (fieldOffset == offsetof(IMAGE_SECTION_HEADER, SizeOfRawData)) ImGui::Text("Field: SizeOfRawData");
		else if (fieldOffset == offsetof(IMAGE_SECTION_HEADER, PointerToRawData)) ImGui::Text("Field: PointerToRawData (File Offset)");
		else if (fieldOffset == offsetof(IMAGE_SECTION_HEADER, Characteristics)) ImGui::Text("Field: Characteristics (e.g., Read, Write, Exec)");

		return;
	}

	// check section data
	for (const auto& section : Info::targetFile.pSectionHeaders)
	{
		size_t rawStart = section.PointerToRawData;
		size_t rawEnd = rawStart + section.SizeOfRawData;
		if (offset >= rawStart && offset < rawEnd)
		{
			std::stringstream ss;
			ss << "Location: Section Data: " << (char*)section.Name;
			ImGui::TextColored(ImVec4(0, 1, 1, 1), "%s", ss.str().c_str());
			ImGui::Text("Offset within section: +0x%X", (unsigned int)(offset - rawStart));
			return;
		}
	}

	ImGui::Text("Location: Unknown (e.g., DOS Stub or file padding)");
}


