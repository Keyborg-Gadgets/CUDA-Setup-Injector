#include <Windows.h>
#include <string>
#include <sstream>
#include <map>
#include <set>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdint.h>
#include <cstdint>
#include <Shlwapi.h>
#include <filesystem>

#pragma comment(lib, "Shlwapi.lib")

#include "../detours.h"

#if INTPTR_MAX == INT64_MAX
#pragma comment(lib, "../detours64.lib")
#elif INTPTR_MAX == INT32_MAX
#pragma comment(lib, "../detours32.lib")
#else
#error Unknown pointer size or missing size macros!
#endif

// -------------------------------------------------------------------------
// Constants and Global Maps

static const std::wstring CUDA_PATH = L"C:\\Program Files\\NVIDIA GPU Computing Toolkit\\CUDA\\";
std::wstring REDIRECT_PATH = L"";

// For each created directory, store a set of known files.
// Key: original directory path. Value: set of file names we've already seen/copied.
static std::map<std::wstring, std::set<std::wstring>> g_DirectoryFileMap;

// -------------------------------------------------------------------------
// Utility: Check if 'fullString' starts with 'prefix'
static bool StartsWith(const std::wstring& fullString, const std::wstring& prefix)
{
    if (fullString.size() < prefix.size()) return false;
    // Case-sensitive check; adjust if you need case-insensitive
    return fullString.compare(0, prefix.size(), prefix) == 0;
}

// -------------------------------------------------------------------------
// Utility: Redirect path from one prefix to another, if it matches
static std::wstring RedirectPath(const std::wstring& originalPath,
    const std::wstring& oldPrefix,
    const std::wstring& newPrefix)
{
    if (StartsWith(originalPath, oldPrefix))
    {
        std::wstring relativePart = originalPath.substr(oldPrefix.size());
        return newPrefix + relativePart;
    }
    return originalPath;
}

