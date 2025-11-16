/*
gui.h
*/

#pragma once

#include <d3d9.h>


#include "PE/PEInfo.h"

namespace gui
{
	constexpr	int						width				= 700;
	constexpr	int						height				= 700;

	inline		bool					exit				= true;

	inline		HWND					window				= nullptr;
	inline		WNDCLASSEXA				windowClass			= { };

	inline		POINTS					position			= { };

	inline		PDIRECT3D9				d3d					= nullptr;
	inline		LPDIRECT3DDEVICE9		device				= nullptr;
	inline		D3DPRESENT_PARAMETERS	presentParameters	= { };

	inline		size_t					hexPopupOffset		= 0;

	inline		char					droppedFilePath[512] = { 0 };
	inline		bool					fileWasDropped		= false;

				void					CreateHWindow(
											const char* windowName,
											const char* className)	noexcept;
				void					DestroyWindow()				noexcept;

				bool					CreateDevice()				noexcept;
				void					ResetDevice()				noexcept;
				void					DestroyDevice()				noexcept;

				void					CreateImGui()				noexcept;
				void					DestroyImGui()				noexcept;

				void					BeginRender()				noexcept;
				void					EndRender()					noexcept;
				void					ShowByteDetails(
											size_t offset);
				void					DrawHex(
											size_t highlightStart, 
											size_t highlightEnd);
				void					Render()					noexcept;

}


