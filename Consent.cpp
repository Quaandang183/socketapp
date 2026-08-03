#include "Consent.h"
#include <windows.h>
#include <nlohmann/json.hpp>
#include <string>
#include <iostream>

using json = nlohmann::json;

ConsentDialog::ConsentDialog(HINSTANCE hInstance, const std::string& agentId)
    : m_hInstance(hInstance), m_agentId(agentId), m_hwndPopup(NULL), m_hwndLabel(NULL), m_countdown(15) {
    
    // Đăng ký Window Class
    WNDCLASS wc = { };
    wc.lpfnWndProc   = ConsentDialog::StaticPopupWndProc; // Trỏ tới hàm tĩnh
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);

    RegisterClass(&wc);
}

ConsentDialog::~ConsentDialog() {
    if (m_hwndPopup) {
        DestroyWindow(m_hwndPopup);
    }
}

void ConsentDialog::Show(const std::string& moduleName) {
    if (m_hwndPopup != NULL) return; // Nếu đang có popup khác thì không mở thêm

    m_currentModule = moduleName;
    m_countdown = 15; // Reset thời gian

    // Tính toán để hiển thị ở giữa màn hình
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int windowW = 320;
    int windowH = 170;

    // Tạo cửa sổ và truyền con trỏ 'this' qua tham số cuối cùng (lpParam)
    m_hwndPopup = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
        CLASS_NAME,
        "Yêu cầu quyền truy cập!",
        WS_POPUPWINDOW | WS_CAPTION, 
        (screenW - windowW) / 2, (screenH - windowH) / 2, windowW, windowH,
        NULL, NULL, m_hInstance, this
    );

    if (m_hwndPopup) {
        ShowWindow(m_hwndPopup, SW_SHOW);
        UpdateWindow(m_hwndPopup);
    }
}

void ConsentDialog::SendResponseToServer(const std::string& status) {
    json j;
    
    j["agent_id"] = m_agentId;
    j["module"] = m_currentModule; 
    j["action"] = "consent_response";
    j["payload"]["status"] = status;

    std::string jsonStr = j.dump(); 
    OutputDebugStringA(("[Socket Send] " + jsonStr + "\n").c_str());
    // Gửi jsonStr qua WebSocket/TCP Socket ở đây...
}

// Cầu nối tĩnh (Static Window Procedure)
LRESULT CALLBACK ConsentDialog::StaticPopupWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    ConsentDialog* pThis = nullptr;

    if (message == WM_NCCREATE) {
        // Trích xuất con trỏ 'this' được truyền từ CreateWindowEx
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<ConsentDialog*>(pCreate->lpCreateParams);
        // Lưu con trỏ 'this' vào bộ nhớ của cửa sổ
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        
        pThis->m_hwndPopup = hWnd; // Lưu hwnd vào instance
    } else {
        // Lấy lại con trỏ 'this' từ bộ nhớ cửa sổ cho các message sau
        pThis = reinterpret_cast<ConsentDialog*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        // Gọi hàm xử lý thực tế của đối tượng
        return pThis->HandleMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Hàm xử lý sự kiện (đã trở thành non-static)
LRESULT ConsentDialog::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            m_hwndLabel = CreateWindow("STATIC", "Web App đang yêu cầu truy cập.\nTự động từ chối sau: 15s",
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                20, 20, 260, 40, hWnd, NULL, NULL, NULL);

            CreateWindow("BUTTON", "Đồng ý",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                30, 80, 100, 30, hWnd, (HMENU)ID_BTN_ACCEPT, NULL, NULL);

            CreateWindow("BUTTON", "Từ chối",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                160, 80, 100, 30, hWnd, (HMENU)ID_BTN_REJECT, NULL, NULL);

            SetTimer(hWnd, ID_TIMER, 1000, NULL);
            break;
        }

        case WM_TIMER: {
            m_countdown--;
            if (m_countdown > 0) {
                std::string text = "Web App đang yêu cầu truy cập.\nTự động từ chối sau: " + std::to_string(m_countdown) + "s";
                SetWindowText(m_hwndLabel, text.c_str());
            } else {
                KillTimer(hWnd, ID_TIMER);
                SendResponseToServer("timeout"); 
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == ID_BTN_ACCEPT) {
                KillTimer(hWnd, ID_TIMER);
                SendResponseToServer("accepted"); 
                DestroyWindow(hWnd);
            } 
            else if (wmId == ID_BTN_REJECT) {
                KillTimer(hWnd, ID_TIMER);
                SendResponseToServer("rejected"); 
                DestroyWindow(hWnd);
            }
            break;
        }

        case WM_DESTROY:
            m_hwndPopup = NULL; 
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}