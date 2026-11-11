/* Test-only Windows publication failure after the original moves to its backup. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdlib.h>

static BOOL WINAPI fail_replacement(LPCWSTR original, LPCWSTR replacement, LPCWSTR backup,
                                    DWORD flags, LPVOID reserved1, LPVOID reserved2)
{
    (void)replacement;
    (void)flags;
    (void)reserved1;
    (void)reserved2;
    if (!MoveFileExW(original, backup, MOVEFILE_WRITE_THROUGH))
        return FALSE;
    SetLastError(ERROR_UNABLE_TO_MOVE_REPLACEMENT_2);
    return FALSE;
}

static BOOL WINAPI restore_backup(LPCWSTR backup, LPCWSTR original, DWORD flags)
{
    if (getenv("SAROO_CDG_FAIL_ROLLBACK")) {
        SetLastError(ERROR_SHARING_VIOLATION);
        return FALSE;
    }
    return MoveFileExW(backup, original, flags);
}

#define ReplaceFileW fail_replacement
#define MoveFileExW restore_backup
#include "../tools/cdgtools/cdgfix.c"
