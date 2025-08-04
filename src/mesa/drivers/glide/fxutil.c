#ifdef _WIN32
#if defined(FX)

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "fxutil.h"

/* ToDo, test - Make sure to link with advapi32.lib (or add -ladvapi32 with MinGW) */

/* Detect if running on Win9x/ME */
int is_win9x() {
    OSVERSIONINFO verInfo;
    ZeroMemory(&verInfo, sizeof(verInfo));
    verInfo.dwOSVersionInfoSize = sizeof(verInfo);
    if (GetVersionEx(&verInfo)) {
        return (verInfo.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) ? 1 : 0;
    }
    return 0; /* Default: assume NT-based (Win2000/XP+) */
}

/* Reads an environment variable first, then checks registry */
char *fxGetRegistryOrEnvironmentString(const char *name) {
    static char value[256];
    char regPath[256];
    HKEY hKey;
    DWORD dwType, dwSize;

    /* Check environment variable first */
    if (GetEnvironmentVariable(name, value, sizeof(value)) > 0) {
        return value;
    }

    /* Base registry path depends on OS */
    if (is_win9x()) {
        strcpy(regPath, "System\\CurrentControlSet\\Services\\Class\\Display\\0000\\GLIDE");
    } else {
        strcpy(regPath, "SYSTEM\\CurrentControlSet\\Services\\3dfxvs\\Device0\\GLIDE");
    }

    /* Query registry */
    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        dwSize = sizeof(value);
        if (RegQueryValueEx(hKey, name, NULL, &dwType, (LPBYTE)value, &dwSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return value;
        }
        RegCloseKey(hKey);
    }

    /* Try alternate device/display keys */
    int i; /* C89 compatible*/
    for (i = 1; i < 10; i++) {
        if (is_win9x()) {
            sprintf(regPath, "System\\CurrentControlSet\\Services\\Class\\Display\\%04d\\GLIDE", i);
        } else {
            sprintf(regPath, "SYSTEM\\CurrentControlSet\\Services\\3dfxvs\\Device%d\\GLIDE", i);
        }

        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            dwSize = sizeof(value);
            if (RegQueryValueEx(hKey, name, NULL, &dwType, (LPBYTE)value, &dwSize) == ERROR_SUCCESS) {
                RegCloseKey(hKey);
                return value;
            }
            RegCloseKey(hKey);
        }
    }

    return NULL; /* Not found */
}
#endif
#endif