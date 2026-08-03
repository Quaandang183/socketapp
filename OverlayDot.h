#ifndef PRIVACY_INDICATOR_H
#define PRIVACY_INDICATOR_H
#include <windows.h>

// --- Định nghĩa ID ---
#define ID_BLINK_TIMER 201

// --- Định nghĩa các trạng thái của đèn ---
enum IndicatorState {
    STATE_OFF = 0,
    STATE_FLASHING = 1, // Dùng cho Webcam, Live Screen, và Keylogger
    STATE_SOLID = 2     // Dùng cho Screenshot
};

class PrivacyIndicator {
public:
    // Khởi tạo cửa sổ cảnh báo (gọi 1 lần khi khởi động Agent)
    PrivacyIndicator(HINSTANCE hInstance);
    
    // Hủy và dọn dẹp cửa sổ
    ~PrivacyIndicator();

    // Thay đổi trạng thái đèn báo
    void SetState(IndicatorState newState);

private:
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    IndicatorState m_currentState;
    bool m_isRedVisible;
    
    const char* CLASS_NAME = "PrivacyIndicatorClass";

    // Cầu nối tĩnh (Static Window Procedure) cho Win32 API
    static LRESULT CALLBACK StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    
    // Hàm xử lý sự kiện thực tế của đối tượng
    LRESULT HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
};

#endif // PRIVACY_INDICATOR_H