#ifndef SAROO_CDGFIX_FILE_H
#define SAROO_CDGFIX_FILE_H

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <sddl.h>
#include <wchar.h>
#else
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <copyfile.h>
#endif
_Static_assert(sizeof(off_t) >= 8, "cdgfix needs 64-bit file offsets");
#endif

typedef struct {
    FILE *input;
    uint64_t size;
    int output;
    bool temporary_created, replaced;
#ifdef _WIN32
    wchar_t *path, *directory, *temporary, *backup;
    BY_HANDLE_FILE_INFORMATION original;
    bool directory_created, backup_kept, restored, path_uncertain;
#else
    char *path, *temporary;
    struct stat original;
    int parent;
#endif
} CDG_FILE;

static int write_all(int fd, const void *data, size_t size)
{
    const unsigned char *bytes = data;
    while (size) {
#ifdef _WIN32
        int count = _write(fd, bytes, (unsigned)size);
#else
        ssize_t count = write(fd, bytes, size);
#endif
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (!count)
                errno = EIO;
            return -1;
        }
        bytes += count;
        size -= (size_t)count;
    }
    return 0;
}

#ifdef _WIN32
#define CDG_FILE_INIT { .output = -1 }
#define cdg_casecmp _stricmp
#define CDG_INPUT_CHANGED EAGAIN
#define CDG_FILE_SHARE (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)

static int windows_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        errno = ENOENT;
        break;
    case ERROR_ACCESS_DENIED:
    case ERROR_WRITE_PROTECT:
    case ERROR_SHARING_VIOLATION:
        errno = EACCES;
        break;
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        errno = ENOSPC;
        break;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        errno = ENOMEM;
        break;
    default:
        errno = EIO;
        break;
    }
    return -1;
}

