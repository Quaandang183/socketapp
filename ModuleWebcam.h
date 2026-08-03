#ifndef MODULE_WEBCAM_H
#define MODULE_WEBCAM_H

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <nlohmann/json.hpp>
#include <opencv2/opencv.hpp>

using json = nlohmann::json;

// --- Mock các hàm hệ thống (Kế thừa từ bài trước) ---
extern void SetIndicatorState(int state); // 0: OFF, 1: FLASHING, 2: SOLID
class ConsentManager {
public:
    static void AskPermission(const std::string& msg, int timeout, std::function<void()> onAcc, std::function<void()> onRej);
};
// ----------------------------------------------------

class ModuleWebcam {
private:
    std::atomic<bool> isStreaming;
    std::thread streamThread;
    std::mutex streamMutex; // Mutex đảm bảo thread-safe khi start/stop

    // Vòng lặp stream Webcam chạy trên luồng riêng
    void StreamLoop(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback);

public:
    ModuleWebcam();
    ~ModuleWebcam();

    // Hàm nhận lệnh từ Router
    void HandleWebcamCommand(const std::string& action, std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback);
};

#endif // MODULE_WEBCAM_H