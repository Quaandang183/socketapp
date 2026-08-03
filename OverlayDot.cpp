#include "OverlayDot.h"
#include <iostream>

PrivacyIndicator::PrivacyIndicator(HINSTANCE hInstance) 
    : m_hInstance(hInstance), m_hwnd(NULL), m_currentState(STATE_OFF), m_isRedVisible(false) {
    
    WNDCLASS wc = { };
    wc.lpfnWndProc   = PrivacyIndicator::StaticWndProc; // Trỏ tới hàm tĩnh
    wc.hInstance     = m_hInstance;
    wc.lpszClassName = CLASS_NAME;
    
    RegisterClass(&wc);

    // Kích thước cửa sổ
    int width = 20;
    int height = 20;
    
    // Đặt ở góc trên bên phải màn hình, cách lề 30px
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int posX = screenW - width - 30;
    int posY = 30;

    // Các cờ cấu hình quan trọng để cửa sổ nổi, trong suốt, ẩn khỏi taskbar và click xuyên qua
    m_hwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
        CLASS_NAME, "Privacy Indicator",
        WS_POPUP, // Không viền, không tiêu đề
        posX, posY, width, height,
        NULL, NULL, m_hInstance, this // Truyền con trỏ 'this' vào lParam
    );
}

PrivacyIndicator::~PrivacyIndicator() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
    }
}

void PrivacyIndicator::SetState(IndicatorState newState) {
    if (!m_hwnd) return;

    m_currentState = newState;

    if (newState == STATE_FLASHING) {
        m_isRedVisible = true;
        SetTimer(m_hwnd, ID_BLINK_TIMER, 500, NULL); // Nháy mỗi 500ms
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE); // Hiện mà không cướp focus
    } 
    else if (newState == STATE_SOLID) {
        KillTimer(m_hwnd, ID_BLINK_TIMER);
        m_isRedVisible = true;
        ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
        InvalidateRect(m_hwnd, NULL, TRUE); // Ép vẽ lại ngay lập tức
    } 
    else { // STATE_OFF
        KillTimer(m_hwnd, ID_BLINK_TIMER);
        ShowWindow(m_hwnd, SW_HIDE);
    }
}

LRESULT CALLBACK PrivacyIndicator::StaticWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    PrivacyIndicator* pThis = nullptr;

    if (message == WM_NCCREATE) {
        // Trích xuất con trỏ 'this' từ CreateWindowEx
        CREATESTRUCT* pCreate = reinterpret_cast<CREATESTRUCT*>(lParam);
        pThis = reinterpret_cast<PrivacyIndicator*>(pCreate->lpCreateParams);
        // Lưu con trỏ 'this' vào bộ nhớ của cửa sổ
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
        
        pThis->m_hwnd = hWnd; // Lưu hwnd vào instance
    } else {
        // Lấy lại con trỏ 'this' từ bộ nhớ cửa sổ
        pThis = reinterpret_cast<PrivacyIndicator*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    if (pThis) {
        // Gọi hàm xử lý thực tế của đối tượng
        return pThis->HandleMessage(hWnd, message, wParam, lParam);
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

LRESULT PrivacyIndicator::HandleMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            // Thiết lập màu nền là Magenta (Hồng cánh sen) và dùng nó làm "Color Key" (màu sẽ bị làm trong suốt)
            SetLayeredWindowAttributes(hWnd, RGB(255, 0, 255), 0, LWA_COLORKEY);
            break;
        }

        case WM_TIMER: {
            if (wParam == ID_BLINK_TIMER) {
                // Đảo trạng thái hiển thị màu đỏ để tạo hiệu ứng nhấp nháy
                m_isRedVisible = !m_isRedVisible;
                // Yêu cầu Windows vẽ lại cửa sổ
                InvalidateRect(hWnd, NULL, TRUE);
            }
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            // 1. Tô toàn bộ nền bằng màu Magenta (sẽ trở nên trong suốt nhờ LWA_COLORKEY)
            HBRUSH bgBrush = CreateSolidBrush(RGB(255, 0, 255));
            FillRect(hdc, &ps.rcPaint, bgBrush);
            DeleteObject(bgBrush);

            // 2. Vẽ hình tròn màu đỏ nếu đang ở trạng thái cần hiển thị
            if (m_currentState == STATE_SOLID || (m_currentState == STATE_FLASHING && m_isRedVisible)) {
                HBRUSH redBrush = CreateSolidBrush(RGB(255, 0, 0)); // Màu đỏ chuẩn
                SelectObject(hdc, redBrush);
                
                // Bỏ đường viền đen mặc định của hình tròn
                HPEN nullPen = (HPEN)GetStockObject(NULL_PEN);
                SelectObject(hdc, nullPen);
                
                // Vẽ hình tròn vừa khít với cửa sổ 20x20
                Ellipse(hdc, 0, 0, 20, 20);
                
                DeleteObject(redBrush);
            }

            EndPaint(hWnd, &ps);
            break;
        }

        case WM_NCHITTEST: {
            // Ép hệ điều hành coi toàn bộ cửa sổ này là TRONG SUỐT với chuột (Click-through)
            // Lệnh này dự phòng thêm cho cờ WS_EX_TRANSPARENT
            return HTTRANSPARENT;
        }

        case WM_DESTROY:
            KillTimer(hWnd, ID_BLINK_TIMER);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}