static int file_info(HANDLE handle, BY_HANDLE_FILE_INFORMATION *info)
{
    if (GetFileType(handle) != FILE_TYPE_DISK) {
        errno = EINVAL;
        return -1;
    }
    if (!GetFileInformationByHandle(handle, info))
        return windows_error(GetLastError());
    if (info->dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

static bool same_file(const BY_HANDLE_FILE_INFORMATION *a, const BY_HANDLE_FILE_INFORMATION *b)
{
    return a->dwVolumeSerialNumber == b->dwVolumeSerialNumber &&
           a->nFileIndexHigh == b->nFileIndexHigh && a->nFileIndexLow == b->nFileIndexLow;
}

static bool same_input(const BY_HANDLE_FILE_INFORMATION *a, const BY_HANDLE_FILE_INFORMATION *b)
{
    return same_file(a, b) && a->dwFileAttributes == b->dwFileAttributes &&
           a->nFileSizeHigh == b->nFileSizeHigh && a->nFileSizeLow == b->nFileSizeLow &&
           CompareFileTime(&a->ftCreationTime, &b->ftCreationTime) == 0 &&
           CompareFileTime(&a->ftLastWriteTime, &b->ftLastWriteTime) == 0;
}

static HANDLE input_handle(const CDG_FILE *file)
{
    return (HANDLE)_get_osfhandle(_fileno(file->input));
}

static int path_info(const wchar_t *path, DWORD access, BY_HANDLE_FILE_INFORMATION *info)
{
    HANDLE handle = CreateFileW(path, access, CDG_FILE_SHARE, NULL, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return windows_error(GetLastError());
    int result = file_info(handle, info);
    if (!CloseHandle(handle) && result == 0)
        return windows_error(GetLastError());
    return result;
}

static int open_input(CDG_FILE *file, const char *name)
{
    int length = MultiByteToWideChar(CP_ACP, 0, name, -1, NULL, 0);
    if (!length)
        return windows_error(GetLastError());
    wchar_t *wide = malloc((size_t)length * sizeof(*wide));
    if (!wide)
        return -1;
    if (!MultiByteToWideChar(CP_ACP, 0, name, -1, wide, length)) {
        DWORD error = GetLastError();
        free(wide);
        return windows_error(error);
    }

    HANDLE handle = CreateFileW(wide, GENERIC_READ, CDG_FILE_SHARE, NULL, OPEN_EXISTING,
                                FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    DWORD error = GetLastError();
    free(wide);
    if (handle == INVALID_HANDLE_VALUE)
        return windows_error(error);
    if (file_info(handle, &file->original))
        goto fail;
    file->size = (uint64_t)file->original.nFileSizeHigh << 32 | file->original.nFileSizeLow;
    if (!file->size || file->size > INT64_MAX) {
        errno = EINVAL;
        goto fail;
    }

    /* Resolve links to the open file so replacement preserves the named link. */
    DWORD needed = GetFinalPathNameByHandleW(handle, NULL, 0, FILE_NAME_NORMALIZED);
    if (!needed) {
        windows_error(GetLastError());
        goto fail;
    }
    if (needed > 32768) {
        errno = ENAMETOOLONG;
        goto fail;
    }
    file->path = malloc(needed * sizeof(*file->path));
    if (!file->path)
        goto fail;
    DWORD copied = GetFinalPathNameByHandleW(handle, file->path, needed, FILE_NAME_NORMALIZED);
    if (!copied || copied >= needed) {
        if (copied)
            errno = ENAMETOOLONG;
        else
            windows_error(GetLastError());
        goto fail;
    }
    int fd = _open_osfhandle((intptr_t)handle, _O_RDONLY | _O_BINARY | _O_NOINHERIT);
    if (fd < 0)
        goto fail;
    file->input = _fdopen(fd, "rb");
    if (!file->input) {
        int error_code = errno;
        _close(fd);
        errno = error_code;
        return -1;
    }
    return 0;

fail:;
    int error_code = errno;
    CloseHandle(handle);
    errno = error_code;
    return -1;
}

static int verify_input(CDG_FILE *file)
{
    BY_HANDLE_FILE_INFORMATION handle_info, named_info;
    if (file_info(input_handle(file), &handle_info) ||
        path_info(file->path, FILE_READ_ATTRIBUTES, &named_info))
        return -1;
    if (!same_input(&file->original, &handle_info) || !same_input(&file->original, &named_info)) {
        errno = CDG_INPUT_CHANGED;
        return -1;
    }
    return 0;
}

static wchar_t *stage_path(const wchar_t *directory, const wchar_t *leaf)
{
    size_t base = wcslen(directory), suffix = wcslen(leaf);
    wchar_t *path = malloc((base + suffix + 1) * sizeof(*path));
    if (path) {
        wmemcpy(path, directory, base);
        wmemcpy(path + base, leaf, suffix + 1);
    }
    return path;
}

static int make_staging_directory(CDG_FILE *file)
{
    const wchar_t *slash = wcsrchr(file->path, L'\\');
    if (!slash) {
        errno = EINVAL;
        return -1;
    }
    size_t prefix = (size_t)(slash - file->path) + 1;
    enum { SUFFIX_CAPACITY = 48 };
    file->directory = malloc((prefix + SUFFIX_CAPACITY) * sizeof(*file->directory));
    if (!file->directory)
        return -1;
    wmemcpy(file->directory, file->path, prefix);

    PSECURITY_DESCRIPTOR security;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;OICI;FA;;;OW)", SDDL_REVISION_1, &security, NULL))
        return windows_error(GetLastError());
    SECURITY_ATTRIBUTES attributes = {sizeof(attributes), security, FALSE};
    unsigned long serial = GetTickCount();
    DWORD error = ERROR_ALREADY_EXISTS;
    for (unsigned attempt = 0; attempt < 32; ++attempt) {
        swprintf(file->directory + prefix, SUFFIX_CAPACITY, L".saroo-cdg-%08lx-%08lx-%02x",
                 (unsigned long)GetCurrentProcessId(), serial, attempt);
        if (CreateDirectoryW(file->directory, &attributes)) {
            file->directory_created = true;
            break;
        }
        error = GetLastError();
        if (error != ERROR_ALREADY_EXISTS)
            break;
    }
    LocalFree(security);
    return file->directory_created ? 0 : windows_error(error);
}

static int begin_replacement(CDG_FILE *file, uint64_t prefix)
{
    BY_HANDLE_FILE_INFORMATION writable;
    if (path_info(file->path, FILE_WRITE_DATA | FILE_READ_ATTRIBUTES, &writable))
        return -1;
    if (!same_input(&file->original, &writable)) {
        errno = CDG_INPUT_CHANGED;
        return -1;
    }
    if (make_staging_directory(file))
        return -1;
    file->temporary = stage_path(file->directory, L"\\repair");
    file->backup = stage_path(file->directory, L"\\original");
    if (!file->temporary || !file->backup)
        return -1;
    HANDLE handle = CreateFileW(file->temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, NULL);
    if (handle == INVALID_HANDLE_VALUE)
        return windows_error(GetLastError());
    file->temporary_created = true;
    file->output = _open_osfhandle((intptr_t)handle, _O_WRONLY | _O_BINARY | _O_NOINHERIT);
    if (file->output < 0) {
        int error = errno;
        CloseHandle(handle);
        errno = error;
        return -1;
    }
    if (!prefix)
        return 0;

    /* Rewind only on the first repair after the first window. Resume at the
     * stream's logical position, including any input buffered by stdio. */
    fpos_t resume;
    if (fgetpos(file->input, &resume) || _fseeki64(file->input, 0, SEEK_SET))
        return -1;
    unsigned char buffer[8192];
    while (prefix) {
        size_t count = prefix < sizeof(buffer) ? (size_t)prefix : sizeof(buffer);
        if (fread(buffer, 1, count, file->input) != count) {
            errno = EIO;
            return -1;
        }
        if (write_all(file->output, buffer, count))
            return -1;
        prefix -= count;
    }
    return fsetpos(file->input, &resume);
}

static int publish_replacement(CDG_FILE *file)
{
    if (_commit(file->output) < 0)
        return -1;
    int closed = _close(file->output);
    file->output = -1;
    if (closed < 0)
        return -1;

    /* ReplaceFile preserves the original file's metadata and can leave the
     * old file at the backup name if publication fails partway through. */
    if (!ReplaceFileW(file->path, file->temporary, file->backup, 0, NULL, NULL)) {
        DWORD error = GetLastError();
        file->backup_kept = GetFileAttributesW(file->backup) != INVALID_FILE_ATTRIBUTES;
        BY_HANDLE_FILE_INFORMATION current;
        if (file->backup_kept && !path_info(file->backup, FILE_READ_ATTRIBUTES, &current) &&
            same_file(&file->original, &current) &&
            MoveFileExW(file->backup, file->path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            file->backup_kept = false;
            file->restored = true;
        }
        file->path_uncertain = path_info(file->path, FILE_READ_ATTRIBUTES, &current) ||
                               !same_file(&file->original, &current);
        return windows_error(error);
    }
    file->replaced = true;
    if (!DeleteFileW(file->backup)) {
        DWORD error = GetLastError();
        if (error != ERROR_FILE_NOT_FOUND) {
            file->backup_kept = true;
            return windows_error(error);
        }
    }
    if (!RemoveDirectoryW(file->directory))
        return windows_error(GetLastError());
    file->directory_created = false;
    return 0;
}

static void cleanup_file(CDG_FILE *file)
{
    if (file->input)
        fclose(file->input);
    if (file->output >= 0)
        _close(file->output);
    if (file->directory_created) {
        if (file->temporary_created && !file->replaced)
            DeleteFileW(file->temporary);
        if (!file->backup_kept)
            RemoveDirectoryW(file->directory);
    }
    free(file->backup);
    free(file->temporary);
    free(file->directory);
    free(file->path);
}

static void pause_new_console(void)
{
    DWORD pid, mode;
    if (GetConsoleProcessList(&pid, 1) == 1 && GetConsoleMode(GetStdHandle(STD_INPUT_HANDLE), &mode)) {
        fputs("Press Enter to close.\n", stderr);
        (void)getchar();
    }
}

#else
#define CDG_FILE_INIT { .output = -1, .parent = -1 }
#define cdg_casecmp strcasecmp
#define CDG_INPUT_CHANGED ESTALE

static bool same_input(const struct stat *a, const struct stat *b)
{
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_size != b->st_size ||
        a->st_mode != b->st_mode || a->st_uid != b->st_uid || a->st_gid != b->st_gid)
        return false;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}

static int open_input(CDG_FILE *file, const char *name)
{
    file->path = realpath(name, NULL);
    if (!file->path)
        return -1;
    int fd = open(file->path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0)
        return -1;
    if (fstat(fd, &file->original) < 0)
        goto fail;
    if (!S_ISREG(file->original.st_mode) || file->original.st_size <= 0) {
        errno = EINVAL;
        goto fail;
    }
    file->size = (uint64_t)file->original.st_size;
    file->input = fdopen(fd, "rb");
    if (file->input)
        return 0;
fail:;
    int error = errno;
    close(fd);
    errno = error;
    return -1;
}

static int verify_input(CDG_FILE *file)
{
    struct stat open_info, named_info;
    if (fstat(fileno(file->input), &open_info) < 0 || lstat(file->path, &named_info) < 0)
        return -1;
    if (!same_input(&file->original, &open_info) || !same_input(&file->original, &named_info)) {
        errno = CDG_INPUT_CHANGED;
        return -1;
    }
    return 0;
}

static int begin_replacement(CDG_FILE *file, uint64_t prefix)
{
    struct stat current;
    int writable = open(file->path, O_RDWR | O_NONBLOCK | O_NOFOLLOW);
    if (writable < 0)
        return -1;
    int result = fstat(writable, &current);
    int stat_error = errno;
    int closed = close(writable);
    if (result < 0) {
        errno = stat_error;
        return -1;
    }
    if (closed < 0)
        return -1;
    if (!same_input(&file->original, &current)) {
        errno = CDG_INPUT_CHANGED;
        return -1;
    }

    static const char suffix[] = ".saroo-cdg-XXXXXX";
    const char *slash = strrchr(file->path, '/');
    size_t length = (size_t)(slash - file->path) + 1; /* realpath is absolute. */
    if (length > SIZE_MAX - sizeof(suffix)) {
        errno = ENAMETOOLONG;
        return -1;
    }
    file->temporary = malloc(length + sizeof(suffix));
    if (!file->temporary)
        return -1;
    memcpy(file->temporary, file->path, length);
    file->temporary[length] = '\0';
    file->parent = open(file->temporary, O_RDONLY | O_DIRECTORY);
    if (file->parent < 0)
        return -1;
    memcpy(file->temporary + length, suffix, sizeof(suffix));
    file->output = mkstemp(file->temporary);
    if (file->output < 0)
        return -1;
    file->temporary_created = true;
    if (fstat(file->output, &current) < 0 ||
        ((current.st_uid != file->original.st_uid || current.st_gid != file->original.st_gid) &&
         fchown(file->output, file->original.st_uid, file->original.st_gid) < 0))
        return -1;
#ifdef __APPLE__
    if (fcopyfile(fileno(file->input), file->output, NULL, COPYFILE_METADATA) < 0)
        return -1;
#else
    if (fchmod(file->output, file->original.st_mode & 07777) < 0)
        return -1;
#endif

    unsigned char buffer[8192];
    for (uint64_t offset = 0; offset < prefix;) {
        size_t bytes = prefix - offset < sizeof(buffer) ? (size_t)(prefix - offset) : sizeof(buffer);
        ssize_t count = pread(fileno(file->input), buffer, bytes, (off_t)offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0) {
            if (!count)
                errno = EIO;
            return -1;
        }
        if (write_all(file->output, buffer, (size_t)count))
            return -1;
        offset += (size_t)count;
    }
    return 0;
}

static int publish_replacement(CDG_FILE *file)
{
    if (fsync(file->output) < 0)
        return -1;
    int closed = close(file->output);
    file->output = -1;
    if (closed < 0 || rename(file->temporary, file->path) < 0)
        return -1;
    file->replaced = true;
    if (fsync(file->parent) < 0)
        return -1;
    closed = close(file->parent);
    file->parent = -1;
    return closed;
}

static void cleanup_file(CDG_FILE *file)
{
    if (file->input)
        fclose(file->input);
    if (file->output >= 0)
        close(file->output);
    if (file->parent >= 0)
        close(file->parent);
    if (file->temporary_created && !file->replaced)
        unlink(file->temporary);
    free(file->temporary);
    free(file->path);
}

#endif
#endif
