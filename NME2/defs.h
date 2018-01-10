#pragma once

#include <Windows.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>


// Yes this is stolen from Qt
#define UNUSED(x) (void)x

#define TRIM(c) realloc(c, strlen(c))

#define VERSION_MAJOR 0
#define VERSION_MINOR 5

#define PATH_INV    0
#define PATH_DIR    1
#define PATH_FILE   2

#define RESPONSE_NIL     0
#define RESPONSE_NO      1
#define RESPONSE_YES     2
#define RESPONSE_YES_ALL 3

#define FORMAT_NIL 0
#define FORMAT_USM 1
#define FORMAT_WSP 2
#define FORMAT_CPK 3

#define VP9_CODEC     "libvpx-vp9"
#define H265_CODEC    "libx265"
#define H264_CODEC    "libx264"
#define VIDEO_CODEC_FALLBACK H264_CODEC

#define FLAC_CODEC    "flac"
#define OPUS_CODEC    "libopus"
#define VORBIS_CODEC  "libvorbis"
#define AAC_CODEC     "aac"
#define MP3_CODEC     "libmp3lame"
#define PCM_F32_CODEC "pcm_f32le"
#define PCM_F64_CODEC "pcm_f64le"
#define PCM_S16_CODEC "pcm_s16le"
#define PCM_S24_CODEC "pcm_s24le"
#define PCM_S32_CODEC "pcm_s32le"
#define PCM_S64_CODEC "pcm_s64le"
#define AUDIO_CODEC_FALLBACK FLAC_CODEC

#define FLAC_FALLBACK_SAMPLE_FMT "-sample_size s16"
#define MP3_FALLBACK_SAMPLE_FMT  "-sample_size s16p"

#define VIDEO_QUALITY_FALLBACK_VP9  "-crf 24 -b:v 0"
#define VIDEO_QUALITY_FALLBACK_H264 "-crf 18"
#define VIDEO_QUALITY_FALLBACK_H265 "-crf 21"

#define AUDIO_QUALITY_FALLBACK_FLAC   "-compression_level 9"
#define AUDIO_QUALITY_FALLBACK_OPUS   "-b:a 320k"
#define AUDIO_QUALITY_FALLBACK_VORBIS "-b:a 320k"
#define AUDIO_QUALITY_FALLBACK_AAC    "-b:a 320k"
#define AUDIO_QUALITY_FALLBACK_MP3    "-b:a 320k"

#define CMD_BASE_VIDEO "ffmpeg -hide_banner -v fatal -stats -f mpegvideo -i \"%s\" -an -c:v %s %s %s -threads %i %s -y \"%s\""
#define CMD_BASE_AUDIO "ffmpeg -hide_banner -v fatal -i - -c:a copy -f ogg - | revorb - - | ffmpeg -hide_banner -v fatal -stats -i - -c:a %s %s %s -threads %i -y \"%s\""

#define CMD_MAX_LENGTH 0x1FFF

#define OFFSET_OFFSET   71991
#define CODEBOOK_COUNT  599

typedef unsigned char format;
typedef unsigned char path_t;
typedef unsigned char yn_response;

typedef struct {
    char drive[_MAX_DRIVE];
    char dir[_MAX_DIR];
    char fname[_MAX_FNAME];
    char ext[_MAX_EXT];
} fpath;

typedef struct {
    char* output;
    char* encoder;
    char* quality;
    char* filters;
    char* format;
} VideoArgs;

typedef struct {
    char* encoder;
    char* quality;
    char* sample_fmt;
} AudioArgs;

typedef union {
    VideoArgs video_args;
    AudioArgs audio_args;
} Args;

typedef struct {
    fpath input;
    fpath output;
    format format;
    Args args;
} File;

typedef struct {
    int MAJOR;
    int MINOR;
    char BUILD_TIME[sizeof __TIMESTAMP__];
} VersionInfo;

extern const unsigned char pcb[];
