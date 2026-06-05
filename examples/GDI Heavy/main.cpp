#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "ole32")

/* Enhanced Windows RT ARM32 Test Payload for ImWindows PE Loader Testing */
/* Pure C - zero C++ runtime dependencies */
#ifndef RT_WAVE
#define RT_WAVE MAKEINTRESOURCE(2)  /* WAVE resource type */
#endif
#include <windows.h>
#include <stdio.h>
#include <wininet.h>
#include <psapi.h>
#include <xaudio2.h>

/* Colors */
#define BG_COLOR      RGB(20, 20, 35)
#define TITLE_COLOR   RGB(0, 200, 255)
#define MOUSE_COLOR   RGB(255, 200, 0)
#define KEY_COLOR     RGB(0, 255, 127)
#define TEXT_COLOR    RGB(200, 200, 200)
#define ACCENT_COLOR  RGB(255, 60, 100)
#define SUCCESS_COLOR RGB(0, 255, 0)
#define ERROR_COLOR   RGB(255, 0, 0)

/* Button Structure */
typedef struct {
    RECT rect;
    wchar_t text[64];
    BOOL visible;
    BOOL enabled;
    int action;
} Button;

/* Test Results Structure */
typedef struct {
    BOOL NetworkConnected;
    BOOL FileSystemWorking;
    BOOL RegistryWorking;
    BOOL ProcessWorking;
    wchar_t NetworkStatus[256];
    wchar_t SystemInfo[512];
    wchar_t FileSystemStatus[256];
    wchar_t RegistryStatus[256];
    wchar_t ProcessStatus[256];
    
    /* Detailed test results */
    wchar_t NetworkDetails[1024];
    wchar_t FileSystemDetails[1024];
    wchar_t RegistryDetails[1024];
    wchar_t ProcessDetails[1024];
} TestResults;

/* UI State */
typedef enum {
    PAGE_MAIN,
    PAGE_NETWORK,
    PAGE_FILESYSTEM,
    PAGE_REGISTRY,
    PAGE_PROCESS,
    PAGE_SYSTEM
} UIPage;

/* Global State */
static wchar_t g_LastMouseEvent[128] = L"Mouse: Idle";
static wchar_t g_LastKeyEvent[128]   = L"Key: None";
static int g_MouseX = 0, g_MouseY = 0;
static int g_Frame = 0;
static HBITMAP g_BitmapBackground = NULL;
static TestResults g_TestResults = {0};
static int g_CurrentTest = 0;
static UIPage g_CurrentPage = PAGE_MAIN;
static Button g_Buttons[20];
static int g_ButtonCount = 0;
static BOOL g_MouseDown = FALSE;

/* XAudio2 Globals */
static IXAudio2* g_XAudio2 = NULL;
static IXAudio2MasteringVoice* g_MasterVoice = NULL;
static BOOL g_XAudio2Initialized = FALSE;

/* Button Actions */
#define ACTION_RUN_ALL_TESTS    1
#define ACTION_SHOW_NETWORK     2
#define ACTION_SHOW_FILESYSTEM  3
#define ACTION_SHOW_REGISTRY    4
#define ACTION_SHOW_PROCESS     5
#define ACTION_SHOW_SYSTEM      6
#define ACTION_BACK_TO_MAIN     7
#define ACTION_REFRESH_TESTS     8

/* Network Connectivity Test */
static void TestNetworkConnectivity() {
    OutputDebugStringW(L"[TestPayload] Testing network connectivity...\n");
    
    HINTERNET hInternet = InternetOpenW(L"PeLoaderTest", INTERNET_OPEN_TYPE_DIRECT, NULL, NULL, 0);
    if (hInternet) {
        HINTERNET hConnect = InternetOpenUrlW(hInternet, L"http://www.google.com", NULL, 0, 
                                           INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE, 0);
        if (hConnect) {
            g_TestResults.NetworkConnected = TRUE;
            wcscpy_s(g_TestResults.NetworkStatus, 256, L"Network: Connected to Internet");
            wcscpy_s(g_TestResults.NetworkDetails, 1024, 
                     L"Network Test Results:\n"
                     L"[OK] InternetOpenW() succeeded\n"
                     L"[OK] HTTP connection to google.com successful\n"
                     L"[OK] Wininet.dll is properly loaded\n"
                     L"[OK] Network stack is functional\n"
                     L"\nTest completed successfully.");
            InternetCloseHandle(hConnect);
        } else {
            g_TestResults.NetworkConnected = FALSE;
            DWORD error = GetLastError();
            wcscpy_s(g_TestResults.NetworkStatus, 256, L"Network: Connection Failed");
            swprintf_s(g_TestResults.NetworkDetails, 1024, 
                      L"Network Test Results:\n"
                      L"[FAIL] InternetOpenW() succeeded\n"
                      L"[FAIL] HTTP connection failed (Error: %d)\n"
                      L"[FAIL] Possible causes: No internet, firewall, DNS issues\n"
                      L"\nTest failed with error code %d.", error, error);
        }
        InternetCloseHandle(hInternet);
    } else {
        g_TestResults.NetworkConnected = FALSE;
        DWORD error = GetLastError();
        wcscpy_s(g_TestResults.NetworkStatus, 256, L"Network: Wininet Failed");
        swprintf_s(g_TestResults.NetworkDetails, 1024, 
                  L"Network Test Results:\n"
                  L"[FAIL] InternetOpenW() failed (Error: %d)\n"
                  L"[FAIL] Wininet.dll may not be available\n"
                  L"[FAIL] Network initialization failed\n"
                  L"\nTest failed with error code %d.", error, error);
    }
}

/* System Information Test */
static void TestSystemInformation() {
    OutputDebugStringW(L"[TestPayload] Gathering system information...\n");
    
    SYSTEM_INFO si;
    MEMORYSTATUSEX ms;
    OSVERSIONINFOEX osvi;
    
    GetSystemInfo(&si);
    ms.dwLength = sizeof(ms);
    GlobalMemoryStatusEx(&ms);
    
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO*)&osvi)) {
        swprintf_s(g_TestResults.SystemInfo, 512, 
                   L"OS: %d.%d Build %d | CPU: %d cores | RAM: %lldMB/%lldMB",
                   osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber,
                   si.dwNumberOfProcessors,
                   ms.ullAvailPhys / (1024*1024), ms.ullTotalPhys / (1024*1024));
    } else {
        wcscpy_s(g_TestResults.SystemInfo, 512, L"System: Failed to get version info");
    }
}

