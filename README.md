# PEViewer

A lightweight, robust, and GUI-independent Portable Executable (PE) parser with an ImGui frontend. 

## Releases & Demo
Download the latest pre-compiled build from the **[Releases tab](../../releases)**. 
* **To use:** Drag and drop any `.exe`, `.dll`, or `.sys` file into the application window.
* **To test error handling:** Drop a truncated or corrupted PE file. The UI will catch the parsing fault and display a structured error state rather than crashing.

## Architecture
PEViewer is built on a strict separation of concerns, ensuring that UI rendering and binary parsing do not overlap:
* **`PEParser` (Backend):** A completely GUI-independent, bounded parser. It utilizes a `BufferView` concept (`ReadAt<T>`) to guarantee memory safety against malformed headers, malicious section sizes, and out-of-bounds RVAs. It uses a `Result<T>` pattern for structured error reporting.
* **ImGui Frontend:** A stateless renderer that asynchronously dispatches parsing requests and maps the validated structures (like `ExportInfo` and `ImportedDLL`) to visual tables and hex viewers. 

## Supported Toolchain & Configuration
* **OS:** Windows 10 / 11
* **IDE:** Visual Studio 2022
* **Language Standard:** C++17 (or newer)
* **Architecture:** x64 (Primary) / x86
* **Dependencies:** DirectX 9 (for ImGui rendering). All ImGui headers are included in the repository.

### Building from Source
1. Clone the repository.
2. Open `PEViewer.sln` in Visual Studio 2022.
3. Set the build configuration to `Release` and platform to `x64`.
4. Build Solution (`Ctrl+Shift+B`).


## Known Limitations
* **Read-Only:** This tool is an analyzer/viewer. It does not support modifying or rebuilding PE files.
* **.NET/CLR:** Deep inspection of .NET metadata directories is not yet fully supported.
* **Packers:** Highly obfuscated or packed executables (e.g., UPX, VMP) will parse structurally, but the true import table or payload will remain compressed/hidden.

## Contributing
1. Fork the repository.
2. Create a feature branch (`git checkout -b feature/NewFeature`).
3. Ensure the standalone regression suite (`test_main.cpp`) compiles and passes.
4. Open a Pull Request detailing the architectural impact of your change.
