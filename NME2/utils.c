#include "utils.h"

VersionInfo PrintVersionInfo(void) {
    VersionInfo version;
    version.MAJOR = VERSION_MAJOR;
    version.MINOR = VERSION_MINOR;
    strcpy_s(version.BUILD_TIME, sizeof version.BUILD_TIME, __TIMESTAMP__);
    printf("NieR:Automata(tm) Media Extractor Build %i.%i (at %s)\n", version.MAJOR, version.MINOR, version.BUILD_TIME);

    return version;
}

void perrf(const char* f, ...) {
    va_list args;
    HANDLE hErr;

    va_start(args, f);
    hErr = GetStdHandle(STD_ERROR_HANDLE);

    SetConsoleTextAttribute(hErr, 12);

    vfprintf(stderr, f, args);

    SetConsoleTextAttribute(hErr, 7);

    va_end(args);
}

void pwarnf(const char* f, ...) {
    va_list args;
    HANDLE hErr;

    va_start(args, f);
    hErr = GetStdHandle(STD_ERROR_HANDLE);

    SetConsoleTextAttribute(hErr, 14);

    vfprintf(stdout, f, args);

    SetConsoleTextAttribute(hErr, 7);

    va_end(args);
}

path_t ResolveFullpath(char* buf, const char* path) {
    if (_fullpath(buf, path, _MAX_PATH) == NULL) {
        return PATH_INV;
    } else {
        // Check if the path is a directory
        wchar_t* path_w = malloc(strlen(buf) * sizeof(wchar_t));

        mbstowcs_s(NULL, path_w, strlen(buf) * sizeof(WORD), buf, _TRUNCATE);

        DWORD attr = GetFileAttributes(path_w);
        
        free(path_w);

        if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
            // Append a backslash if there's not one already
            if (buf[strlen(buf) - 1] != '\\') {
                strcat_s(buf, _MAX_PATH, "\\");
            }

            return PATH_DIR;
        } else {
            return PATH_FILE;
        }
    }
}

yn_response ConfirmOverwrite(const fpath path, bool multi) {
    char c;

    if (multi == true) {
        printf("File '%s%s' and subsequent files already exist. Overwrite? [a/y/n]: ", path.fname, path.ext);
    } else {
        printf("File '%s%s' already exists. Overwrite? [y/n]: ", path.fname, path.ext);
    }

    c = (char)getchar();

    while (c != '\n' && getchar() != '\n');

    if (c == 'y' || c == 'Y') {
        printf("Overwriting '%s%s'\n", path.fname, path.ext);
        
        return RESPONSE_YES;
    } else if (multi == true && (c == 'a' || c == 'A')) {
        printf("Overwriting '%s%s' and all subsequent files\n", path.fname, path.ext);

        return RESPONSE_YES_ALL;
    } else if (multi == true && (c == 'n' || c == 'N')) {
        return RESPONSE_NO;
    } else if (multi == true) {
        return RESPONSE_NO;
    } else {
        perrf("Not overwriting file, exiting");

        exit(1);
    }

    return RESPONSE_NIL;
}

bool CheckFileSignature(fpath path, char sig[]) {
    const int SIGNATURE_SIZE = (const int)strlen(sig);
    char* signature = malloc(SIGNATURE_SIZE);
    char* path_str = MakePath(path);
    FILE* file;

    if (fopen_s(&file, path_str, "r") != 0) {
        perrf("Could not open '%s'\n", path_str);

        return false;
    }

    const int read = (const int)fread_s(signature, SIGNATURE_SIZE, 1, SIGNATURE_SIZE, file);

    // Confirm that the entire signature was read
    if (read < SIGNATURE_SIZE) {
        perrf("Could not read the file's signature.");

        exit(1);
    }

    fclose(file);
    
    // For some reason the returned signature length is too large sometimes. We truncate it like this
    if (strlen(signature) > SIGNATURE_SIZE) {
        *(signature + SIGNATURE_SIZE) = '\0';
    }

    bool success = strcmp(signature, sig) == 0;

    free(signature);

    return success;
}

int LongestStrlen(const int n,  ...) {
    va_list args;

    int length = 0;
    
    va_start(args, n);

    for (int i = 0; i < n; i++) {
        int str = (int)strlen(va_arg(args, char*));
        if (str > length) {
            length = str;
        }
    }

    va_end(args);

    return length;
}

