#include <windows.h>
#include <nlohmann/json.hpp>
#include <shellapi.h>
#include <thread>
#include <atomic>
#include <string>

// Sử dụng atomic để luồng chính có thể ra hiệu cho luồng mạng dừng lại an toàn
std::atomic<bool> isRunning(true);
#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_APP_ICON 1001

using json = nlohmann::json;

// ---------------------------------------------------------
// [Luồng chạy ngầm] Xử lý Network & Socket
// ---------------------------------------------------------
void NetworkWorker() {
    // Lưu ý: Vì ứng dụng WinMain không có Console mặc định, std::cout sẽ không hiển thị.
    // Dùng OutputDebugStringA để in log. Bạn có thể xem log này bằng phần mềm DebugView của Microsoft.
    OutputDebugStringA("[Network] Luồng mạng đã khởi động.\n");

    while (isRunning) {
        // Mô phỏng việc nhận JSON từ Server
        OutputDebugStringA("[Network] Đang chờ lệnh từ Server...\n");
        std::string mockReceivedPayload = R"({
          "agent_id": "PC-001",
          "module": "processes",
          "action": "kill",
          "payload": { "process_id": 1024 }
        })";
        /*
        Gói JSON mẫu bạn định nghĩa:
        {
          "agent_id": "PC-001",
          "module": "processes",
          "action": "kill",
          "payload": { "process_id": 1024 }
        }
        */
        try {
            json j = json::parse(mockReceivedPayload);
            
            // Đọc các trường chuẩn
            std::string agent_id = j.value("agent_id", "");
            std::string module = j.value("module", "");
            std::string action = j.value("action", "");
            
            // Định tuyến hành động dựa trên module
            if (module == "processes") {
                if (action == "kill") {
                    int process_id = j["payload"]["process_id"];
                    std::string logMsg = "[Network] Thuc thi module: processes, action: kill, PID: " + std::to_string(process_id) + "\n";
                    OutputDebugStringA(logMsg.c_str());
                    // Gọi hàm thực tế: KillProcessByID(process_id);
                }
            }
            // Thêm các else if (module == "webcam") v.v... ở đây
            
        } catch (json::parse_error& e) {
            std::string errMsg = std::string("[Network] Lỗi parse JSON: ") + e.what() + "\n";
            OutputDebugStringA(errMsg.c_str());
        }
        Sleep(5000); // Giả lập delay chờ gói tin để không tốn CPU
    }

    OutputDebugStringA("[Network] Luồng mạng đã dừng an toàn.\n");
}

// ---------------------------------------------------------
// [Luồng chính] Xử lý sự kiện (Message Loop) cho UI ngầm
// ---------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            // Khi cửa sổ ẩn bị đóng (ví dụ lúc tắt máy hoặc người dùng chọn Exit từ Tray Icon)
            isRunning = false;      // Báo cho luồng mạng dừng lại
            PostQuitMessage(0);     // Thoát Message Loop
            break;
            
        case WM_TRAYICON: {
            // lParam chứa loại sự kiện chuột
            if (lParam == WM_RBUTTONDOWN) {
                OutputDebugStringA("[UI] Nguoi dung nhan chuot phai vao Tray Icon.\n");
                // Thường ở đây sẽ gọi TrackPopupMenu() để hiện menu thoát
            }
            else if (lParam == WM_LBUTTONDBLCLK) {
                OutputDebugStringA("[UI] Nguoi dung nhan dup chuot vao Tray Icon.\n");
                MessageBoxA(NULL, "Remote Agent đang chạy ngầm bảo vệ hệ thống.", "Thông tin", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Entry point cho Windows Desktop Application (Không mở Console)
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    // 1. Đăng ký một lớp cửa sổ (Window Class)
    const char CLASS_NAME[] = "RemoteAgentHiddenWindowClass";
    WNDCLASS wc = { };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = CLASS_NAME;

    if (!RegisterClass(&wc)) {
        return -1;
    }

    // 2. Tạo một cửa sổ ẨN (Không truyền vào cờ WS_VISIBLE và không gọi ShowWindow)
    // Cửa sổ này dùng để duy trì Message Loop và là nơi neo giữ System Tray Icon.
    HWND hwnd = CreateWindowEx(
        0,                              // Tùy chọn kiểu mở rộng
        CLASS_NAME,                     // Tên lớp cửa sổ
        "Remote Agent Hidden Window",   // Tiêu đề (không quan trọng vì bị ẩn)
        WS_OVERLAPPEDWINDOW,            // Kiểu cửa sổ chuẩn
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL,                           // Không có cửa sổ cha
        NULL,                           // Không có Menu
        hInstance,                      // Instance hiện tại
        NULL                            // Không truyền thêm dữ liệu
    );

    if (hwnd == NULL) {
        return -1;
    }

    // [MINH BẠCH - BẢO MẬT]
    // Sử dụng hàm Shell_NotifyIcon() tại đây, truyền vào 'hwnd' để tạo một biểu tượng ở thanh Taskbar cho người dùng biết Agent đang chạy.
    NOTIFYICONDATA nid = {};
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;                           // Neo vào cửa sổ ẩn của chúng ta
    nid.uID = ID_TRAY_APP_ICON;                // ID của icon
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; 
    nid.uCallbackMessage = WM_TRAYICON;        // Message sẽ gửi về WndProc khi click
    nid.hIcon = LoadIcon(NULL, IDI_SHIELD);    // Dùng icon khiên bảo mật có sẵn của hệ thống
    strcpy_s(nid.szTip, "Remote Agent Đang Hoạt Động"); // Text hiện khi hover chuột

    Shell_NotifyIcon(NIM_ADD, &nid);           // Đăng ký icon lên System Tray
    // 3. Khởi chạy luồng Mạng (Network Thread) song song
    std::thread networkThread(NetworkWorker);

    // 4. Vòng lặp sự kiện chuẩn (Message Loop) ở luồng chính
    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // 5. Dọn dẹp tài nguyên trước khi thoát
    isRunning = false;
    
    // Đợi luồng mạng kết thúc hoàn toàn mới đóng ứng dụng
    // (Trong thực tế, bạn có thể cần dùng shutdown() trên socket để ngắt thread đang bị block ở hàm recv)
    if (networkThread.joinable()) {
        networkThread.join(); 
    }

    return (int) msg.wParam;
}