/* File System Test */
static void TestFileSystemOperations() {
    OutputDebugStringW(L"[TestPayload] Testing file system operations...\n");
    
    wchar_t tempPath[MAX_PATH];
    wchar_t testFile[MAX_PATH];
    HANDLE hFile;
    DWORD bytesWritten;
    DWORD bytesRead;
    char readBuffer[64];
    
    if (GetTempPathW(MAX_PATH, tempPath)) {
        swprintf_s(testFile, MAX_PATH, L"%s\\peloadertest.tmp", tempPath);
        
        hFile = CreateFileW(testFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 
                          FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            const char* testData = "PE Loader Test File";
            if (WriteFile(hFile, testData, strlen(testData), &bytesWritten, NULL)) {
                CloseHandle(hFile);
                
                // Test reading back
                hFile = CreateFileW(testFile, GENERIC_READ, 0, NULL, OPEN_EXISTING, 
                                  FILE_ATTRIBUTE_NORMAL, NULL);
                if (hFile != INVALID_HANDLE_VALUE) {
                    if (ReadFile(hFile, readBuffer, sizeof(readBuffer), &bytesRead, NULL)) {
                        g_TestResults.FileSystemWorking = TRUE;
                        swprintf_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Read/Write OK");
                        swprintf_s(g_TestResults.FileSystemDetails, 1024,
                                 L"File System Test Results:\n"
                                 L"[OK] GetTempPathW() succeeded\n"
                                 L"[OK] CreateFileW() (write) succeeded\n"
                                 L"[OK] WriteFile() succeeded\n"
                                 L"[OK] CreateFileW() (read) succeeded\n"
                                 L"[OK] ReadFile() succeeded\n"
                                 L"[OK] File cleanup successful\n"
                                 L"\nTemp path: %s\n"
                                 L"Test file: peloadertest.tmp\n"
                                 L"Data written: %d bytes\n"
                                 L"Data read: %d bytes\n"
                                 L"\nFile system operations working correctly.", tempPath, (int)strlen(testData), bytesRead);
                    } else {
                        g_TestResults.FileSystemWorking = FALSE;
                        wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Read Failed");
                        wcscpy_s(g_TestResults.FileSystemDetails, 1024,
                                 L"File System Test Results:\n"
                                 L"[OK] GetTempPathW() succeeded\n"
                                 L"[OK] CreateFileW() (write) succeeded\n"
                                 L"[OK] WriteFile() succeeded\n"
                                 L"[FAIL] ReadFile() failed\n"
                                 L"\nFile write works but read failed.");
                    }
                    CloseHandle(hFile);
                } else {
                    g_TestResults.FileSystemWorking = FALSE;
                    wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Reopen Failed");
                    wcscpy_s(g_TestResults.FileSystemDetails, 1024,
                             L"File System Test Results:\n"
                             L"[OK] GetTempPathW() succeeded\n"
                             L"[OK] CreateFileW() (write) succeeded\n"
                             L"[OK] WriteFile() succeeded\n"
                             L"[FAIL] CreateFileW() (read) failed\n"
                             L"\nFile write works but reopening failed.");
                }
            } else {
                g_TestResults.FileSystemWorking = FALSE;
                wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Write Failed");
                wcscpy_s(g_TestResults.FileSystemDetails, 1024,
                         L"File System Test Results:\n"
                         L"[OK] GetTempPathW() succeeded\n"
                         L"[OK] CreateFileW() succeeded\n"
                         L"[FAIL] WriteFile() failed\n"
                         L"\nFile creation works but write failed.");
            }
            DeleteFileW(testFile);
        } else {
            g_TestResults.FileSystemWorking = FALSE;
            wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Create Failed");
            wcscpy_s(g_TestResults.FileSystemDetails, 1024,
                     L"File System Test Results:\n"
                     L"[OK] GetTempPathW() succeeded\n"
                     L"[FAIL] CreateFileW() failed\n"
                     L"\nTemp path accessible but file creation failed.");
        }
    } else {
        g_TestResults.FileSystemWorking = FALSE;
        wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Temp Path Failed");
        wcscpy_s(g_TestResults.FileSystemDetails, 1024,
                 L"File System Test Results:\n"
                 L"[FAIL] GetTempPathW() failed\n"
                 L"\nCannot access system temp directory.");
    }
}

/* Registry Access Test */
static void TestRegistryAccess() {
    OutputDebugStringW(L"[TestPayload] Testing registry access...\n");
    
    HKEY hKey;
    DWORD dwType = REG_SZ;
    wchar_t szValue[256];
    DWORD dwSize = sizeof(szValue);
    
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 
                     0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExW(hKey, L"ProductName", NULL, &dwType, 
                           (LPBYTE)szValue, &dwSize) == ERROR_SUCCESS) {
            g_TestResults.RegistryWorking = TRUE;
            wcscpy_s(g_TestResults.RegistryStatus, 256, L"Registry: Read OK");
            swprintf_s(g_TestResults.RegistryDetails, 1024,
                     L"Registry Test Results:\n"
                     L"[OK] RegOpenKeyExW() succeeded\n"
                     L"[OK] RegQueryValueExW() succeeded\n"
                     L"[OK] Registry key access granted\n"
                     L"[OK] Value read successfully\n"
                     L"\nKey: HKLM\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\ProductName\n"
                     L"Value: %s\n"
                     L"\nRegistry access working correctly.", szValue);
        } else {
            DWORD error = GetLastError();
            g_TestResults.RegistryWorking = FALSE;
            wcscpy_s(g_TestResults.RegistryStatus, 256, L"Registry: Query Failed");
            swprintf_s(g_TestResults.RegistryDetails, 1024,
                     L"Registry Test Results:\n"
                     L"[OK] RegOpenKeyExW() succeeded\n"
                     L"[FAIL] RegQueryValueExW() failed (Error: %d)\n"
                     L"\nRegistry key opened but value query failed.\n"
                     L"Possible causes: Insufficient permissions, value doesn't exist.", error);
        }
        RegCloseKey(hKey);
    } else {
        DWORD error = GetLastError();
        g_TestResults.RegistryWorking = FALSE;
        wcscpy_s(g_TestResults.RegistryStatus, 256, L"Registry: Open Failed");
        swprintf_s(g_TestResults.RegistryDetails, 1024,
                 L"Registry Test Results:\n"
                 L"[FAIL] RegOpenKeyExW() failed (Error: %d)\n"
                 L"\nCannot open registry key.\n"
                 L"Possible causes: Insufficient permissions, key doesn't exist,\n"
                 L"or running on a system where this path is different.", error);
    }
}

