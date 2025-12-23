
#define _CRT_SECURE_NO_WARNINGS

// ===== Win32 / System =====
#include <windows.h>
#include <shellapi.h>
#include <d3d11.h>
#include <tchar.h>
#include <commdlg.h>
#include <dbt.h>

// ===== C++ STL =====
#include <vector>
#include <string>
#include <fstream>
#include <thread>
#include <cstdio>
#include <cstdarg>
#include <algorithm>
// ===== ImGui =====
#include "../imgui/imgui.h"
#include "../imgui/backends/imgui_impl_win32.h"
#include "../imgui/backends/imgui_impl_dx11.h"

// ===== ZeroCrypto Core =====
#include "core/VaultRegistry.h"
#include "Crypto.h" 
#include "SecureBuffer.h"

// Fix for Drag & Drop UIPI issue
#ifndef WM_COPYGLOBALDATA
#define WM_COPYGLOBALDATA 0x0049
#endif
#ifndef MSGFLT_ADD
#define MSGFLT_ADD 1
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Link necessary libraries (Ensure build script links these too)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib") 
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")

// ================== Global Data ==================
struct AppConfig {
    bool killOnUSB = true;
    bool autoUnmount = false;
    bool wipeOnPanic = false;
    int lastActiveIndex = 0; 
};


static bool g_OpenAddPopup = false;
static bool g_OpenCreatePopup = false;
static bool g_ShowSuccess = false;

// Add new flags for windows
static bool g_ShowAddWindow = false;
static bool g_ShowCreateWindow = false;

// Direct X
static ID3D11Device* g_Device = nullptr;
static ID3D11DeviceContext* g_Context = nullptr;
static IDXGISwapChain* g_SwapChain = nullptr;
static ID3D11RenderTargetView* g_RTV = nullptr;

// Data Buffers
static char newVaultName[64] = "";
static char newVaultPath[MAX_PATH] = "";
static int  driveIndex = 0;
static const char* driveLetters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

static char createPath[MAX_PATH] = "";
static SecureBuffer g_createPassword(128);
static int  createSizeMB = 500; 

static AppConfig g_config; 
static std::vector<std::string> g_logs; 
static bool g_switchToLogs = false;     
static char g_mountedDrive = 0;
static bool g_isMounting = false;

static SecureBuffer g_vaultPassword(128);

#define HOTKEY_ID_PANIC 1001

// ================== Helper Functions ==================


void SanitizeVaults() {
    auto& vaults = VaultRegistry::All();
    if (vaults.empty()) return;

    bool changed = false;
    for (auto it = vaults.begin(); it != vaults.end(); ) {
        
        if (it->path.empty() || (it->name == "A" && it->path.length() < 3)) {
            it = vaults.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        VaultRegistry::Save(); 
    }
}


void ExtractFileName(const char* fullPath, char* dest, size_t size) {
    if (!fullPath || !dest) return;
    std::string pathStr = fullPath;
    
    size_t lastSlash = pathStr.find_last_of("\\/");
    if (lastSlash != std::string::npos) pathStr = pathStr.substr(lastSlash + 1);
    
    size_t lastDot = pathStr.find_last_of(".");
    if (lastDot != std::string::npos) pathStr = pathStr.substr(0, lastDot);
    
    strncpy(dest, pathStr.c_str(), size - 1);
    dest[size - 1] = '\0';
}

void AddLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, IM_ARRAYSIZE(buf), fmt, args);
    buf[IM_ARRAYSIZE(buf)-1] = 0;
    va_end(args);
    g_logs.push_back(std::string(buf));
}

std::string GetBaseDir() {
    char buffer[MAX_PATH];
    GetModuleFileNameA(NULL, buffer, MAX_PATH);
    std::string::size_type pos = std::string(buffer).find_last_of("\\/");
    return std::string(buffer).substr(0, pos);
}

std::string GetAbsolutePath(const std::string& relativePath) {
    char fullPath[MAX_PATH];
    if (GetFullPathNameA(relativePath.c_str(), MAX_PATH, fullPath, NULL) != 0) return std::string(fullPath);
    return relativePath;
}

bool FileExists(const std::string& name) {
    std::ifstream f(name.c_str());
    return f.good();
}

bool IsDriveMounted(char letter) {
    DWORD mask = GetLogicalDrives();
    return (mask & (1 << (toupper(letter) - 'A')));
}

