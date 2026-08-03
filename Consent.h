#ifndef CONSENT_H
#define CONSENT_H

#pragma once
#include <windows.h>
#include <string>

// --- Các ID cho các thành phần UI ---
#define ID_BTN_ACCEPT 101
#define ID_BTN_REJECT 102
#define ID_TIMER 103

class ConsentDialog {
public:
    // Khởi tạo Dialog với HINSTANCE của ứng dụng và Agent ID
    ConsentDialog(HINSTANCE hInstance, const std::string& agentId = "PC-001");
    ~ConsentDialog();

    // Hiển thị popup hỏi ý kiến truy cập
    void Show(const std::string& moduleName);

private:
    // Thông tin trạng thái
    HWND m_hwndPopup;
    HWND m_hwndLabel;
    HINSTANCE m_hInstance;
    int m_countdown;
    std::string m_currentModule;
    std::string m_agentId;

    // Tên lớp cửa sổ
    const char* CLASS_NAME = "ConsentPopupClass";

    // Hàm tĩnh làm cầu nối cho Win32 API Window Procedure
    static LRESULT CALLBACK StaticPopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Hàm xử lý sự kiện thực tế của đối tượng
    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

    // Gửi phản hồi
    void SendResponseToServer(const std::string& status);
};

#endif // CONSENT_H