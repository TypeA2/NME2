#pragma once

#include <Windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <sys/stat.h>

// Yes this is stolen from Qt
#define UNUSED(x) (void)x

#define TRIM(c) realloc(c, strlen(c))

#define VERSION_MAJOR 0
#define VERSION_MINOR 2

#define PATH_INV    0
#define PATH_DIR    1
#define PATH_FILE   2

#define RESPONSE_NIL     0
#define RESPONSE_NO      1
#define RESPONSE_YES     2
#define RESPONSE_YES_ALL 3

#define FORMAT_NIL 0
#define FORMAT_USM 1

#define VP9_LIB     "libvpx-vp9"
#define H265_LIB    "libx265"
#define H264_LIB    "libx264"
#define VIDEO_LIB_FALLBACK H264_LIB

#define VIDEO_QUALITY_FALLBACK_VP9  "-crf 24 -b:v 0"
#define VIDEO_QUALITY_FALLBACK_H264 "-crf 18"
#define VIDEO_QUALITY_FALLBACK_H265 "-crf 21"

#define CMD_BASE_VIDEO "ffmpeg -hide_banner -v fatal -stats -f mpegvideo -i \"%s\" -an -c:v %s %s %s -threads %i %s -y \"%s\""

#define CMD_MAX_LENGTH 0x1FFF

typedef unsigned char format;
typedef unsigned char path_t;
typedef unsigned char yn_response;

typedef struct fpath {
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
} fpath;

typedef struct VideoArgs {
    char* output;
    char* encoder;
    char* quality;
    char* filters;
    char* format;
} VideoArgs;

typedef union Args {
    VideoArgs video_args;
} Args;

typedef struct File {
    fpath input;
    fpath output;
    format format;
    Args args;
} File;

typedef struct VersionInfo {
    int MAJOR;
    int MINOR;
    char BUILD_TIME[sizeof __TIMESTAMP__];
} VersionInfo;