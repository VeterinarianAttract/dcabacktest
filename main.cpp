// language: C++17, file: main.cpp, app: dcabacktest — DCA vs lump-sum backtester
// Windows 11, MSVC, WinAPI + GDI. No external dependencies, fully offline.
// Input: budget, years, seed. Runs both strategies on a deterministic seeded
// price walk and draws both equity curves in the window.
#include <windows.h>
#include <vector>
#include <cstdio>

static HWND g_budget, g_years, g_seed, g_canvas, g_verdict;
static std::vector<double> g_dcaCurve, g_lumpCurve;

// Deterministic LCG walk — same seed, same series, reproducible.
class PriceWalk {
public:
    explicit PriceWalk(uint32_t seed, double base) : state_(seed ? seed : 1), base_(base) {}
    double Next() {
        state_ = state_ * 1664525u + 1013904223u;
        double r = (state_ >> 8) / 16777216.0;
        base_ *= (1.0 + (r - 0.5) * 0.02);
        return base_;
    }
private:
    uint32_t state_;
    double base_;
};

static double GetDouble(HWND h) {
    wchar_t buf[64]; GetWindowTextW(h, buf, 64);
    return _wtof(buf);
}

static void RunBacktest() {
    double budget = GetDouble(g_budget);
    int years = (int)GetDouble(g_years);
    uint32_t seed = (uint32_t)(int)GetDouble(g_seed);
    if (budget <= 0 || years <= 0) return;
    int steps = years * 365;

    g_dcaCurve.clear(); g_lumpCurve.clear();
    // lump sum: all at step 0
    {
        PriceWalk walk(seed, 61240.0);
        double p0 = walk.Next();
        double units = budget / p0;
        for (int i = 0; i < steps; ++i)
            g_lumpCurve.push_back(units * walk.Next());
    }
    // DCA weekly
    {
        PriceWalk walk(seed, 61240.0);
        double perBuy = budget / (steps / 7.0);
        double units = 0, spent = 0;
        for (int i = 0; i < steps; ++i) {
            double p = walk.Next();
            if (i % 7 == 0 && spent < budget) { units += perBuy / p; spent += perBuy; }
            g_dcaCurve.push_back(units * p);
        }
    }
    double dcaF = g_dcaCurve.back(), lumpF = g_lumpCurve.back();
    wchar_t buf[160];
    swprintf(buf, 160, L"lump-sum: $%.0f   DCA weekly: $%.0f   winner: %s",
             lumpF, dcaF, dcaF > lumpF ? L"DCA" : L"lump-sum");
    SetWindowTextW(g_verdict, buf);
    InvalidateRect(g_canvas, nullptr, TRUE);
}

static LRESULT CALLBACK WndProc(HWND w, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        HFONT f = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0, 0, 0, 0, 0, 0, 0, 0, L"Segoe UI");
        auto label = [&](const wchar_t* t, int x, int y) {
            HWND h = CreateWindowW(L"STATIC", t, WS_CHILD | WS_VISIBLE, x, y, 120, 22, w, nullptr, nullptr, nullptr);
            SendMessageW(h, WM_SETFONT, (WPARAM)f, TRUE);
        };
        auto input = [&](HWND& s, const wchar_t* d, int x, int y) {
            s = CreateWindowW(L"EDIT", d, WS_CHILD | WS_VISIBLE | WS_BORDER, x, y, 90, 26, w, nullptr, nullptr, nullptr);
            SendMessageW(s, WM_SETFONT, (WPARAM)f, TRUE);
        };
        label(L"Budget $", 20, 22);       input(g_budget, L"12000", 150, 20);
        label(L"Years", 260, 22);         input(g_years, L"5", 330, 20);
        label(L"Seed", 440, 22);          input(g_seed, L"42", 510, 20);
        CreateWindowW(L"BUTTON", L"Run backtest", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      620, 18, 140, 32, w, (HMENU)1, nullptr, nullptr);
        g_verdict = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 20, 58, 740, 24, w, nullptr, nullptr, nullptr);
        SendMessageW(g_verdict, WM_SETFONT, (WPARAM)f, TRUE);
        g_canvas = w; // draw directly on the window client area below controls
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == 1) RunBacktest();
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(w, &ps);
        RECT rc; GetClientRect(w, &rc);
        int x0 = 30, y0 = 100, x1 = rc.right - 30, y1 = rc.bottom - 40;
        SelectObject(dc, GetStockObject(DC_PEN));
        SetDCPenColor(dc, RGB(139, 148, 158));
        MoveToEx(dc, x0, y1, nullptr); LineTo(dc, x1, y1); LineTo(dc, x0, y1); LineTo(dc, x0, y0);
        if (!g_dcaCurve.empty()) {
            double maxV = 0;
            for (double v : g_dcaCurve) if (v > maxV) maxV = v;
            auto draw = [&](const std::vector<double>& c, COLORREF col) {
                HPEN pen = CreatePen(PS_SOLID, 2, col);
                SelectObject(dc, pen);
                for (size_t i = 1; i < c.size(); ++i) {
                    int xa = x0 + (int)((i - 1) / (double)(c.size() - 1) * (x1 - x0));
                    int xb = x0 + (int)(i / (double)(c.size() - 1) * (x1 - x0));
                    int ya = y1 - (int)(c[i - 1] / maxV * (y1 - y0));
                    int yb = y1 - (int)(c[i] / maxV * (y1 - y0));
                    MoveToEx(dc, xa, ya, nullptr); LineTo(dc, xb, yb);
                }
                DeleteObject(pen);
            };
            draw(g_lumpCurve, RGB(139, 148, 158));  // dashed-ish grey
            draw(g_dcaCurve, RGB(63, 185, 80));     // green DCA
        }
        SetTextColor(dc, RGB(139, 148, 158)); SetBkMode(dc, TRANSPARENT);
        TextOutW(dc, x0, y1 + 8, L"grey: lump-sum   green: DCA weekly", 34);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(w, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, LPWSTR, int show) {
    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc; wc.hInstance = inst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(13, 17, 23));
    wc.lpszClassName = L"DcaBacktestWnd";
    RegisterClassW(&wc);
    HWND w = CreateWindowExW(0, L"DcaBacktestWnd", L"DcaBacktest — DCA vs lump-sum (offline)",
                             WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 520,
                             nullptr, nullptr, inst, nullptr);
    ShowWindow(w, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}