DWORD ExecuteProcess(const std::string& appPath, const std::string& args, bool showWindow) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = showWindow ? SW_SHOWNORMAL : SW_HIDE; 

    std::string cmdLine = "\"" + appPath + "\" " + args;
    if (!CreateProcessA(NULL, &cmdLine[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

// ================== Persistence ==================
std::string GetConfigPath() { return GetBaseDir() + "\\config.bin"; }

void SaveConfig() {
    std::ofstream ofs(GetConfigPath(), std::ios::binary);
    if (ofs.is_open()) { ofs.write((char*)&g_config, sizeof(AppConfig)); ofs.close(); }
}

void LoadConfig() {
    std::ifstream ifs(GetConfigPath(), std::ios::binary);
    if (ifs.is_open()) { ifs.read((char*)&g_config, sizeof(AppConfig)); ifs.close(); }
}

void ResetConfig() {
    g_config.killOnUSB = true;
    g_config.autoUnmount = false;
    g_config.wipeOnPanic = false;
    g_config.lastActiveIndex = 0;
    SaveConfig();
    AddLog("[INFO] Settings reset to default.");
}

void SaveEncryptedLogs() {
    if (g_logs.empty()) { AddLog("[WARN] No logs to save."); return; }
    std::string fullLog = "";
    for (const auto& line : g_logs) fullLog += line + "\n";
    std::vector<uint8_t> rawData(fullLog.begin(), fullLog.end());
    std::vector<uint8_t> encryptedData = Encrypt(rawData);
    std::ofstream outFile("system.log.enc", std::ios::binary);
    if (outFile.is_open()) {
        outFile.write(reinterpret_cast<const char*>(encryptedData.data()), encryptedData.size());
        outFile.close();
        AddLog("[SECURE] Logs encrypted & saved to 'system.log.enc'");
    } else {
        AddLog("[ERROR] Failed to save log file!");
    }
}

// ================== Styling ==================
void SetupProfessionalStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 5.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.ItemSpacing = ImVec2(8, 6);
    style.FramePadding = ImVec2(6, 4);

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg]       = ImVec4(0.06f, 0.06f, 0.06f, 0.98f);
    colors[ImGuiCol_PopupBg]        = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_Text]           = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_Border]         = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_Header]         = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_HeaderActive]   = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_Tab]            = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TabHovered]     = ImVec4(0.00f, 0.55f, 0.55f, 0.60f);
    colors[ImGuiCol_TabActive]      = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_FrameBg]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_Button]         = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_ButtonHovered]  = ImVec4(0.00f, 0.45f, 0.45f, 1.00f);
    colors[ImGuiCol_ButtonActive]   = ImVec4(0.00f, 0.60f, 0.60f, 1.00f);
    colors[ImGuiCol_CheckMark]      = ImVec4(0.00f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TitleBgActive]  = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.7f);
}

// ================== Core Logic ==================

void SecureDeleteFile(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile != INVALID_HANDLE_VALUE) {
        DWORD size = GetFileSize(hFile, NULL);
        if (size != INVALID_FILE_SIZE) {
            std::vector<char> zeros(4096, 0);
            DWORD written;
            DWORD bytesLeft = size;
            SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
            while (bytesLeft > 0) {
                DWORD chunk = (bytesLeft < 4096) ? bytesLeft : 4096;
                WriteFile(hFile, zeros.data(), chunk, &written, NULL);
                bytesLeft -= written;
            }
        }
        FlushFileBuffers(hFile);
        CloseHandle(hFile);
    }
    DeleteFileA(path.c_str());
}

void UnmountVault() {
    AddLog("[CMD] Executing Force Dismount...");
    std::string exeDir = GetBaseDir();
    std::string vcPath = exeDir + "\\assets\\veracrypt\\VeraCrypt.exe";
    DWORD exitCode = ExecuteProcess(vcPath, "/dismount /force /quit /silent", false);
    if (exitCode != 0) AddLog("[WARN] Dismount command failed with exit code %d.", exitCode);
    
    if (g_config.wipeOnPanic) {
        AddLog("[PANIC] Wiping Configuration...");
        SecureDeleteFile(GetConfigPath());
        SecureDeleteFile("zerocrypto.cfg"); 
        AddLog("[PANIC] Terminating.");
        Sleep(500); 
        exit(0); 
    }
    AddLog("[CMD] Dismount command sent.");
}

