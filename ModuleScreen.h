#ifndef MODULE_SCREEN_H
#define MODULE_SCREEN_H

#include <windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// --- Mock các hàm đã viết ở phần trước để dễ hình dung tích hợp ---
extern void SetIndicatorState(int state); // 0: OFF, 1: FLASHING, 2: SOLID
class ConsentManager {
public:
    static void CountdownPrompt(const std::string& msg, int timeout, std::function<void()> onAcc, std::function<void()> onRej);
};

class ModuleScreen {
private:
    ULONG_PTR gdiplusToken;
    std::atomic<bool> isStreaming;
    std::thread streamThread;

    // Helper: Lấy CLSID của bộ mã hóa (ví dụ: image/jpeg)
    int GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

    // Lõi: Chụp, Resize, Nén JPEG và ghi vào RAM
    std::vector<uint8_t> CaptureAndCompressToMemory(int targetW, int targetH, long quality);

    // Vòng lặp stream
    void StreamLoop(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback);

public:
    ModuleScreen();
    ~ModuleScreen();

    // Chụp ảnh 1 lần (Screenshot)
    void HandleScreenshot(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback);

    // Bật/Tắt Live Screen (Có xin quyền)
    void HandleLiveStream(const std::string& action, std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback);
};

#endif // MODULE_SCREEN_H