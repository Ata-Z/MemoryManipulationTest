#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <sstream>
#include <chrono>


namespace Offsets {
    constexpr uintptr_t Children = 0x0;
    constexpr uintptr_t ChildrenEnd = 0x0;
    constexpr uintptr_t WalkSpeed = 0x0;
    constexpr uintptr_t WalkSpeedCheck = 0x0;
    constexpr uintptr_t Parent = 0x0;
    constexpr uintptr_t Name = 0x0;
    constexpr uintptr_t LocalPlayer = 0x0;
    constexpr uintptr_t FakeDataModelToDataModel = 0x0;
    constexpr uintptr_t FakeDataModelPointer = 0x0;
    constexpr uintptr_t CanCollide = 0x0;
    constexpr uintptr_t Primitive = 0x0;
}

HANDLE hProcess = nullptr;
DWORD pid = 0;

uintptr_t GetModuleBase(const wchar_t* modName, DWORD procId) {
    uintptr_t modBaseAddr = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, procId);
    if (hSnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W modEntry;
        modEntry.dwSize = sizeof(modEntry);
        if (Module32FirstW(hSnap, &modEntry)) {
            do {
                if (!_wcsicmp(modEntry.szModule, modName)) {
                    modBaseAddr = (uintptr_t)modEntry.modBaseAddr;
                    break;
                }
            } while (Module32NextW(hSnap, &modEntry));
        }
    }
    CloseHandle(hSnap);
    return modBaseAddr;
}

template <typename T>
T rpm(uintptr_t addr) {
    T value{};
    ReadProcessMemory(hProcess, (LPCVOID)addr, &value, sizeof(T), nullptr);
    return value;
}

template <typename T>
void wpm(uintptr_t addr, T value) {
    WriteProcessMemory(hProcess, (LPVOID)addr, &value, sizeof(T), nullptr);
}

std::string ReadString(uintptr_t addr, int max = 200) {
    std::string result;
    char ch;
    for (int i = 0; i < max; i++) {
        ReadProcessMemory(hProcess, (LPCVOID)(addr + i), &ch, 1, nullptr);
        if (ch == '\0') break;
        result.push_back(ch);
    }
    return result;
}

uintptr_t GetProcessIdByName(const wchar_t* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe32;
    pe32.dwSize = sizeof(pe32);
    if (Process32FirstW(hSnap, &pe32)) {
        do {
            if (!_wcsicmp(pe32.szExeFile, procName)) {
                procId = pe32.th32ProcessID;
                break;
            }
        } while (Process32NextW(hSnap, &pe32));
    }
    CloseHandle(hSnap);
    return procId;
}

struct Instance {
    uintptr_t address;
    Instance(uintptr_t addr = 0) : address(addr) {}

    std::string Name() {
        uintptr_t ptr = rpm<uintptr_t>(address + Offsets::Name);
        return (ptr ? ReadString(ptr) : "");
    }

    Instance Parent() {
        return Instance(rpm<uintptr_t>(address + Offsets::Parent));
    }

    std::vector<Instance> GetChildren() {
        std::vector<Instance> result;
        uintptr_t start = rpm<uintptr_t>(address + Offsets::Children);
        uintptr_t end = rpm<uintptr_t>(start + Offsets::ChildrenEnd);

        for (uintptr_t ptr = rpm<uintptr_t>(start); ptr != end; ptr += 0x10) {
            uintptr_t childAddr = rpm<uintptr_t>(ptr);
            result.push_back(Instance(childAddr));
        }
        return result;
    }

    Instance FindFirstChild(const std::string& target) {
        for (auto& child : GetChildren()) {
            if (child.Name() == target) return child;
        }
        return Instance(0);
    }
};

namespace rbx {
    uintptr_t DataModel;
    uintptr_t Humanoid;
    uintptr_t Workspace;
    uintptr_t CoreGui;

    uintptr_t GetDataModel(uintptr_t base) {
        uintptr_t fakeDM = rpm<uintptr_t>(base + Offsets::FakeDataModelPointer);
        return rpm<uintptr_t>(fakeDM + Offsets::FakeDataModelToDataModel);
    }