void AttemptAutorun(char driveLetter) {
    std::string scriptPath = std::string(1, driveLetter) + ":\\SecureEnv\\StartEnv.bat";
    AddLog("[AUTORUN] Checking: %s", scriptPath.c_str());
    Sleep(1500); 
    if (FileExists(scriptPath)) {
        AddLog("[AUTORUN] Found! Executing...");
        ExecuteProcess("powershell", "-WindowStyle Hidden -Command \"Get-ChildItem -Path '" + std::string(1, driveLetter) + ":\\SecureEnv' -Recurse | Unblock-File\"", false);
        std::string workingDir = std::string(1, driveLetter) + ":\\SecureEnv";
        ShellExecuteA(NULL, "open", scriptPath.c_str(), NULL, workingDir.c_str(), SW_SHOWNORMAL);
        AddLog("[AUTORUN] Environment launched.");
    } else {
        AddLog("[AUTORUN] 'StartEnv.bat' not found.");
    }
}

void CreateNewVaultContainer() {
    g_switchToLogs = true;
    AddLog("------------------------------------------------");
    AddLog("[CREATE] Initializing Vault Creation...");
    if (createSizeMB <= 0) { AddLog("[ERROR] Invalid vault size!"); return; }
    if (strlen(createPath) == 0) { AddLog("[ERROR] No path specified!"); return; }
    std::string fmtPath = GetBaseDir() + "\\assets\\veracrypt\\VeraCrypt Format.exe";
    
    if (!FileExists(fmtPath)) { AddLog("[ERROR] 'VeraCrypt Format.exe' not found!"); return; }

    std::string args = "/create \"" + std::string(createPath) + 
                      "\" /size " + std::to_string(createSizeMB) + "M" + 
                      " /password \"" + g_createPassword.ToString() + 
                      "\" /encryption AES /hash sha-512 /filesystem exfat /silent";

    AddLog("[CMD] Executing Formatter...");
    DWORD exitCode = ExecuteProcess(fmtPath, args, true); 
    g_createPassword.Clear(); 
    bool result = (exitCode == 0);
    if (result) AddLog("[SUCCESS] Vault Created!");
    else AddLog("[FAIL] Creation failed with exit code %d.", exitCode);
}

void MountVault() {
    auto* v = VaultRegistry::GetActive();
    if (!v) { AddLog("[ERROR] No vault selected!"); return; }
    
    if (g_vaultPassword.ToString().empty()) { AddLog("[ERROR] Password is empty!"); return; }
    
    if (IsDriveMounted(v->letter)) {
        AddLog("[INFO] Drive %c: is already mounted.", v->letter);
        return;
    }
    g_switchToLogs = true; 
    g_isMounting = true;
    AddLog("[INIT] Starting Mount Process...");
    
    std::string vcPath = GetBaseDir() + "\\assets\\veracrypt\\VeraCrypt.exe";
    if (!FileExists(vcPath)) { AddLog("[ERROR] VeraCrypt.exe not found!"); g_isMounting = false; return; }
    
    std::string args = "/volume \"" + GetAbsolutePath(v->path) + "\" " +
                       "/letter " + std::string(1, v->letter) + " " +
                       "/password \"" + g_vaultPassword.ToString() + "\" " +
                       "/quit"; 

    AddLog("[CMD] Executing VeraCrypt (GUI Mode)...");
    DWORD exitCode = ExecuteProcess(vcPath, args, true);
    g_vaultPassword.Clear(); 
    g_isMounting = false;

    bool success = (exitCode == 0);
    if (!success) AddLog("[FAIL] VeraCrypt process failed with exit code %d.", exitCode);

    if (success) {
        Sleep(1000);
        if (IsDriveMounted(v->letter)) {
            AddLog("[SUCCESS] Mount operation completed.");
            g_mountedDrive = v->letter;
            g_ShowSuccess = true;
            std::thread([=](){ AttemptAutorun(v->letter); }).detach();
        } else {
            AddLog("[FAIL] Drive not found after process exit.");
        }
    } else {
        AddLog("[FAIL] Failed to execute VeraCrypt process.");
    }
}