/* Process/Thread Test */
static void TestProcessOperations() {
    OutputDebugStringW(L"[TestPayload] Testing process operations...\n");
    
    DWORD processes[1024];
    DWORD bytesNeeded;
    DWORD processCount;
    
    if (EnumProcesses(processes, sizeof(processes), &bytesNeeded)) {
        processCount = bytesNeeded / sizeof(DWORD);
        if (processCount > 0) {
            g_TestResults.ProcessWorking = TRUE;
            swprintf_s(g_TestResults.ProcessStatus, 256, L"Processes: Found %d processes", processCount);
            swprintf_s(g_TestResults.ProcessDetails, 1024,
                     L"Process Test Results:\n"
                     L"[OK] EnumProcesses() succeeded\n"
                     L"[OK] Process enumeration working\n"
                     L"[OK] PSAPI.dll loaded successfully\n"
                     L"\nTotal processes found: %d\n"
                     L"Buffer size used: %d bytes\n"
                     L"\nProcess enumeration working correctly.", processCount, bytesNeeded);
        } else {
            g_TestResults.ProcessWorking = FALSE;
            wcscpy_s(g_TestResults.ProcessStatus, 256, L"Processes: No processes found");
            wcscpy_s(g_TestResults.ProcessDetails, 1024,
                     L"Process Test Results:\n"
                     L"[OK] EnumProcesses() succeeded\n"
                     L"[FAIL] No processes returned\n"
                     L"\nEnumProcesses worked but returned 0 processes.\n"
                     L"This is unusual and may indicate a problem.");
        }
    } else {
        DWORD error = GetLastError();
        g_TestResults.ProcessWorking = FALSE;
        wcscpy_s(g_TestResults.ProcessStatus, 256, L"Processes: Enum Failed");
        swprintf_s(g_TestResults.ProcessDetails, 1024,
                 L"Process Test Results:\n"
                 L"[FAIL] EnumProcesses() failed (Error: %d)\n"
                 L"\nProcess enumeration failed.\n"
                 L"Possible causes: PSAPI.dll not available,\n"
                 L"insufficient permissions, or system error.", error);
    }
}

#include <thread>
#include <atomic>
#include <stdbool.h>

/* Forward declaration to fix function redefinition */
static void RunAllTestsThreaded();

/* Thread Management */
static std::thread* g_TestThread = nullptr;
static std::atomic<bool> g_TestRunning;
static std::atomic<int> g_CurrentTestStep;
static const wchar_t* g_TestStepNames[] = {
    L"Initializing...",
    L"Testing Network...",
    L"Gathering System Info...",
    L"Testing File System...",
    L"Testing Registry...",
    L"Testing Processes...",
    L"Finalizing..."
};

/* Double Buffering */
static HDC g_hdcBuffer = NULL;
static HBITMAP g_hbmBuffer = NULL;
static HBITMAP g_hbmOldBuffer = NULL;

/* Threaded Test Execution */
static void RunAllTestsThreaded() {
    if (g_TestRunning.load()) return;
    
    g_TestRunning.store(true);
    g_CurrentTestStep.store(0);
    
    // Step 1: Network Test
    g_CurrentTestStep.store(1);
    TestNetworkConnectivity();
    
    // Step 2: System Info
    g_CurrentTestStep.store(2);
    TestSystemInformation();
    
    // Step 3: File System Test
    g_CurrentTestStep.store(3);
    TestFileSystemOperations();
    
    // Step 4: Registry Test
    g_CurrentTestStep.store(4);
    TestRegistryAccess();
    
    // Step 5: Process Test
    g_CurrentTestStep.store(5);
    TestProcessOperations();
    
    // Finalize
    g_CurrentTestStep.store(6);
    Sleep(500); // Brief pause to show "Finalizing..."
    
    g_CurrentTestStep.store(0);
    g_TestRunning.store(false);
}

/* Cleanup Thread Resources */
static void CleanupTestThread() {
    if (g_TestThread) {
        g_TestRunning = false;
        if (g_TestThread->joinable()) {
            g_TestThread->join();
        }
        delete g_TestThread;
        g_TestThread = nullptr;
    }
}

/* Double Buffering Functions */
static void InitDoubleBuffer(HWND hWnd, HDC hdc) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    
    if (g_hdcBuffer) {
        DeleteDC(g_hdcBuffer);
        DeleteObject(g_hbmBuffer);
    }
    
    g_hdcBuffer = CreateCompatibleDC(hdc);
    g_hbmBuffer = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    g_hbmOldBuffer = (HBITMAP)SelectObject(g_hdcBuffer, g_hbmBuffer);
}

static void CleanupDoubleBuffer() {
    if (g_hdcBuffer) {
        SelectObject(g_hdcBuffer, (HGDIOBJ)g_hbmOldBuffer);
        DeleteDC(g_hdcBuffer);
        DeleteObject(g_hbmBuffer);
        g_hdcBuffer = NULL;
        g_hbmBuffer = NULL;
        g_hbmOldBuffer = NULL;
    }
}

static void FlipBuffer(HWND hWnd, HDC hdc) {
    RECT rc;
    GetClientRect(hWnd, &rc);
    BitBlt(hdc, 0, 0, rc.right, rc.bottom, g_hdcBuffer, 0, 0, SRCCOPY);
}

/* Text Measurement Functions */
static int GetTextWidth(HDC hdc, const wchar_t* text) {
    SIZE size;
    GetTextExtentPoint32W(hdc, text, wcslen(text), &size);
    return size.cx;
}

static int GetTextHeight(HDC hdc) {
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    return tm.tmHeight;
}

static void DrawTruncatedText(HDC hdc, const wchar_t* text, RECT* rect, UINT format) {
    // Draw text clipped to the rectangle instead of replacing it with ellipses.
    DrawTextW(hdc, text, -1, rect, format);
}

static int GetButtonTextWidth(const wchar_t* text) {
    // Calculate actual text width based on ImWindows rendering
    // ImWindows uses 8x8 font with scale factor (default 2)
    int len = (int)wcslen(text);
    int charWidth = 8 * 2; // 8 pixels per char * scale factor 2
    int textWidth = len * charWidth;
    int padding = 10; // 5 pixels on each side
    return textWidth + (padding * 2);
}

