#ifndef MODULE_PROCESS_H
#define MODULE_PROCESS_H

#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class ModuleProcess {
private:
    std::vector<std::string> whitelist;

    // Cấu trúc lưu trữ thời gian của lần lấy mẫu trước đó để tính Delta CPU%
    struct ProcessCpuRecord {
        ULONGLONG lastSystemTime;
        ULONGLONG lastProcessTime;
    };
    std::unordered_map<DWORD, ProcessCpuRecord> cpuRecords;

    // Các hàm Helpers nội bộ
    static std::string ToLowerCase(const std::string& str);
    bool IsWhitelisted(const std::string& processName);
    
    // Helper chuyển đổi FILETIME của Win32 sang số nguyên 64-bit
    static ULONGLONG FileTimeToULarge(const FILETIME& ft);

public:
    ModuleProcess(); 

    // Đọc danh sách Process được phép theo dõi (Whitelist)
    void SetWhitelist(const std::vector<std::string>& allowedProcesses);

    // Quét toàn bộ Process, tính RAM/CPU và trả về JSON chuẩn
    json ListProcesses();
};

#endif // MODULE_PROCESS_H