// ================== Window Proc ==================
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    
    if (msg == WM_DROPFILES) {
        HDROP hDrop = (HDROP)wParam;
        DragQueryFileA(hDrop, 0, newVaultPath, MAX_PATH);
        DragFinish(hDrop);
        
        ExtractFileName(newVaultPath, newVaultName, sizeof(newVaultName));
        g_OpenAddPopup = true; 

        driveIndex = 0;
        AddLog("[INFO] Vault dropped: %s", newVaultPath);
        SetForegroundWindow(hWnd);
        return 0;
    }

    if (msg == WM_HOTKEY && wParam == HOTKEY_ID_PANIC) { UnmountVault(); PostQuitMessage(0); }
    
    if (msg == WM_DEVICECHANGE && wParam == DBT_DEVICEREMOVECOMPLETE) {
        auto* hdr = (DEV_BROADCAST_HDR*)lParam;
        if (hdr && hdr->dbch_devicetype == DBT_DEVTYP_VOLUME) {
            if (g_config.killOnUSB && !g_isMounting) { 
                 AddLog("[ALERT] USB Removal Detected!");
                 UnmountVault(); PostQuitMessage(0);
            }
        }
    }
    if (msg == WM_DESTROY) { UnregisterHotKey(hWnd, HOTKEY_ID_PANIC); PostQuitMessage(0); return 0; }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ================== File Pickers ==================
bool PickVaultFile(char* outPath, DWORD size, HWND owner) {
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "VeraCrypt Vault (*.hc;*.vc)\0*.hc;*.vc\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = size;
    ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    ofn.lpstrInitialDir = GetBaseDir().c_str();
    return GetOpenFileNameA(&ofn);
}

bool SaveVaultFile(char* outPath, DWORD size, HWND owner) {
    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = "VeraCrypt Vault (*.hc)\0*.hc\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = outPath;
    ofn.nMaxFile = size;
    ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = "hc";
    ofn.lpstrInitialDir = GetBaseDir().c_str();
    return GetSaveFileNameA(&ofn);
}

