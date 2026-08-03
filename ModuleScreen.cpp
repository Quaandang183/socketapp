#include "ModuleScreen.h"
#include <iostream>
#include <chrono>
#include <objbase.h>

using namespace Gdiplus;

// ---------------------------------------------------------
// Khởi tạo và Dọn dẹp GDI+
// ---------------------------------------------------------
ModuleScreen::ModuleScreen() : isStreaming(false) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
}

ModuleScreen::~ModuleScreen() {
    isStreaming = false;
    if (streamThread.joinable()) {
        streamThread.join();
    }
    GdiplusShutdown(gdiplusToken);
}

// ---------------------------------------------------------
// Helper: Tìm Encoder (vd: "image/jpeg")
// ---------------------------------------------------------
int ModuleScreen::GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;
    
    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    GetImageEncoders(num, size, pImageCodecInfo);
    
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }
    free(pImageCodecInfo);
    return -1;
}

// ---------------------------------------------------------
// Lõi: Chụp, Resize và Nén vào RAM
// ---------------------------------------------------------
std::vector<uint8_t> ModuleScreen::CaptureAndCompressToMemory(int targetW, int targetH, long quality) {
    // 1. Chụp màn hình bằng Win32 API (Nhanh nhất)
    HWND hDesktop = GetDesktopWindow();
    HDC hScreenDC = GetDC(hDesktop);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, screenW, screenH);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
    
    BitBlt(hMemoryDC, 0, 0, screenW, screenH, hScreenDC, 0, 0, SRCCOPY);
    
    // 2. Chuyển sang GDI+ để xử lý Resize & Nén
    Bitmap originalBitmap(hBitmap, NULL);
    
    // Resize (Vẽ lại ảnh kích thước 1280x720)
    Bitmap resizedBitmap(targetW, targetH, PixelFormat24bppRGB);
    Graphics graphics(&resizedBitmap);
    // Tùy chọn: Đặt chất lượng nội suy (Interpolation) thấp để render nhanh hơn
    graphics.SetInterpolationMode(InterpolationModeBilinear); 
    graphics.DrawImage(&originalBitmap, Rect(0, 0, targetW, targetH));

    // 3. Chuẩn bị luồng bộ nhớ (IStream)
    IStream* pStream = NULL;
    CreateStreamOnHGlobal(NULL, TRUE, &pStream);

    // 4. Thiết lập Encoder Parameters (Quality = 60)
    CLSID jpegClsid;
    GetEncoderClsid(L"image/jpeg", &jpegClsid);

    EncoderParameters encoderParams;
    encoderParams.Count = 1;
    encoderParams.Parameter[0].Guid = EncoderQuality;
    encoderParams.Parameter[0].Type = EncoderParameterValueTypeLong;
    encoderParams.Parameter[0].NumberOfValues = 1;
    encoderParams.Parameter[0].Value = &quality;

    // 5. Lưu ảnh JPEG vào pStream
    resizedBitmap.Save(pStream, &jpegClsid, &encoderParams);

    // 6. Trích xuất mảng Byte từ pStream
    STATSTG statstg;
    pStream->Stat(&statstg, STATFLAG_NONAME);
    size_t streamSize = statstg.cbSize.QuadPart;

    std::vector<uint8_t> buffer(streamSize);
    
    LARGE_INTEGER liZero = {};
    pStream->Seek(liZero, STREAM_SEEK_SET, NULL); // Đưa con trỏ stream về đầu
    
    ULONG bytesRead = 0;
    pStream->Read(buffer.data(), (ULONG)streamSize, &bytesRead);

    // 7. Dọn dẹp tài nguyên
    pStream->Release();
    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(hDesktop, hScreenDC);

    return buffer;
}

// ---------------------------------------------------------
// Xử lý Chụp ảnh tĩnh (Screenshot)
// ---------------------------------------------------------
void ModuleScreen::HandleScreenshot(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback) {
    SetIndicatorState(2); // Hiện đèn ĐỎ ĐỨNG YÊN (Solid)

    auto imgData = CaptureAndCompressToMemory(1280, 720, 80); // Screenshot cho chất lượng cao hơn (80%)

    json metadata = {
        {"module", "screen"},
        {"action", "screenshot_result"},
        {"payload", {
            {"timestamp", std::time(nullptr)},
            {"size_bytes", imgData.size()}
        }}
    };

    sendCallback(metadata, imgData);
    
    SetIndicatorState(0); // Tắt đèn
}

// ---------------------------------------------------------
// Vòng lặp Live Stream (24 FPS)
// ---------------------------------------------------------
void ModuleScreen::StreamLoop(std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback) {
    uint64_t frameIndex = 0;
    
    SetIndicatorState(1); // Hiện đèn ĐỎ NHẤP NHÁY (Flashing)

    while (isStreaming) {
        auto start = std::chrono::high_resolution_clock::now();

        // Chất lượng 60%, độ phân giải 1280x720 để tối ưu băng thông
        auto imgData = CaptureAndCompressToMemory(1280, 720, 60);

        json metadata = {
            {"module", "screen"},
            {"action", "stream"},
            {"payload", {
                {"frame_index", frameIndex++},
                {"timestamp", std::time(nullptr)},
                {"size_bytes", imgData.size()}
            }}
        };

        sendCallback(metadata, imgData);

        // Tính toán thời gian đã tốn để render frame này
        auto end = std::chrono::high_resolution_clock::now();
        auto durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // 24 FPS = ~41.6ms mỗi frame. Trừ đi thời gian render, phần còn lại thì Sleep
        if (durationMs < 41) {
            std::this_thread::sleep_for(std::chrono::milliseconds(41 - durationMs));
        }
    }

    SetIndicatorState(0); // Tắt đèn khi dừng Stream
}

// ---------------------------------------------------------
// Xử lý Start/Stop Stream (Tích hợp xin quyền)
// ---------------------------------------------------------
void ModuleScreen::HandleLiveStream(const std::string& action, std::function<void(const json&, const std::vector<uint8_t>&)> sendCallback) {
    if (action == "stop") {
        isStreaming = false;
        if (streamThread.joinable()) {
            streamThread.join();
        }
        return;
    }

    if (action == "start") {
        if (isStreaming) return; // Đang chạy rồi thì bỏ qua

        // Gửi thông báo chờ về Server
        json pendingMsg = {{"module", "screen"}, {"status", "waiting_user_consent"}};
        sendCallback(pendingMsg, {});

        // Hiển thị Pop-up (Bắt buộc phải có người ngồi máy đồng ý)
        ConsentManager::CountdownPrompt(
            "Quản trị viên muốn truyền trực tiếp màn hình của bạn. Chấp nhận?",
            15,
            
            // onAccept callback
            [this, sendCallback]() {
                json acceptMsg = {{"module", "screen"}, {"status", "accepted"}};
                sendCallback(acceptMsg, {});

                this->isStreaming = true;
                this->streamThread = std::thread(&ModuleScreen::StreamLoop, this, sendCallback);
            },
            
            // onReject callback
            [sendCallback]() {
                json rejectMsg = {{"module", "screen"}, {"status", "rejected"}};
                sendCallback(rejectMsg, {});
            }
        );
    }
}