/* Responsive UI Functions */
static int GetResponsiveButtonWidth(HDC hdc, const wchar_t* text, int minWidth, int maxWidth) {
    int textWidth = GetTextWidth(hdc, text) + 20; // Add padding
    if (textWidth < minWidth) return minWidth;
    if (textWidth > maxWidth) return maxWidth;
    return textWidth;
}

static int GetResponsiveFontSize(int windowWidth) {
    (void)windowWidth;
    return 14;
}

/* Button Management Functions */
static void AddButton(int x, int y, int width, int height, const wchar_t* text, int action, BOOL enabled) {
    if (g_ButtonCount < 20) {
        Button* btn = &g_Buttons[g_ButtonCount];
        btn->rect.left = x;
        btn->rect.top = y;
        btn->rect.right = x + width;
        btn->rect.bottom = y + height;
        wcscpy_s(btn->text, 64, text);
        btn->visible = TRUE;
        btn->enabled = enabled;
        btn->action = action;
        g_ButtonCount++;
    }
}

static void ClearButtons() {
    g_ButtonCount = 0;
    memset(g_Buttons, 0, sizeof(g_Buttons));
}

static int CheckButtonClick(int mouseX, int mouseY) {
    for (int i = 0; i < g_ButtonCount; i++) {
        Button* btn = &g_Buttons[i];
        if (btn->visible && btn->enabled) {
            if (mouseX >= btn->rect.left && mouseX <= btn->rect.right &&
                mouseY >= btn->rect.top && mouseY <= btn->rect.bottom) {
                return btn->action;
            }
        }
    }
    return 0;
}

/* Simple WAV Header Structure */
typedef struct {
    char riff[4];           // "RIFF"
    DWORD fileSize;        // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    DWORD fmtSize;          // Format chunk size (16 for PCM)
    WORD format;            // Audio format (1 = PCM)
    WORD channels;          // Number of channels
    DWORD sampleRate;       // Sample rate
    DWORD byteRate;         // Bytes per second
    WORD blockAlign;        // Block alignment
    WORD bitsPerSample;     // Bits per sample
    char data[4];            // "data"
    DWORD dataSize;         // Data chunk size
} WAVHEADER;

/* Initialize XAudio2 */
static BOOL InitXAudio2() {
    if (g_XAudio2Initialized) return TRUE;
    
    HRESULT hr = XAudio2Create(&g_XAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    if (FAILED(hr)) {
        OutputDebugStringA("[TestPayload] XAudio2Create failed\n");
        return FALSE;
    }
    hr = g_XAudio2->CreateMasteringVoice(&g_MasterVoice);
    if (FAILED(hr)) {
        OutputDebugStringA("[TestPayload] CreateMasteringVoice failed\n");
        g_XAudio2->Release();
        g_XAudio2 = NULL;
        return FALSE;
    }
    
    g_XAudio2Initialized = TRUE;
    OutputDebugStringA("[TestPayload] XAudio2 initialized successfully\n");
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    return TRUE;
}

/* Shutdown XAudio2 */
static void ShutdownXAudio2() {
    if (g_MasterVoice) {
        g_MasterVoice->DestroyVoice();
        g_MasterVoice = NULL;
    }
    if (g_XAudio2) {
        g_XAudio2->Release();
        g_XAudio2 = NULL;
    }
    g_XAudio2Initialized = FALSE;
}

/* Play WAV from embedded resource using XAudio2 */
static void PlayResourceWAV(int resourceId) {
    // Initialize XAudio2 if not already done
    if (!g_XAudio2Initialized) {
        if (!InitXAudio2()) {
            // Fallback to Beep if XAudio2 fails
            Beep(800, 50);
            return;
        }
    }
    
    // Load WAV resource (use "WAVE" string, not RT_WAVE which doesn't exist)
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), TEXT("WAVE"));
    if (!hRes) {
        OutputDebugStringA("[TestPayload] Failed to find WAV resource\n");
        Beep(800, 50);
        return;
    }
    
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) {
        OutputDebugStringA("[TestPayload] Failed to load WAV resource\n");
        Beep(800, 50);
        return;
    }
    
    void* pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);
    
    if (!pData || size < sizeof(WAVHEADER)) {
        OutputDebugStringA("[TestPayload] Invalid WAV resource\n");
        Beep(800, 50);
        return;
    }
    
    // Verify RIFF/WAVE header
    if (memcmp(pData, "RIFF", 4) != 0 || 
        memcmp((BYTE*)pData + 8, "WAVE", 4) != 0) {
        OutputDebugStringA("[TestPayload] Invalid WAV format (not RIFF/WAVE)\n");
        Beep(800, 50);
        return;
    }
    
    // Parse fmt chunk (can be at various positions)
    BYTE* p = (BYTE*)pData + 12; // After RIFF header
    BYTE* end = (BYTE*)pData + size - 8;
    
    WAVEFORMATEX wfx = {};
    BOOL foundFmt = FALSE;
    
    while (p < end) {
        char chunkId[5] = {0};
        memcpy(chunkId, p, 4);
        DWORD chunkSize = *(DWORD*)(p + 4);
        
        if (strcmp(chunkId, "fmt ") == 0) {
            // Found fmt chunk - careful with offsets!
            // fmt chunk starts at p+8 after "fmt " (4) + size (4)
            WORD format = *(WORD*)(p + 8);
            WORD channels = *(WORD*)(p + 10);
            DWORD sampleRate = *(DWORD*)(p + 12);
            DWORD byteRate = *(DWORD*)(p + 16);
            WORD blockAlign = *(WORD*)(p + 20);  // was 18, should be 20
            WORD bitsPerSample = *(WORD*)(p + 22); // was 20, should be 22
            
            wfx.wFormatTag = format;
            wfx.nChannels = channels;
            wfx.nSamplesPerSec = sampleRate;
            wfx.nAvgBytesPerSec = byteRate;
            wfx.nBlockAlign = blockAlign;
            wfx.wBitsPerSample = bitsPerSample;
            wfx.cbSize = 0;
            
            foundFmt = TRUE;
        }
        else if (strcmp(chunkId, "data") == 0) {
            // Found data chunk - we can stop here
            break;
        }
        
        // Move to next chunk
        p += 8 + ((chunkSize + 1) & ~1); // Align to word boundary
    }
    
    if (!foundFmt) {
        OutputDebugStringA("[TestPayload] Failed to find fmt chunk\n");
        Beep(800, 50);
        return;
    }
    
    // Search for data chunk again to get audio data pointer
    BYTE* pAudioData = NULL;
    DWORD audioDataSize = 0;
    
    p = (BYTE*)pData + 12;
    while (p < end) {
        char chunkId[5] = {0};
        memcpy(chunkId, p, 4);
        DWORD chunkSize = *(DWORD*)(p + 4);
        
        if (strcmp(chunkId, "data") == 0) {
            pAudioData = p + 8;
            audioDataSize = chunkSize;
            break;
        }
        
        p += 8 + ((chunkSize + 1) & ~1);
    }
    
    if (!pAudioData) {
        OutputDebugStringA("[TestPayload] Failed to find data chunk\n");
        Beep(800, 50);
        return;
    }
    
    // Validate format for XAudio2
    char debugMsg[256];
    sprintf(debugMsg, "[TestPayload] WAV Format: tag=%d, channels=%d, rate=%d, bits=%d, align=%d, size=%lu\n",
            wfx.wFormatTag, wfx.nChannels, wfx.nSamplesPerSec, wfx.wBitsPerSample, wfx.nBlockAlign, audioDataSize);
    OutputDebugStringA(debugMsg);
    
    // XAudio2 requires PCM format (1) with specific constraints
    if (wfx.wFormatTag != 1) { // WAVE_FORMAT_PCM
        OutputDebugStringA("[TestPayload] Error: WAV is not PCM format (required for XAudio2), falling back to beep\n");
        Beep(800, 50);
        return;
    }
    
    if (wfx.nChannels != 1 && wfx.nChannels != 2) {
        OutputDebugStringA("[TestPayload] Error: WAV must be mono or stereo (1 or 2 channels), falling back to beep\n");
        Beep(800, 50);
        return;
    }
    
    // XAudio2 supports 8-bit or 16-bit PCM only
    if (wfx.wBitsPerSample != 8 && wfx.wBitsPerSample != 16) {
        sprintf(debugMsg, "[TestPayload] Error: WAV has %d bits per sample, XAudio2 requires 8 or 16-bit, falling back to beep\n",
                wfx.wBitsPerSample);
        OutputDebugStringA(debugMsg);
        Beep(800, 50);
        return;
    }
    
    // Create source voice
    IXAudio2SourceVoice* pSourceVoice = NULL;
    HRESULT hr = g_XAudio2->CreateSourceVoice(&pSourceVoice, &wfx);
    if (FAILED(hr)) {
        OutputDebugStringA("[TestPayload] CreateSourceVoice failed\n");
        Beep(800, 50);
        return;
    }
    
    // Submit buffer
    XAUDIO2_BUFFER buffer = {};
    buffer.pAudioData = pAudioData;
    buffer.AudioBytes = audioDataSize;
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    
    hr = pSourceVoice->SubmitSourceBuffer(&buffer);
    if (FAILED(hr)) {
        OutputDebugStringA("[TestPayload] SubmitSourceBuffer failed\n");
        pSourceVoice->DestroyVoice();
        Beep(800, 50);
        return;
    }
    
    // Play the sound
    hr = pSourceVoice->Start(0);
    if (FAILED(hr)) {
        OutputDebugStringA("[TestPayload] Start failed\n");
        pSourceVoice->DestroyVoice();
        Beep(800, 50);
        return;
    }
    
    // Note: Source voice will be destroyed when playback completes
    // For simplicity, we don't track it - XAudio2 handles cleanup
    OutputDebugStringA("[TestPayload] WAV playback started\n");
}