void PrintSettingsVideo(File* file) {

    printf("\n"
        "Input:\t\t%s\n"
        "Output:\t\t%s\n"
        "Encoder:\t%s\n"
        "Quality:\t'%s'\n"
        "Filters:\t%s\n"
        "Format:\t\t%s\n\n", MakePath(file->input), MakePath(file->output), file->args.video_args.encoder, file->args.video_args.quality, file->args.video_args.filters, file->args.video_args.format);
}

char* MakePath(fpath path) {
    char* output = malloc(_MAX_PATH);

    if (_makepath_s(output, _MAX_PATH, path.drive, path.dir, path.fname, path.ext) != 0) {
        perrf("Error creating path %s%s%s%s", path.drive, path.dir, path.fname, path.ext);

        exit(1);
    }

    return output;
}

wchar_t* MakePathW(fpath path){
    wchar_t* output = malloc(MAX_PATH * sizeof(wchar_t));
    wchar_t* drive = malloc(_MAX_DRIVE * sizeof(wchar_t));
    wchar_t* dir = malloc(_MAX_DIR * sizeof(wchar_t));
    wchar_t* fname = malloc(_MAX_FNAME * sizeof(wchar_t));
    wchar_t* ext = malloc(_MAX_EXT * sizeof(wchar_t));

    mbstowcs_s(NULL, drive, _MAX_DRIVE * sizeof(WORD), path.drive, _TRUNCATE);
    mbstowcs_s(NULL, dir, _MAX_DIR * sizeof(WORD), path.dir, _TRUNCATE);
    mbstowcs_s(NULL, fname, _MAX_FNAME * sizeof(WORD), path.fname, _TRUNCATE);
    mbstowcs_s(NULL, ext, _MAX_EXT * sizeof(WORD), path.ext, _TRUNCATE);

    if (_wmakepath_s(output, sizeof(wchar_t) * _MAX_PATH, drive, dir, fname, ext) != 0) {
        perrf("Error creating path %s%s%s%s", path.drive, path.dir, path.fname, path.ext);

        exit(1);
    }

    free(drive);
    free(dir);
    free(fname);
    free(ext);

    return output;
}

char* ConstructCommand(File* file) {
    char* cmd = malloc(CMD_MAX_LENGTH);

    SYSTEM_INFO* info = malloc(sizeof(SYSTEM_INFO));
    GetSystemInfo(info);

    int thread_count = info->dwNumberOfProcessors;

    free(info);

    switch (file->format) {
        case FORMAT_USM:
            sprintf_s(cmd, CMD_MAX_LENGTH, CMD_BASE_VIDEO,
                MakePath(file->input), file->args.video_args.encoder,
                file->args.video_args.quality, file->args.video_args.filters,
                thread_count, file->args.video_args.format, MakePath(file->output));
            break;
        case FORMAT_WSP:
            sprintf_s(cmd, CMD_MAX_LENGTH, CMD_BASE_AUDIO,
                file->args.audio_args.encoder, file->args.audio_args.quality,
                file->args.audio_args.sample_fmt, thread_count,
                MakePath(file->output));
            break;
        default:
            perrf("Unknown format %i\n%s\n%s\n", file->format, MakePath(file->input), MakePath(file->output));

            exit(1);
    }
    
    cmd = TRIM(cmd);

    return cmd;
}

void WriteToLog(const char* str) {
    FILE* log;
    SYSTEMTIME t;
    char* msg = malloc(30 + strlen(str));

    GetSystemTime(&t);

    sprintf_s(msg, 30 + strlen(str), "\n[%04d-%d-%d %02d:%02d:%02d:%03d]: %s\n", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, str);
    msg = TRIM(msg);

    fopen_s(&log, "conversion.log", "a");
    fwrite(msg, strlen(msg) - 1, 1, log);
    fclose(log);
}

format GetFileFormat(const fpath path) {
    if (_stricmp(path.ext, ".usm") == 0) {
        if (CheckFileSignature(path, "CRID")) {
            return FORMAT_USM;
        } else {
            perrf("Incomplete format for '%s'", MakePath(path));

            exit(1);
        }
    } else if (_stricmp(path.ext, ".wsp") == 0 || _stricmp(path.ext, ".wem") == 0) {
        if (CheckFileSignature(path, "RIFF")) {
            return FORMAT_WSP;
        } else {
            perrf("Incomplete format for '%s'", MakePath(path));

            exit(1);
        }
    } else if(_stricmp(path.ext, ".cpk") == 0){
        if (CheckFileSignature(path, "CPK ")) {
            return FORMAT_CPK;
        } else {
            perrf("Incomplete format for '%s'", MakePath(path));

            exit(1);
        }
    } else {
        return FORMAT_NIL;
    }
}
