// SMCPlay.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "SMCPlay.h"

#define SMC_POLL_INTERVAL       1000

#define IOCTRL_SMC_READ_KEY     0x9C402454
#define IOCTRL_SMC_WRITE_KEY    0x9C402458

#define APP_MUTEX               _T("SMCPLAY")

HANDLE h_mac_hal;
HANDLE h_mutex;

int smc_init()
{
    h_mac_hal = CreateFile(_T("\\\\.\\MacHALDriver"), GENERIC_READ , 0, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL , NULL);

    return 0;
}

int smc_read(const char *key, unsigned char *buffer, int len)
{
    int result;

    DWORD bytesReturned = 0;

    result = DeviceIoControl(h_mac_hal, IOCTRL_SMC_READ_KEY,
        (LPVOID)key, 5,
        buffer, len,
        &bytesReturned, NULL);

    return bytesReturned;
}

void smc_write(const char *key, unsigned char *buffer, int len)
{
    int result;

    DWORD bytesReturned = 0;

    char write_buff[16];

    memcpy(write_buff, key, 5);
    memcpy(write_buff + 5, buffer, len);

    result = DeviceIoControl(h_mac_hal, IOCTRL_SMC_WRITE_KEY,
        write_buff, 5 + len,
        NULL, 0,
        &bytesReturned, NULL);
}

#define CLASS_NAME_SHELL_TRAY   _T("Shell_TrayWnd")
#define CLASS_NAME_TRAY_NOTIFY  _T("TrayNotifyWnd")
#define CLASS_NAME_TRAY_CLOCK   _T("TrayClockWClass")
#define LABEL_LENGTH            16

void smc_tray()
{
    int smc_result_len, i_buffer;
    unsigned char smc_buffer[2] = { 0 };

    const char  smc_key_power   [] = { "PDTR" },
                smc_key_cputmp  [] = { "TC0D" };

    SYSTEMTIME local_tm;

    int volt_i, temp_i;
    float volt_d, temp_d;

    HWND h_shell_tray, h_tray_notify, h_tray_clock;
    HDC hdc_tray_clock;

    RECT rc_tray_clock, rc_time, rc_watt;

    DWORD btn_color;
    HBRUSH tray_clock_brush;
    HPEN tray_clock_pen;

    TCHAR tray_buffer[LABEL_LENGTH] = { 0 };

    DWORD err = GetLastError();

    h_shell_tray    = FindWindowEx(NULL         , NULL, CLASS_NAME_SHELL_TRAY   , NULL);
    h_tray_notify   = FindWindowEx(h_shell_tray , NULL, CLASS_NAME_TRAY_NOTIFY  , NULL);
    h_tray_clock    = FindWindowEx(h_tray_notify, NULL, CLASS_NAME_TRAY_CLOCK   , NULL);

    GetClientRect(h_tray_clock, &rc_tray_clock);
    CopyRect(&rc_time, &rc_tray_clock);
    CopyRect(&rc_watt, &rc_tray_clock);

    btn_color = GetSysColor(COLOR_BTNFACE);
    tray_clock_brush = CreateSolidBrush(btn_color);
    tray_clock_pen = CreatePen(PS_SOLID, 0, btn_color);

    rc_time.bottom /= 2;
    rc_watt.top = rc_time.bottom;
    rc_time.top += 2;
    rc_watt.bottom -= 2;

    for (;;)
    {
        GetLocalTime(&local_tm);

        // read power supply:
        smc_result_len = smc_read(smc_key_power, smc_buffer, sizeof(smc_buffer));

#ifdef _DEBUG
        for (i_buffer = 0; i_buffer < smc_result_len; i_buffer ++)
        {
            printf("%.2X ", smc_buffer[i_buffer]);
        }
        printf("\r\n");
#endif // _DEBUG

        // sp78:
        volt_i = ((smc_buffer[0] << 8) + smc_buffer[1]);
        volt_d = volt_i;
        volt_d /= 256;
#ifdef _DEBUG
        printf("%f\r\n", volt_d);
#endif // _DEBUG

        // read cpu temp:
        smc_result_len = smc_read(smc_key_cputmp, smc_buffer, sizeof(smc_buffer));

        // sp78:
        temp_i = ((smc_buffer[0] << 8) + smc_buffer[1]);
        temp_d = temp_i;
        temp_d /= 256;

        hdc_tray_clock = GetDC(h_tray_clock);

        SelectObject(hdc_tray_clock, tray_clock_brush);
        SelectObject(hdc_tray_clock, tray_clock_pen);

        SelectObject(hdc_tray_clock, GetStockObject(DEFAULT_GUI_FONT));

        //SetBkMode(hdc_tray_clock, TRANSPARENT);
        SetBkColor(hdc_tray_clock, btn_color);

        swprintf(tray_buffer, _T("%02d:%02d:%02d"), local_tm.wHour, local_tm.wMinute, local_tm.wSecond);
        DrawText(hdc_tray_clock, tray_buffer, -1, &rc_time, DT_CENTER | DT_NOCLIP | DT_SINGLELINE | DT_VCENTER);

        swprintf(tray_buffer, _T("%.1fW %.1f¡ãC"), volt_d, temp_d);
        DrawText(hdc_tray_clock, tray_buffer, -1, &rc_watt, DT_CENTER | DT_NOCLIP | DT_SINGLELINE | DT_VCENTER);

        ReleaseDC(h_tray_clock, hdc_tray_clock);

        if (local_tm.wSecond == 01)
        {
            Rectangle(hdc_tray_clock, rc_tray_clock.left, rc_tray_clock.top, rc_tray_clock.right, rc_tray_clock.bottom);
        }

        Sleep(SMC_POLL_INTERVAL);
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    const TCHAR sz_app_mutex[] = APP_MUTEX;
    
    h_mutex = CreateMutex(NULL, TRUE, sz_app_mutex);
    if(GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 1;
    }

#ifdef _DEBUG
#else // _DEBUG
    HWND h_wnd = GetConsoleWindow();
    ShowWindow(h_wnd, SW_HIDE);
#endif // _DEBUG

    smc_init();
    smc_tray();

    ReleaseMutex(h_mutex);
    CloseHandle(h_mutex);

    return 0;
}
