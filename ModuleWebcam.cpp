#include "ModuleWebcam.h"
#include <iostream>
#include <chrono>

ModuleWebcam::ModuleWebcam() : isStreaming(false) {}

ModuleWebcam::~ModuleWebcam() {
    isStreaming = false;
    if (streamThread.joinable()) {
        streamThread.join();
    }
}

// ---------------------------------------------------------
// Vòng lặp lấy khung hình Webcam, Nén JPEG và Gửi đi
// ---------------------------------------------------------
void ModuleWebcam::StreamLoop(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback) {
    // Mở Camera mặc định (ID = 0)
    cv::VideoCapture cap(0);
    
    if (!cap.isOpened()) {
        json errorMsg = {{"module", "webcam"}, {"status", "error"}, {"message", "Không tìm thấy hoặc không thể mở Webcam!"}};
        sendCallback(errorMsg, {});
        isStreaming = false;
        SetIndicatorState(0);
        return;
    }

    // Yêu cầu phần cứng Camera quay ở 1280x720 (Nếu phần cứng hỗ trợ)
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);

    SetIndicatorState(1); // Bật đèn ĐỎ NHẤP NHÁY cảnh báo đang quay lén/webcam

    cv::Mat frame;
    uint64_t frameIndex = 0;
    
    // Cấu hình thông số nén JPEG (Quality = 60)
    std::vector<int> encodeParams = {cv::IMWRITE_JPEG_QUALITY, 60};
    std::vector<uint8_t> buffer;

    while (isStreaming) {
        auto start = std::chrono::high_resolution_clock::now();

        // 1. Đọc frame từ Webcam
        cap >> frame;
        if (frame.empty()) {
            // Camera bị ngắt kết nối đột ngột
            break; 
        }

        // 2. Ép kích thước về chuẩn 1280x720 (Đề phòng phần cứng không tuân thủ CAP_PROP ở trên)
        if (frame.cols != 1280 || frame.rows != 720) {
            cv::resize(frame, frame, cv::Size(1280, 720));
        }

        // 3. Nén ảnh thành JPEG trong RAM
        cv::imencode(".jpg", frame, buffer, encodeParams);

        // 4. Đóng gói Metadata JSON
        json metadata = {
            {"module", "webcam"},
            {"action", "stream"},
            {"payload", {
                {"frame_index", frameIndex++},
                {"timestamp", std::time(nullptr)},
                {"size_bytes", buffer.size()}
            }}
        };

        // 5. Gửi qua Callback (Gửi JSON trước, mảng Byte sau)
        sendCallback(metadata, buffer);

        // 6. Căn chỉnh Delay để đạt 24 FPS (~41.6ms mỗi frame)
        auto end = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        if (durationMs < 41) {
            std::this_thread::sleep_for(std::chrono::milliseconds(41 - durationMs));
        }
    }

    // Dọn dẹp khi kết thúc
    cap.release();
    SetIndicatorState(0); // Tắt đèn cảnh báo
}

// ---------------------------------------------------------
// Xử lý Lệnh (Bắt buộc hỏi quyền trước khi cấp luồng)
// ---------------------------------------------------------
void ModuleWebcam::HandleWebcamCommand(const std::string& action, std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback) {
    std::lock_guard<std::mutex> lock(streamMutex); // Đảm bảo thread-safe

    if (action == "stop") {
        if (isStreaming) {
            isStreaming = false;
            // Đợi luồng cũ kết thúc hoàn toàn
            if (streamThread.joinable()) {
                streamThread.join();
            }
        }
        return;
    }

    if (action == "start") {
        if (isStreaming) return; // Đang chạy rồi thì bỏ qua

        // Gửi báo cáo cho Server biết để hiện Loading
        json pendingMsg = {{"module", "webcam"}, {"status", "waiting_user_consent"}};
        sendCallback(pendingMsg, {});

        // Gọi hàm hiển thị Pop-up xin quyền
        ConsentManager::AskPermission(
            "Quản trị viên muốn mở Webcam. Đồng ý?",
            15, // Đợi tối đa 15 giây
            
            // User ĐỒNG Ý
            [this, sendCallback]() {
                json acceptMsg = {{"module", "webcam"}, {"status", "accepted"}};
                sendCallback(acceptMsg, {});

                // Khởi động luồng Webcam
                this->isStreaming = true;
                this->streamThread = std::thread(&ModuleWebcam::StreamLoop, this, sendCallback);
            },
            
            // User TỪ CHỐI (hoặc quá giờ)
            [sendCallback]() {
                json rejectMsg = {{"module", "webcam"}, {"status", "rejected"}};
                sendCallback(rejectMsg, {});
            }
        );
    }
}