// -------------------------------------------------------------------------
// Utility: Get all files in the given directory (non-recursive for simplicity)
static std::vector<std::wstring> GetFilesInDirectory(const std::wstring& directory)
{
    std::vector<std::wstring> files;

    // Construct a search pattern like "C:\\some\\path\\*"
    std::wstring searchPath = directory;
    if (!searchPath.empty() && searchPath.back() != L'\\')
    {
        searchPath += L"\\";
    }
    searchPath += L"*";

    WIN32_FIND_DATAW findData;
    HANDLE hFind = FindFirstFileW(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        return files;
    }

    do
    {
        // Ignore "." and ".." and subdirectories
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            files.push_back(findData.cFileName);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
    return files;
}

// -------------------------------------------------------------------------
// Hook: CreateDirectoryW

static BOOL(WINAPI* TrueCreateDirectoryW)(
    LPCWSTR               lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes
    ) = CreateDirectoryW;

BOOL WINAPI TracedCreateDirectoryW(
    LPCWSTR lpPathName,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes
)
{
    std::wostringstream debugOutput;
    debugOutput << L"Traced CreateDirectoryW called with: "
        << (lpPathName ? lpPathName : L"(null)") << std::endl;
    OutputDebugStringW(debugOutput.str().c_str());

    std::wstring original(lpPathName ? lpPathName : L"");
    std::wstring newPath = RedirectPath(original, CUDA_PATH, REDIRECT_PATH);

    if (newPath != original)
    {
        std::wostringstream redirectMsg;
        redirectMsg << L"Redirecting directory creation from "
            << original << L" to " << newPath << std::endl;
        OutputDebugStringW(redirectMsg.str().c_str());

        // Optionally create directory under the new path
        CreateDirectoryW(newPath.c_str(), lpSecurityAttributes);
    }

    // Add this directory to our global map (if not already present).
    if (g_DirectoryFileMap.find(original) == g_DirectoryFileMap.end())
    {
        g_DirectoryFileMap[original] = std::set<std::wstring>();
    }

    // Call the real CreateDirectoryW
    return TrueCreateDirectoryW(lpPathName, lpSecurityAttributes);
}

// -------------------------------------------------------------------------
// Hook: GetFileAttributesW

static DWORD(WINAPI* TrueGetFileAttributesW)(
    LPCWSTR lpFileName
    ) = GetFileAttributesW;

DWORD WINAPI HookedGetFileAttributesW(
    LPCWSTR lpFileName
)
{
    // If the path begins with CUDA_PATH, then do our scanning/copy logic.
    // Otherwise, just call the real GetFileAttributesW.

    if (lpFileName && StartsWith(lpFileName, CUDA_PATH))
    {
        // Log that we encountered a path in the CUDA folder
        {
            std::wostringstream debugOut;
            debugOut << L"[Detour] HookedGetFileAttributesW called for path: "
                << lpFileName << std::endl;
            OutputDebugStringW(debugOut.str().c_str());
        }

        // Scan all known directories only if they are also under CUDA_PATH
        // (depending on your design, you might filter here or check all).
        for (auto& dirEntry : g_DirectoryFileMap)
        {
            const std::wstring& originalDir = dirEntry.first;
            std::set<std::wstring>& knownFiles = dirEntry.second;

            // If you only want to check directories that start with CUDA_PATH:
            if (!StartsWith(originalDir, CUDA_PATH))
            {
                // skip scanning this directory if it's outside CUDA_PATH
                continue;
            }

            // Get the list of current files in this directory
            std::vector<std::wstring> currentFiles = GetFilesInDirectory(originalDir);

            for (const auto& file : currentFiles)
            {
                if (knownFiles.find(file) == knownFiles.end())
                {
                    // This is a new file we haven't seen yet
                    std::wostringstream newFileMsg;
                    newFileMsg << L"New file found: " << file
                        << L" in directory: " << originalDir << std::endl;
                    OutputDebugStringW(newFileMsg.str().c_str());

                    // Mark it as known so we don't copy it multiple times
                    knownFiles.insert(file);

                    // Build full original file path
                    std::wstring originalFilePath = originalDir;
                    if (!originalFilePath.empty() && originalFilePath.back() != L'\\')
                    {
                        originalFilePath += L"\\";
                    }
                    originalFilePath += file;

                    // Build the redirected path for that file
                    std::wstring redirectedFilePath =
                        RedirectPath(originalFilePath, CUDA_PATH, REDIRECT_PATH);

                    // Copy the file from original -> redirected location
                    if (!CopyFileW(originalFilePath.c_str(), redirectedFilePath.c_str(), FALSE))
                    {
                        std::wostringstream copyErrorMsg;
                        copyErrorMsg << L"[ERROR] CopyFileW failed from "
                            << originalFilePath << L" to " << redirectedFilePath
                            << L" (error code: " << GetLastError() << L")" << std::endl;
                        OutputDebugStringW(copyErrorMsg.str().c_str());
                    }
                    else
                    {
                        std::wostringstream copyMsg;
                        copyMsg << L"Copied new file from "
                            << originalFilePath << L" to " << redirectedFilePath << std::endl;
                        OutputDebugStringW(copyMsg.str().c_str());
                    }
                }
            }
        }
    }

    // Always call the real GetFileAttributesW in the end
    return TrueGetFileAttributesW(lpFileName);
}

// -------------------------------------------------------------------------
// DllMain

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    wchar_t buffer[MAX_PATH + 1];
    auto len = ::GetTempPath(MAX_PATH + 1, buffer);

    std::filesystem::path binaries_path{ {buffer, len} };
    binaries_path /= L"takedetour";
    std::filesystem::path outputPath = binaries_path / L"path.txt";

    std::wifstream inputFile(outputPath);
    if (inputFile) {
        std::getline(inputFile, REDIRECT_PATH); 
    }
    else {
        wprintf(L"Failed to open file: %s\n", outputPath.c_str());
    }

    OutputDebugStringW((L"Path is: " + REDIRECT_PATH).c_str());


    if (DetourIsHelperProcess()) {
        return TRUE;
    }

    LONG error = NO_ERROR;

    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        DetourRestoreAfterWith();

        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        // Hook CreateDirectoryW
        DetourAttach(&(PVOID&)TrueCreateDirectoryW, TracedCreateDirectoryW);

        // Hook GetFileAttributesW
        DetourAttach(&(PVOID&)TrueGetFileAttributesW, HookedGetFileAttributesW);

        error = DetourTransactionCommit();
        if (error != NO_ERROR)
        {
            std::wostringstream output;
            output << L"[ERROR] DetourTransactionCommit (ATTACH) failed: " << error;
            OutputDebugStringW(output.str().c_str());
        }
        break;

    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        // Nothing special here
        break;

    case DLL_PROCESS_DETACH:
        DetourTransactionBegin();
        DetourUpdateThread(GetCurrentThread());

        DetourDetach(&(PVOID&)TrueCreateDirectoryW, TracedCreateDirectoryW);
        DetourDetach(&(PVOID&)TrueGetFileAttributesW, HookedGetFileAttributesW);

        error = DetourTransactionCommit();
        if (error != NO_ERROR)
        {
            std::wostringstream output;
            output << L"[ERROR] DetourTransactionCommit (DETACH) failed: " << error;
            OutputDebugStringW(output.str().c_str());
        }
        break;
    }
    return TRUE;
}