    void SetWalkspeed(uintptr_t base, float speed) {
        DataModel = GetDataModel(base);
        Instance dm(DataModel);

        Instance workspace = dm.FindFirstChild("Workspace");
        Instance players = dm.FindFirstChild("Players");

        uintptr_t localPlayer = rpm<uintptr_t>(players.address + Offsets::LocalPlayer);
        Instance plr(localPlayer);
        Instance character = workspace.FindFirstChild(plr.Name());
        Instance humanoid = character.FindFirstChild("Humanoid");

        Workspace = workspace.address;
        Humanoid = humanoid.address;

        wpm<float>(Humanoid + Offsets::WalkSpeed, speed);
        wpm<float>(Humanoid + Offsets::WalkSpeedCheck, speed);
    }
    void SetNoClip(uintptr_t base, bool enable) {
        DataModel = GetDataModel(base);
        Instance dm(DataModel);

        Instance workspace = dm.FindFirstChild("Workspace");
        Instance players = dm.FindFirstChild("Players");

        uintptr_t localPlayer = rpm<uintptr_t>(players.address + Offsets::LocalPlayer);
        Instance plr(localPlayer);
        Instance character = workspace.FindFirstChild(plr.Name());

        if (!character.address) return;  // safety check

        // Loop through all direct children of character
        for (auto& child : character.GetChildren()) {
            // Read pointer to Primitive struct
            uintptr_t Primitive = rpm<uintptr_t>(child.address + Offsets::Primitive);
            if (!Primitive) continue;  // skip if invalid

            // Read current CanCollide byte
            BYTE val = rpm<BYTE>(Primitive + Offsets::CanCollide);

            // Set or clear the 0x08 bit (bit 3)
            if (enable)
                val |= 0x08;   // enable collision
            else
                val &= ~0x08;  // disable collision

            // Write the modified byte back
            wpm<BYTE>(Primitive + Offsets::CanCollide, val);
        }
}

}

void slowPrintLines(const std::string& text, int delayMs = 200) {
    std::istringstream stream(text);
    std::string line;

    while (std::getline(stream, line)) {
        std::cout << line << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
}

int main() {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 5);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);

    pid = GetProcessIdByName(L"RobloxPlayerBeta.exe");
    if (!pid) {
        SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 4);
        std::cout << "Error 302" << std::endl;
        Sleep(3000);
        return -1;
    }

    hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProcess) {
        std::cout << "Failed to open process." << std::endl;
        return -1;
    }

    uintptr_t base = GetModuleBase(L"Redacted.exe", pid); // removed the program name

    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 2);
    std::cout << "Attached To RobloxPlayerBeta.exe" << std::endl;
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
    
    std::string Title = R"( 
 _     _  _______  ___      _______  _______  __   __  _______ 
| | _ | ||       ||   |    |       ||       ||  |_|  ||       |
| || || ||    ___||   |    |       ||   _   ||       ||    ___|
|       ||   |___ |   |    |       ||  | |  ||       ||   |___ 
|       ||    ___||   |___ |      _||  |_|  ||       ||    ___|
|   _   ||   |___ |       ||     |_ |       || ||_|| ||   |___ 
|__| |__||_______||_______||_______||_______||_|   |_||_______|
                                                                                           
                                                                                                                                                                                                                                      
)";

    slowPrintLines(Title, 300);

    while (true) {
        std::cout << "1 | Change Walkspeed" << std::endl;
        std::cout << "2 | Toggle Noclip" << std::endl;
        std::cout << "3 | Exit" << std::endl;

        int choice;
        std::cin >> choice;
        if (choice == 1) {
            std::cout << "[!] how much speed: ";
            float speed;
            std::cin >> speed;
            rbx::SetWalkspeed(base, speed);
            std::cout << "[!] DONE!" << std::endl;
        }
        else if (choice == 2) {
            std::cout << "[!] Toggle Noclip (0 for NoClip On, 1 for NoClip Off): ";
            bool collide;
            std::cin >> collide;
            std::cout << "[!] DONE!" << std::endl;
            while (true) {
            rbx::SetNoClip(base, collide);
            Sleep(100);
        }}
        
        else {
            return 0;
        }
}
return 0;
    }