// ================== WinMain ==================
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    SetEnvironmentVariable(TEXT("SEE_MASK_NOZONECHECKS"), TEXT("1"));
    BOOL admin = FALSE;
    PSID group;
    SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
    AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0,0,0,0,0,0, &group);
    CheckTokenMembership(NULL, group, &admin);
    FreeSid(group);
    if (!admin) {
        char path[MAX_PATH]; GetModuleFileNameA(NULL, path, MAX_PATH);
        ShellExecuteA(NULL, "runas", path, NULL, NULL, SW_SHOW);
        return 0;
    }

    WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, hInst, LoadIcon(hInst, MAKEINTRESOURCE(101)), NULL, NULL, NULL, "ZeroCrypto", LoadIcon(hInst, MAKEINTRESOURCE(101)) };
    
    VaultRegistry::Load(); 
    SanitizeVaults(); 
    LoadConfig(); 

    RegisterClassEx(&wc);
    HWND hwnd = CreateWindowEx(WS_EX_APPWINDOW, wc.lpszClassName, "ZeroCrypto", WS_POPUP, 300, 200, 520, 360, NULL, NULL, wc.hInstance, NULL);

    
    HMODULE hUser = LoadLibraryA("user32.dll");
    if (hUser) {
        typedef BOOL(WINAPI* ChangeFilter)(UINT, DWORD);
        auto func = (ChangeFilter)GetProcAddress(hUser, "ChangeWindowMessageFilter");
        if (func) { func(WM_DROPFILES, 1); func(WM_COPYDATA, 1); func(0x0049, 1); }
    }
    DragAcceptFiles(hwnd, TRUE);

    if (!RegisterHotKey(hwnd, HOTKEY_ID_PANIC, MOD_CONTROL, VK_F12)) {
        MessageBoxA(NULL, "Failed to register Hotkey!", "Error", MB_ICONEXCLAMATION);
    }

    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA); 
    ShowWindow(hwnd, SW_SHOW);

    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, &sd, &g_SwapChain, &g_Device, NULL, &g_Context);
    ID3D11Texture2D* back;
    g_SwapChain->GetBuffer(0, IID_PPV_ARGS(&back));
    g_Device->CreateRenderTargetView(back, NULL, &g_RTV);
    back->Release();

    ImGui::CreateContext();
    SetupProfessionalStyle(); 
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_Device, g_Context);

    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessage(&msg); continue; }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        
        // ================== POPUP LOGIC ==================
        
        if (g_OpenAddPopup) {
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ImGui::OpenPopup("AddVaultPopup");
            g_OpenAddPopup = false;
        }
        if (g_OpenCreatePopup) {
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ImGui::OpenPopup("CreateVaultPopup");
            g_OpenCreatePopup = false;
        }

        // Add window logic
        if (g_ShowAddWindow) {
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ImGui::Begin("Add Vault", &g_ShowAddWindow, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("CONFIGURATION"); ImGui::Separator();
            if (ImGui::InputText("Vault Name", newVaultName, IM_ARRAYSIZE(newVaultName), ImGuiInputTextFlags_EnterReturnsTrue)) {
                 if (strlen(newVaultPath) > 0) {
                     Vault v; v.name = (strlen(newVaultName) ? newVaultName : "Vault");
                     v.path = newVaultPath; v.letter = driveLetters[driveIndex];
                     VaultRegistry::Add(v); VaultRegistry::Save(); g_ShowAddWindow = false;
                 }
            }
            ImGui::InputText("Path", newVaultPath, MAX_PATH); 
            ImGui::Combo("Mount Letter", &driveIndex, "A\0B\0C\0D\0E\0F\0G\0H\0I\0J\0K\0L\0M\0N\0O\0P\0Q\0R\0S\0T\0U\0V\0W\0X\0Y\0Z\0");
            ImGui::Spacing();
            if (ImGui::Button("CONFIRM", ImVec2(100, 0))) {
                if (strlen(newVaultPath) > 0) {
                    if (IsDriveMounted(driveLetters[driveIndex])) {
                        AddLog("[ERROR] Drive letter %c: already in use!", driveLetters[driveIndex]);
                    } else {
                        Vault v; v.name = (strlen(newVaultName) ? newVaultName : "Vault");
                        v.path = newVaultPath; v.letter = driveLetters[driveIndex];
                        VaultRegistry::Add(v); VaultRegistry::Save(); g_ShowAddWindow = false;
                    }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("CANCEL", ImVec2(100, 0))) g_ShowAddWindow = false;
            ImGui::End();
        }

        if (g_ShowCreateWindow) {
            SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ImGui::Begin("Create Vault", &g_ShowCreateWindow, ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::Text("CREATE NEW CONTAINER"); ImGui::Separator();
            ImGui::LabelText("File Path", "%s", createPath);
            ImGui::InputInt("Size (MB)", &createSizeMB, 10, 100);
            if (ImGui::InputText("Password", g_createPassword.Get(), g_createPassword.Size(), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue)) {
                CreateNewVaultContainer(); g_ShowCreateWindow = false;
            }
            ImGui::Dummy(ImVec2(0,5));
            if (ImGui::Button("CREATE & FORMAT", ImVec2(140, 0))) {
                if (createSizeMB <= 0) {
                    AddLog("[ERROR] Invalid vault size!");
                } else {
                    CreateNewVaultContainer(); g_ShowCreateWindow = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("CANCEL", ImVec2(100, 0))) g_ShowCreateWindow = false;
            ImGui::End();
        }

        auto& allVaults = VaultRegistry::All();
        for(int i=0; i < allVaults.size(); i++) {
             if(IsDriveMounted(allVaults[i].letter)) {
                 if (VaultRegistry::GetActive() != &allVaults[i]) {
                     VaultRegistry::SetActive(i);
                     g_config.lastActiveIndex = i;
                 }
                 break; 
             }
        }

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(520, 360));
        ImGui::Begin("ZeroCrypto", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && ImGui::IsMouseClicked(0)) {
            ReleaseCapture(); SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        }

        // Title Bar
        {
            ImVec2 p = ImGui::GetCursorScreenPos();
            float winWidth = ImGui::GetWindowWidth();
            ImGui::SetCursorPos(ImVec2(10, 8)); ImGui::TextDisabled("ZERO CRYPTO v7.4");
            ImGui::SameLine(); ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "[Speed Exit: CTRL+F12 ]");
            
            float btnRadius = 10.0f;
            ImVec2 btnCenter = ImVec2(p.x + winWidth - 20, p.y + 15);
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            ImGui::SetCursorPos(ImVec2(winWidth - 35, 5));
            if (ImGui::InvisibleButton("CloseBtn", ImVec2(30, 30))) {
                 if (g_config.autoUnmount) UnmountVault();
                 SaveConfig(); PostQuitMessage(0);
            }
            draw_list->AddCircleFilled(btnCenter, btnRadius, ImGui::IsItemHovered() ? IM_COL32(200, 50, 50, 255) : IM_COL32(60, 60, 60, 255));
            float xSz = 4.0f;
            draw_list->AddLine(ImVec2(btnCenter.x - xSz, btnCenter.y - xSz), ImVec2(btnCenter.x + xSz, btnCenter.y + xSz), IM_COL32(255,255,255,255), 2.0f);
            draw_list->AddLine(ImVec2(btnCenter.x + xSz, btnCenter.y - xSz), ImVec2(btnCenter.x - xSz, btnCenter.y + xSz), IM_COL32(255,255,255,255), 2.0f);
        }
        ImGui::Dummy(ImVec2(0, 10));

        if (ImGui::BeginTabBar("MainTabs")) {
            
            if (ImGui::BeginTabItem("VAULTS")) {
                ImGui::Dummy(ImVec2(0.0f, 5.0f));
                auto& vaults = VaultRegistry::All();
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "DETECTED VAULTS:");
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
                if (ImGui::BeginListBox("##vaults", ImVec2(-1, 90))) {
                    for (int i = 0; i < vaults.size(); i++) {
                        bool selected = (VaultRegistry::GetActive() == &vaults[i]);
                        if (ImGui::Selectable(vaults[i].name.c_str(), selected)) {
                            VaultRegistry::SetActive(i);
                            g_config.lastActiveIndex = i; SaveConfig();
                        }
                    }
                    ImGui::EndListBox();
                }
                ImGui::PopStyleColor();
                ImGui::Spacing();
                
                auto* activeVault = VaultRegistry::GetActive();
                if (activeVault) {
                    bool isMounted = IsDriveMounted(activeVault->letter);
                    if (isMounted) {
                        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "STATUS: ACTIVE SESSION");
                        ImGui::Text("Drive Mounted at: %c:", activeVault->letter);
                        ImGui::Dummy(ImVec2(0.0f, 10.0f));
                        if (ImGui::Button("OPEN DRIVE EXPLORER", ImVec2(-1, 40))) {
                            std::string driveRoot = std::string(1, activeVault->letter) + ":\\";
                            ShellExecuteA(NULL, "explore", driveRoot.c_str(), NULL, NULL, SW_SHOW);
                        }
                        ImGui::Spacing();
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
                        if (ImGui::Button("DISMOUNT VAULT", ImVec2(-1, 30))) UnmountVault();
                        ImGui::PopStyleColor(2);
                    } else {
                        ImGui::Text("DECRYPTION KEY:");
                        ImGui::PushItemWidth(-1);
                        if (ImGui::InputText("##password", g_vaultPassword.Get(), g_vaultPassword.Size(), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue)) {
                            MountVault();
                        }
                        ImGui::PopItemWidth();
                        ImGui::Dummy(ImVec2(0.0f, 5.0f));
                        if (ImGui::Button("INITIALIZE MOUNT", ImVec2(-1, 40))) MountVault();
                    }
                } else {
                    ImGui::TextDisabled("STATUS: WAITING FOR SELECTION...");
                    ImGui::Button("NO TARGET SELECTED", ImVec2(-1, 40)); 
                }

                ImGui::Spacing(); ImGui::Separator();
                
                // ===== FIXED BUTTON LOGIC =====
                if (ImGui::Button("ADD EXISTING", ImVec2(150, 25))) {
                    ZeroMemory(newVaultPath, MAX_PATH);
                    if (PickVaultFile(newVaultPath, MAX_PATH, hwnd)) {  
                        ExtractFileName(newVaultPath, newVaultName, sizeof(newVaultName));
                        g_ShowAddWindow = true;  
                        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("CREATE NEW VAULT", ImVec2(150, 25))) {
                    ZeroMemory(createPath, MAX_PATH);
                    if (SaveVaultFile(createPath, MAX_PATH, hwnd)) {  
                        g_ShowCreateWindow = true;  
                        SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
                    }
                }
                // ==============================

                ImGui::EndTabItem(); 
            }

            if (ImGui::BeginTabItem("KILL SWITCH")) {
                ImGui::Dummy(ImVec2(0.0f, 20.0f));
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.1f, 0.1f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.0f, 0.0f, 1.0f));
                if (ImGui::Button("EMERGENCY KILL", ImVec2(-1, 150))) { UnmountVault(); PostQuitMessage(0); }
                ImGui::PopStyleColor(3);
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("SYSTEM")) {
                ImGui::Dummy(ImVec2(0.0f, 10.0f));
                if (ImGui::Checkbox("USB Dead Man's Switch", &g_config.killOnUSB)) SaveConfig();
                ImGui::Spacing();
                if (ImGui::Checkbox("Auto-Dismount on Exit", &g_config.autoUnmount)) SaveConfig();
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(1.0f, 0.0f, 0.0f, 1.0f));
                if (ImGui::Checkbox("Panic: Wipe Config & Exit", &g_config.wipeOnPanic)) SaveConfig();
                ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("DANGER: Pressing CTRL+F12 will DELETE all settings and vaults history!");

                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.0f, 1.0f));
                if (ImGui::Button("RESET DEFAULT SETTINGS", ImVec2(-1, 30))) ResetConfig();
                ImGui::PopStyleColor();
                ImGui::EndTabItem();
            }

            ImGuiTabItemFlags logFlags = 0;
            if (g_switchToLogs) { logFlags = ImGuiTabItemFlags_SetSelected; g_switchToLogs = false; }
            if (ImGui::BeginTabItem("SYSTEM LOGS", nullptr, logFlags)) {
                if (ImGui::Button("COPY LOGS", ImVec2(100, 25))) {
                    std::string allLogs = ""; for(const auto& line : g_logs) allLogs += line + "\n";
                    ImGui::SetClipboardText(allLogs.c_str()); AddLog("[INFO] Logs copied.");
                }
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.4f, 0.0f, 1.0f));
                if (ImGui::Button("SAVE ENCRYPTED LOGS", ImVec2(180, 25))) {
                    SaveEncryptedLogs();
                }
                ImGui::PopStyleColor();

                ImGui::Separator();
                ImGui::BeginChild("LogRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
                for (const auto& log : g_logs) {
                     if (log.find("[FAIL]") != std::string::npos || log.find("[ERROR]") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", log.c_str());
                    else if (log.find("[SUCCESS]") != std::string::npos) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", log.c_str());
                    else if (log.find("[CMD]") != std::string::npos) ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", log.c_str());
                    else if (log.find("[AUTORUN]") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "%s", log.c_str());
                    else if (log.find("[SECURE]") != std::string::npos) ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "%s", log.c_str());
                    else if (log.find("[PANIC]") != std::string::npos) ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", log.c_str());
                    else ImGui::TextUnformatted(log.c_str());
                }
                if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

        if (g_ShowSuccess) { ImGui::OpenPopup("MOUNT SUCCESSFUL"); g_ShowSuccess = false; }
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("MOUNT SUCCESSFUL", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "VAULT MOUNTED SUCCESSFULLY!");
            ImGui::Text("Drive Letter: %c:", g_mountedDrive); ImGui::Separator(); ImGui::Dummy(ImVec2(0.0f, 10.0f));
            if (ImGui::Button("OPEN DRIVE EXPLORER", ImVec2(200, 40))) {
                std::string driveRoot = std::string(1, g_mountedDrive) + ":\\";
                ShellExecuteA(NULL, "explore", driveRoot.c_str(), NULL, NULL, SW_SHOW); ImGui::CloseCurrentPopup();
            }
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            if (ImGui::Button("STAY & VIEW LOGS", ImVec2(200, 40))) ImGui::CloseCurrentPopup();
            ImGui::Dummy(ImVec2(0.0f, 5.0f));
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("EXIT APPLICATION", ImVec2(200, 30))) PostQuitMessage(0);
            ImGui::PopStyleColor();
            ImGui::EndPopup();
        }
        ImGui::End();

        ImGui::Render();
        const float clear_color_with_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        g_Context->OMSetRenderTargets(1, &g_RTV, NULL);
        g_Context->ClearRenderTargetView(g_RTV, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_SwapChain->Present(1, 0);
    }
    VaultRegistry::Save();
    if (g_config.autoUnmount) UnmountVault();
    return 0;
}