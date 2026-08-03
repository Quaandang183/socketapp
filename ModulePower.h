#ifndef MODULE_POWER_H
#define MODULE_POWER_H

#include <windows.h>
#include <string>
#include <nlohmann/json.hpp>
#include <functional>

using json = nlohmann::json;

// --- Mock class ConsentManager để tích hợp ---
// (Bạn có thể điều chỉnh lại theo class thực tế của bạn)
class ConsentManager {
public:
    // Hiển thị Pop-up đếm ngược. 
    // Nếu user bấm Cancel -> Gọi onCancel.
    // Nếu user bấm OK hoặc Hết giờ (Timeout) -> Gọi onAccept.
    static void CountdownPrompt(const std::string& message, int timeoutSeconds, 
                                std::function<void()> onAccept, 
                                std::function<void()> onCancel);
};

// --- Lớp quản lý Power ---
class ModulePower {
private:
    // Hàm phụ trợ xin quyền hệ thống để Shutdown/Restart
    bool EnableShutdownPrivilege();

    // Các hàm thực thi cốt lõi
    void ExecuteShutdown();
    void ExecuteRestart();
    void ExecuteSleep();

public:
    ModulePower() = default;

    // Hàm nhận lệnh từ Router và xử lý logic đếm ngược
    void HandlePowerCommand(const std::string& action, std::function<void(const std::string&)> sendResponseCallback);
};

#endif // MODULE_POWER_H