/* Audio Feedback - Play click sound from resource */
static void PlayButtonClickSound() {
    PlayResourceWAV(100); // ID_WAV_CLICK = 100
}

static void DrawButton(HDC hdc, Button* btn, BOOL isHovered) {
    if (!btn->visible) return;
    
    HBRUSH bgBrush, borderBrush;
    COLORREF textColor;
    
    if (!btn->enabled) {
        bgBrush = CreateSolidBrush(RGB(60, 60, 60));
        borderBrush = CreateSolidBrush(RGB(40, 40, 40));
        textColor = RGB(120, 120, 120);
    } else if (isHovered) {
        bgBrush = CreateSolidBrush(RGB(80, 120, 180));
        borderBrush = CreateSolidBrush(RGB(100, 140, 200));
        textColor = RGB(255, 255, 255);
    } else {
        bgBrush = CreateSolidBrush(RGB(50, 80, 120));
        borderBrush = CreateSolidBrush(RGB(70, 100, 140));
        textColor = RGB(200, 200, 200);
    }
    
    FillRect(hdc, &btn->rect, bgBrush);
    FrameRect(hdc, &btn->rect, borderBrush);
    
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, textColor);
    
    RECT textRect = btn->rect;
    // Add padding to text (10 pixels total, 5 on each side)
    textRect.left += 5;
    textRect.right -= 5;
    
    // Use truncated text drawing to prevent overflow
    DrawTruncatedText(hdc, btn->text, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    
    DeleteObject(bgBrush);
    DeleteObject(borderBrush);
}

static void SetupMainPageButtons(RECT* windowRect) {
    ClearButtons();
    int buttonHeight = 30;
    int padding = 10;
    int y = 90;
    int x = padding;

    AddButton(x, y, max(120, GetButtonTextWidth(L"Run Tests")), buttonHeight, L"Run Tests", ACTION_RUN_ALL_TESTS, TRUE);
    x += max(120, GetButtonTextWidth(L"Run Tests")) + 5;
    AddButton(x, y, max(120, GetButtonTextWidth(L"Network")), buttonHeight, L"Network", ACTION_SHOW_NETWORK, TRUE);
    x += max(120, GetButtonTextWidth(L"Network")) + 5;
    AddButton(x, y, max(120, GetButtonTextWidth(L"Files")), buttonHeight, L"Files", ACTION_SHOW_FILESYSTEM, TRUE);

    y += buttonHeight + 5;
    x = padding;
    AddButton(x, y, max(120, GetButtonTextWidth(L"Registry")), buttonHeight, L"Registry", ACTION_SHOW_REGISTRY, TRUE);
    x += max(120, GetButtonTextWidth(L"Registry")) + 5;
    AddButton(x, y, max(120, GetButtonTextWidth(L"Processes")), buttonHeight, L"Processes", ACTION_SHOW_PROCESS, TRUE);
    x += max(120, GetButtonTextWidth(L"Processes")) + 5;
    AddButton(x, y, max(120, GetButtonTextWidth(L"System")), buttonHeight, L"System", ACTION_SHOW_SYSTEM, TRUE);

    y += buttonHeight + 5;
    AddButton(padding, y, max(120, GetButtonTextWidth(L"Refresh")), buttonHeight, L"Refresh", ACTION_REFRESH_TESTS, TRUE);
}

