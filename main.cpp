/*
 * DiskWrite - writes a disk image into a physical disk.
 *
 *
 * Autor: Carlos Ramos
 *
 */
#include <cstdio>
#include <windows.h>

bool IsAdmin() {
    bool isAdmin = false;
    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION Elevation;
        DWORD dwSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &Elevation, dwSize, &dwSize)) {
            isAdmin = true;
        }
        CloseHandle(hToken);
    }

    return isAdmin;
}

int main(int argc, const char * argv[])
{
    if (argc != 3) {
        printf("Usage: DiskWrite <image_file> <disk_number>\n");
        printf("\nTo get the disk number you can use PowerShell cmdlet: get-disk\n");
        return 1;
    }

    if (!IsAdmin()) {
        fprintf(stderr, "You must launch this program with admin privileges.\n");
        return 1;
    }

    const char *imagePath = argv[1];
    int driveNumber = atoi(argv[2]);
    char drivePath[64];
    snprintf(drivePath, sizeof(drivePath), "\\\\.\\PhysicalDrive%d", driveNumber);

    HANDLE hDrive = CreateFileA(drivePath, GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
        FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);

    if (hDrive == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFileA failed with error %d\n", GetLastError());
        return 1;
    }

    DWORD dwBytesReturned = 0;
    if (!DeviceIoControl(hDrive, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL)) {
        fprintf(stderr, "DeviceIoControl failed with error %d\n", GetLastError());
        CloseHandle(hDrive);
        return 1;
    }

    HANDLE hImage = CreateFileA(imagePath, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, 0);
    if (hImage == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "CreateFileA failed with error %d\n", GetLastError());
        CloseHandle(hDrive);
        return 1;
    }

    DWORD bufferSize = 64 * 1024;
    LPVOID buffer = VirtualAlloc(NULL, bufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!buffer) {
        fprintf(stderr, "VirtualAlloc failed with error %d\n", GetLastError());
        CloseHandle(hImage);
        CloseHandle(hDrive);
        return 1;
    }

    LARGE_INTEGER totalRead {}, totalWritten {};
    DWORD read, written;
    BOOL result;

    LARGE_INTEGER totalSize {};
    if (!GetFileSizeEx(hImage, &totalSize)) {
        fprintf(stderr, "GetFileSizeEx failed with error %d\n", GetLastError());
        CloseHandle(hImage);
        CloseHandle(hDrive);
        return 1;
    }

    while (ReadFile(hImage, buffer, bufferSize, &read, NULL) && read > 0) {
        result = WriteFile(hDrive, buffer, read, &written, NULL);
        if (!result || read != written) {
            fprintf(stderr, "WriteFile failed with error %d\n", GetLastError());
            break;
        }
        totalRead.QuadPart += read;
        totalWritten.QuadPart += written;

        printf("Progress: %.2f%%\r", (double)totalWritten.QuadPart * 100 / totalSize.QuadPart);
    }

    VirtualFree(buffer, 0, MEM_RELEASE);

    DeviceIoControl(hDrive, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &dwBytesReturned, NULL);
    CloseHandle(hDrive);

    if (totalWritten.QuadPart == GetFileSize(hImage, NULL)) {
        CloseHandle(hImage);
        printf("\nDone.\nTotal read: %lld bytes.\nTotal written: %lld bytes.\n", totalRead.QuadPart, totalWritten.QuadPart);
        return 0;
    }
    else {
        CloseHandle(hImage);
        printf("\nDone, but couldn't write all the data.\nTotal read: %lld bytes.\nTotal written: %lld bytes.\n", totalRead.QuadPart, totalWritten.QuadPart);
        return 1;
    }

}
