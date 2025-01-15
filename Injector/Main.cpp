// Injector
#include "Injector.h"
#include "Seh.h"
#include "StringWrap.h"
#include "argh.h"
#include "StringUtil.h"
#include "resource.h"

// Windows API
#include <Windows.h>
#include <tchar.h>
#include <TlHelp32.h>
#include <Shlwapi.h>
#include <wil/result.h>

// C++ Standard Library
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <locale>
#include <filesystem>

// Return values
#define RESULT_SUCCESS          0
#define RESULT_INVALID_COMMAND  1
#define RESULT_GENERAL_ERROR    2
#define RESULT_SEH_ERROR        3
#define RESULT_UNKNOWN_ERROR    4

inline std::string getExecutableDirectory() {
    char path[MAX_PATH];
    DWORD length = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (length == 0) {
        return "";
    }
    std::string fullPath(path, length);
    size_t pos = fullPath.find_last_of("\\/");
    return (pos == std::string::npos) ? "" : fullPath.substr(0, pos);
}

std::filesystem::path unpack_dependencies() {
    auto unpack_binary_file = [](int resid, const std::wstring& dest_path) {
        // find location of the resource and get handle to it
        auto resource_dll = ::FindResource(NULL, MAKEINTRESOURCE(resid), L"DetourDLL");
        THROW_LAST_ERROR_IF_NULL(resource_dll);

        // loads the specified resource into global memory.
        auto resource = ::LoadResource(NULL, resource_dll);
        THROW_LAST_ERROR_IF_NULL(resource);

        // get a pointer to the loaded resource!
        const auto resource_data = static_cast<char*>(::LockResource(resource));
        THROW_LAST_ERROR_IF_NULL(resource_data);

        // determine the size of the resource, so we know how much to write out to file!
        auto resource_size = ::SizeofResource(NULL, resource_dll);
        THROW_LAST_ERROR_IF_MSG(resource_size == 0, "SizeofResource");

        std::ofstream outputFile(dest_path, std::ios::binary);
        outputFile.write(resource_data, resource_size);
        outputFile.close();
        };

    wchar_t buffer[MAX_PATH + 1];
    auto len = ::GetTempPath(MAX_PATH + 1, buffer);
    assert(len <= MAX_PATH + 1);
    THROW_LAST_ERROR_IF(len == 0);

    std::filesystem::path binaries_path{ {buffer, len} };
    binaries_path /= L"takedetour";

    wchar_t buffer1[MAX_PATH]; 
    GetModuleFileNameW(NULL, buffer1, MAX_PATH); 
    std::filesystem::path exePath(buffer1); 
    std::filesystem::path exeDir;
    std::filesystem::path cudaDir = exeDir / L"CUDA"; 
    std::filesystem::create_directory(cudaDir);
    std::filesystem::path outputPath = binaries_path / L"path.txt";
    std::ofstream outFile(outputPath);
    if (outFile.is_open()) {
        outFile << getExecutableDirectory() << "\\CUDA\\";
        outFile.close();
        std::wcout << L"Path written to " << outputPath << std::endl;
    }
    else {
        std::wcerr << L"Failed to open file: " << outputPath << std::endl;
    }

    THROW_LAST_ERROR_IF_MSG(!::CreateDirectory(binaries_path.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS,
        "Creation of the unpack folder failed.");

    unpack_binary_file(102, binaries_path / L"injectdll32.dll");

    return binaries_path;
}

// Entry point
int main(int, char* argv[])
{
    try
    {
        auto injectdll = unpack_dependencies() / "injectdll32.dll";

        // Needed to proxy SEH exceptions to C++ exceptions
        SehGuard Guard;

        // Injector version number
        const std::tstring VerNum(_T("20240218"));

        // Version and copyright output
#ifdef _WIN64
        std::tcout << _T("Injector x64 [Version ") << VerNum << _T("]") << std::endl;
#else
        std::tcout << _T("Injector x86 [Version ") << VerNum << _T("]") << std::endl;
#endif
        std::tcout << _T("Copyright (c) 2009 Cypher, 2012-2024 Nefarius. All rights reserved.") << std::endl << std::endl;

        DWORD ProcID = 0;
        std::tstring ModulePath;
        ProcID = Injector::Get()->GetProcessIdByName(L"setup.exe", false);

        Injector::Get()->GetSeDebugPrivilege();
        Injector::Get()->InjectLib(ProcID, injectdll);
        std::tcout << "Successfully injected module!" << std::endl;
    }
    // Catch STL-based exceptions.
    catch (const std::exception& e)
    {
        std::string TempError(e.what());
        std::tstring Error(TempError.begin(), TempError.end());
        std::tcerr << "General Error:" << std::endl
            << Error << std::endl;
        return RESULT_GENERAL_ERROR;
    }
    // Catch custom SEH-proxy exceptions.
    // Currently only supports outputting error code.
    // TODO: Convert to string and dump more verbose output.
    catch (const SehException& e)
    {
        std::tcerr << "SEH Error:" << std::endl
            << e.GetCode() << std::endl;
        return RESULT_SEH_ERROR;
    }
    // Catch any other unknown exceptions.
    // TODO: Find a better way to handle this. Should never happen anyway, but
    // you never know.
    // Note: Could use SetUnhandledExceptionFilter but would potentially be 
    // messy.
    catch (...)
    {
        std::tcerr << "Unknown error!" << std::endl;
        return RESULT_UNKNOWN_ERROR;
    }

    // Return success
    return RESULT_SUCCESS;
}