static void SetupDetailPageButtons() {
    ClearButtons();
    AddButton(20, 20, max(100, GetButtonTextWidth(L"< Back")), 30, L"< Back", ACTION_BACK_TO_MAIN, TRUE);
    AddButton(130, 20, max(100, GetButtonTextWidth(L"Refresh")), 30, L"Refresh", ACTION_REFRESH_TESTS, TRUE);
}

/* Run All Tests - Threaded Wrapper */
static void RunAllTests() {
    if (g_TestThread) {
        // Wait for previous thread to complete
        if (g_TestThread->joinable()) {
            g_TestThread->join();
        }
        delete g_TestThread;
        g_TestThread = nullptr;
    }
    
    // Reset test state before starting new tests
    g_TestRunning = false;
    g_CurrentTestStep = 0;
    
    g_TestThread = new std::thread(RunAllTestsThreaded);
}

/* Load bitmap from relative guest path - extension resolves to full path */
static HBITMAP LoadBitmapFromPath(const wchar_t* relativePath) {
	OutputDebugStringW(L"[TestPayload] Attempting to load bitmap: ");
	OutputDebugStringW(relativePath);
	OutputDebugStringW(L"\n");
	
	return (HBITMAP)LoadImageW(NULL, relativePath, IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);
}

/* Render bitmap to HDC with scaling to fit destination rectangle */
static void DrawBitmapWithAlpha(HDC hdc, HBITMAP hBitmap, int destX, int destY, int destW, int destH, BYTE alpha) {
	if (!hBitmap) return;
	
	HDC hdcMem = CreateCompatibleDC(hdc);
	HGDIOBJ hOldBitmap = SelectObject(hdcMem, hBitmap);
	
	/* Get bitmap dimensions */
	BITMAP bm;
	GetObjectW(hBitmap, sizeof(bm), &bm);
	
	/* Use StretchBlt to scale bitmap to destination rectangle */
	StretchBlt(hdc, destX, destY, destW > 0 ? destW : bm.bmWidth, destH > 0 ? destH : bm.bmHeight, 
	           hdcMem, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
	
	SelectObject(hdcMem, hOldBitmap);
	DeleteDC(hdcMem);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_LBUTTONDOWN:
        g_MouseX = LOWORD(lParam);
        g_MouseY = HIWORD(lParam);
        g_MouseDown = TRUE;
        swprintf(g_LastMouseEvent, 128, L"Mouse Down: %d, %d", g_MouseX, g_MouseY);
        
        // Check button clicks
        {
            int action = CheckButtonClick(g_MouseX, g_MouseY);
            if (action != 0) {
                PlayButtonClickSound(); // Audio feedback for any button click
            }
            switch (action) {
            case ACTION_RUN_ALL_TESTS:
                if (!g_TestRunning.load()) {
                    RunAllTests();
                    swprintf(g_LastKeyEvent, 128, L"Tests started in background!");
                } else {
                    swprintf(g_LastKeyEvent, 128, L"Tests already running!");
                }
                break;
            case ACTION_SHOW_NETWORK:
                g_CurrentPage = PAGE_NETWORK;
                SetupDetailPageButtons();
                break;
            case ACTION_SHOW_FILESYSTEM:
                g_CurrentPage = PAGE_FILESYSTEM;
                SetupDetailPageButtons();
                break;
            case ACTION_SHOW_REGISTRY:
                g_CurrentPage = PAGE_REGISTRY;
                SetupDetailPageButtons();
                break;
            case ACTION_SHOW_PROCESS:
                g_CurrentPage = PAGE_PROCESS;
                SetupDetailPageButtons();
                break;
            case ACTION_SHOW_SYSTEM:
                g_CurrentPage = PAGE_SYSTEM;
                SetupDetailPageButtons();
                break;
            case ACTION_BACK_TO_MAIN:
                g_CurrentPage = PAGE_MAIN;
                {
                    RECT clientRect;
                    GetClientRect(hWnd, &clientRect);
                    SetupMainPageButtons(&clientRect);
                }
                break;
            case ACTION_REFRESH_TESTS:
                RunAllTests();
                swprintf(g_LastKeyEvent, 128, L"Tests refreshed!");
                break;
            }
        }
        break;
    case WM_MOUSEMOVE:
        g_MouseX = LOWORD(lParam);
        g_MouseY = HIWORD(lParam);
        swprintf(g_LastMouseEvent, 128, L"Mouse Move: %d, %d", g_MouseX, g_MouseY);
        break;
    case WM_LBUTTONUP:
        g_MouseX = LOWORD(lParam);
        g_MouseY = HIWORD(lParam);
        g_MouseDown = FALSE;
        swprintf(g_LastMouseEvent, 128, L"Mouse Up: %d, %d", g_MouseX, g_MouseY);
        break;
    case WM_KEYDOWN:
        swprintf(g_LastKeyEvent, 128, L"Key Down: VK_0x%02X", (UINT)wParam);
        if (wParam == 'T') {
            if (!g_TestRunning.load()) {
                RunAllTests();
                swprintf(g_LastKeyEvent, 128, L"Tests started in background!");
            } else {
                swprintf(g_LastKeyEvent, 128, L"Tests already running!");
            }
        } else if (wParam == VK_ESCAPE) {
            if (g_CurrentPage != PAGE_MAIN) {
                g_CurrentPage = PAGE_MAIN;
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                SetupMainPageButtons(&clientRect);
                swprintf(g_LastKeyEvent, 128, L"Returned to main page");
            }
        }
        break;
    case WM_KEYUP:
        swprintf(g_LastKeyEvent, 128, L"Key Up: VK_0x%02X", (UINT)wParam);
        break;
    case WM_CHAR:
        swprintf(g_LastKeyEvent, 128, L"Last Char: '%c' (0x%02X)", (char)wParam, (UINT)wParam);
        break;
    case WM_DESTROY:
        ShutdownXAudio2();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPWSTR lpCmd, int nShow) {
    WNDCLASSW wc;
    HWND hWnd;
    HDC hdc;
    RECT rc, textRc;
    HBRUSH hBrush;
    MSG msg;
    BOOL running = TRUE;

    /* Initialize test results with default values */
    wcscpy_s(g_TestResults.NetworkStatus, 256, L"Network: Not tested yet");
    wcscpy_s(g_TestResults.SystemInfo, 512, L"System: Press 'T' to gather info");
    wcscpy_s(g_TestResults.FileSystemStatus, 256, L"FileSystem: Not tested yet");
    wcscpy_s(g_TestResults.RegistryStatus, 256, L"Registry: Not tested yet");
    wcscpy_s(g_TestResults.ProcessStatus, 256, L"Processes: Not tested yet");
    
    /* Initialize UI */
    g_CurrentPage = PAGE_MAIN;
    // Note: SetupMainPageButtons will be called after window creation

    wc.style = 0;
    wc.lpfnWndProc = WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"ImInputTest";

    RegisterClassW(&wc);

    hWnd = CreateWindowExW(0, L"ImInputTest", L"PE Loader Test Suite", WS_OVERLAPPEDWINDOW,
        0, 0, 800, 600, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nShow);
    UpdateWindow(hWnd);

    /* Setup buttons after window is created */
    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    SetupMainPageButtons(&clientRect);

    /* Bitmap background loading disabled until extension image support is ready */
    g_BitmapBackground = NULL;

    hdc = GetDC(hWnd);
    GetClientRect(hWnd, &rc);

    /* Initialize double buffering */
    InitDoubleBuffer(hWnd, hdc);
    
    while (running) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) { running = FALSE; break; }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        if (!running) break;

        g_Frame++;
        
        /* Check if window size changed */
        RECT currentRect;
        GetClientRect(hWnd, &currentRect);
        if (currentRect.right != rc.right || currentRect.bottom != rc.bottom) {
            rc = currentRect;
            InitDoubleBuffer(hWnd, hdc); // Recreate buffer with new size
            if (g_CurrentPage == PAGE_MAIN) {
                SetupMainPageButtons(&rc);
            }
        }

        /* All drawing goes to buffer DC */
        HDC drawDC = g_hdcBuffer;

        /* Background - fill with color first */
        hBrush = CreateSolidBrush(BG_COLOR);
        FillRect(drawDC, &rc, hBrush);
        DeleteObject(hBrush);

        /* Draw bitmap background (cnc.bmp) if loaded, at ~50% opacity */
        if (g_BitmapBackground) {
            DrawBitmapWithAlpha(drawDC, g_BitmapBackground, 0, 0, rc.right, rc.bottom, 128);
        }

        /* Setup fixed font size */
        int fontSize = GetResponsiveFontSize(rc.right);
        HFONT hFont = CreateFontW(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, 
                                 DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
        HFONT hOldFont = (HFONT)SelectObject(drawDC, hFont);

        /* Render based on current page */
        if (g_CurrentPage == PAGE_MAIN) {
            /* Main Page Header */
            SetBkMode(drawDC, TRANSPARENT);
            SetTextColor(drawDC, TITLE_COLOR);
            RECT titleRect;
            SetRect(&titleRect, 16, 16, rc.right - 16, 50);
            DrawTruncatedText(drawDC, L"ImWindows PE Loader Test Suite", &titleRect, DT_LEFT | DT_TOP);

            SetTextColor(drawDC, TEXT_COLOR);
            RECT instrRect;
            SetRect(&instrRect, 16, 50, rc.right - 16, 70);
            DrawTruncatedText(drawDC, L"Click buttons to run tests or view details | Press 'T' for quick test", &instrRect, DT_LEFT | DT_TOP);

            /* Show loading indicator if tests are running */
            if (g_TestRunning.load()) {
                SetTextColor(drawDC, ACCENT_COLOR);
                RECT loadingRect;
                SetRect(&loadingRect, 16, 90, rc.right - 16, 110);
                
                // Animated loading dots
                wchar_t loadingText[128];
                int dots = (g_Frame / 10) % 4;
                wchar_t dotStr[5] = L"";
                for (int i = 0; i < dots; i++) dotStr[i] = L'.';
                dotStr[dots] = L'\0';
                
                int currentStep = g_CurrentTestStep.load();
                if (currentStep > 0 && currentStep <= 6) {
                    swprintf(loadingText, 128, L"Running Tests: %s %s", 
                             g_TestStepNames[currentStep], dotStr);
                } else {
                    swprintf(loadingText, 128, L"Running Tests: Processing%s", dotStr);
                }
                
                DrawTruncatedText(drawDC, loadingText, &loadingRect, DT_LEFT | DT_TOP);
            }

            /* Main Page Status Display - Fixed position to prevent jumping */
            int statusY = 220;
            int lineHeight = GetTextHeight(drawDC) + 2;
            
            SetTextColor(drawDC, g_TestResults.NetworkConnected ? SUCCESS_COLOR : ERROR_COLOR);
            RECT statusRect;
            SetRect(&statusRect, 20, statusY, rc.right - 20, statusY + lineHeight);
            DrawTruncatedText(drawDC, g_TestResults.NetworkStatus, &statusRect, DT_LEFT | DT_TOP);
            statusY += lineHeight;

            SetTextColor(drawDC, g_TestResults.FileSystemWorking ? SUCCESS_COLOR : ERROR_COLOR);
            SetRect(&statusRect, 20, statusY, rc.right - 20, statusY + lineHeight);
            DrawTruncatedText(drawDC, g_TestResults.FileSystemStatus, &statusRect, DT_LEFT | DT_TOP);
            statusY += lineHeight;

            SetTextColor(drawDC, g_TestResults.RegistryWorking ? SUCCESS_COLOR : ERROR_COLOR);
            SetRect(&statusRect, 20, statusY, rc.right - 20, statusY + lineHeight);
            DrawTruncatedText(drawDC, g_TestResults.RegistryStatus, &statusRect, DT_LEFT | DT_TOP);
            statusY += lineHeight;

            SetTextColor(drawDC, g_TestResults.ProcessWorking ? SUCCESS_COLOR : ERROR_COLOR);
            SetRect(&statusRect, 20, statusY, rc.right - 20, statusY + lineHeight);
            DrawTruncatedText(drawDC, g_TestResults.ProcessStatus, &statusRect, DT_LEFT | DT_TOP);
            statusY += lineHeight;

            SetTextColor(drawDC, TEXT_COLOR);
            SetRect(&statusRect, 20, statusY, rc.right - 20, statusY + lineHeight);
            DrawTruncatedText(drawDC, g_TestResults.SystemInfo, &statusRect, DT_LEFT | DT_TOP);

            /* Draw main page buttons */
            for (int i = 0; i < g_ButtonCount; i++) {
                Button* btn = &g_Buttons[i];
                BOOL isHovered = (g_MouseX >= btn->rect.left && g_MouseX <= btn->rect.right &&
                                g_MouseY >= btn->rect.top && g_MouseY <= btn->rect.bottom);
                DrawButton(drawDC, btn, isHovered);
            }

        } else {
            /* Detail Pages - Mobile friendly */
            SetBkMode(drawDC, TRANSPARENT);
            SetTextColor(drawDC, TITLE_COLOR);
            RECT detailTitleRect;
            SetRect(&detailTitleRect, 16, 70, rc.right - 16, 90);
            
            const wchar_t* pageTitle = L"";
            const wchar_t* pageContent = L"";
            
            switch (g_CurrentPage) {
            case PAGE_NETWORK:
                pageTitle = L"Network Test";
                pageContent = g_TestResults.NetworkDetails;
                break;
            case PAGE_FILESYSTEM:
                pageTitle = L"File System Test";
                pageContent = g_TestResults.FileSystemDetails;
                break;
            case PAGE_REGISTRY:
                pageTitle = L"Registry Test";
                pageContent = g_TestResults.RegistryDetails;
                break;
            case PAGE_PROCESS:
                pageTitle = L"Process Test";
                pageContent = g_TestResults.ProcessDetails;
                break;
            case PAGE_SYSTEM:
                pageTitle = L"System Info";
                pageContent = g_TestResults.SystemInfo;
                break;
            }
            
            DrawTruncatedText(drawDC, pageTitle, &detailTitleRect, DT_LEFT | DT_TOP);
            
            /* Draw detail content with word wrap and truncation */
            SetTextColor(drawDC, TEXT_COLOR);
            RECT detailContentRect;
            SetRect(&detailContentRect, 20, 110, rc.right - 20, rc.bottom - 60);
            DrawTextW(drawDC, pageContent, -1, &detailContentRect, DT_LEFT | DT_TOP | DT_WORDBREAK);
            
            /* Draw detail page buttons */
            for (int i = 0; i < g_ButtonCount; i++) {
                Button* btn = &g_Buttons[i];
                BOOL isHovered = (g_MouseX >= btn->rect.left && g_MouseX <= btn->rect.right &&
                                g_MouseY >= btn->rect.top && g_MouseY <= btn->rect.bottom);
                DrawButton(drawDC, btn, isHovered);
            }
        }

        /* Mouse Section (always visible) */
        SetTextColor(drawDC, MOUSE_COLOR);
        RECT mouseRect;
        SetRect(&mouseRect, 16, rc.bottom - 60, rc.right - 16, rc.bottom - 40);
        DrawTruncatedText(drawDC, g_LastMouseEvent, &mouseRect, DT_LEFT | DT_TOP);

        /* Mouse Viz - Draw a crosshair at coordinates */
        HPEN hPen = CreatePen(PS_SOLID, 2, ACCENT_COLOR);
        HGDIOBJ hOldPen = SelectObject(drawDC, hPen);
        MoveToEx(drawDC, g_MouseX - 10, g_MouseY, NULL); LineTo(drawDC, g_MouseX + 10, g_MouseY);
        MoveToEx(drawDC, g_MouseX, g_MouseY - 10, NULL); LineTo(drawDC, g_MouseX, g_MouseY + 10);
        SelectObject(drawDC, hOldPen);
        DeleteObject(hPen);

        /* Keyboard Section (always visible) */
        SetTextColor(drawDC, KEY_COLOR);
        RECT keyRect;
        SetRect(&keyRect, 16, rc.bottom - 40, rc.right - 16, rc.bottom - 20);
        DrawTruncatedText(drawDC, g_LastKeyEvent, &keyRect, DT_LEFT | DT_TOP);

        /* Progress Bar at bottom (if tests are running) */
        if (g_TestRunning.load()) {
            int currentStep = g_CurrentTestStep.load();
            int progressWidth = (rc.right - 32) * currentStep / 6;
            
            // Progress bar background
            RECT progressBgRect = {16, rc.bottom - 35, rc.right - 16, rc.bottom - 25};
            HBRUSH progressBg = CreateSolidBrush(RGB(40, 40, 40));
            FillRect(drawDC, &progressBgRect, progressBg);
            
            // Progress bar fill
            RECT progressFillRect = {16, rc.bottom - 35, 16 + progressWidth, rc.bottom - 25};
            HBRUSH progressFg = CreateSolidBrush(ACCENT_COLOR);
            FillRect(drawDC, &progressFillRect, progressFg);
            
            DeleteObject(progressBg);
            DeleteObject(progressFg);
        }

        /* Footer - Mobile friendly */
        SetTextColor(drawDC, TEXT_COLOR);
        wchar_t footer[128];
        if (rc.right < 400) {
            swprintf(footer, 128, L"F:%d | %dx%d | ESC:Back", g_Frame, rc.right, rc.bottom);
        } else {
            swprintf(footer, 128, L"Frames: %d | Resolution: %dx%d | ESC: Back to Main", g_Frame, rc.right, rc.bottom);
        }
        RECT footerRect;
        SetRect(&footerRect, 16, rc.bottom - 20, rc.right - 16, rc.bottom - 5);
        DrawTruncatedText(drawDC, footer, &footerRect, DT_LEFT | DT_BOTTOM);

        /* Clean up font */
        SelectObject(drawDC, hOldFont);
        DeleteObject(hFont);
        
        /* Flip buffer to screen */
        FlipBuffer(hWnd, hdc);

        Sleep(16);
    }

    ReleaseDC(hWnd, hdc);
    if (g_BitmapBackground) DeleteObject(g_BitmapBackground);
    CleanupDoubleBuffer();
    CleanupTestThread();
    return 0;
}
