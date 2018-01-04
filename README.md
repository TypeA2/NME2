# NME2
Extracts NieR:Automata™ media files. Requires [ffmpeg](https://ffmpeg.org/).
Currently uses ffmpeg for converting cutscene video, and an internal modified version of [ww2ogg](https://github.com/hcs64/ww2ogg) along with [revorb](https://hydrogenaud.io/index.php/topic,64328.0.html#msg574110) for audio.

### Usage
All arguments except ```<input>``` are optional and have default values
```
nme <input> (options) (-p <pattern>)
```
- ```<input>```
  - Relative or absolute path to a file
  - Relative or absolute path to a directory
- ```<pattern>```
  - Only used when ```<input>``` points to a directory.
  - Can contain wildcards: ```*```, ```?```

<br>

##### Audio files (\*.wsp, \*.wem)
```
nme <input> -ac <codec> -aq <quality> -sf <samplefmt>
```
- ```<codec>```
  - The audio codec to be used. Supported values (case-insensitive):
    - ```flac```
    - ```vorbis```
	- ```opus```
	- ```aac```
	- ```mp3```
	- ```f32``` (PCM 32-bit float LE)
	- ```f64``` (PCM 64-bit float LE)
	- ```s16``` (PCM 16-bit integer LE)
	- ```s24``` (PCM 24-bit integer LE)
	- ```s32``` (PCM 32-bit integer LE)
	- ```s64``` (PCM 64-bit integer LE)

- ```<quality>```
  - The audio quality to use. Syntax:
    - FLAC
	  - Number with up to 1 decimal point, in the range 0 - 12.5, indicating the compression level
	- Vorbis
	  - From 45k up to 500k, indicating the number of bits per second in kbps (SI)
	- Opus
	  - From 52k up to 512k.
	- AAC
	  - From 46k up to 576k indicating a constant bitrate
	  - Number with up to 1 decimal point, in the range 0.1 - 11.9 indicating the average quality
	- MP3
	  - One of the following values: 8k, 16k, 24k, 32k, 40k, 48k, 64k, 80k, 96k, 112k, 128k, 160k, 192k, 224k, 256k, 320k
  - This option is ignored when using any of the PCM codecs

- ```<samplefmt>```
  - The audio sample format. Syntax:
    - FLAC:
	  - ```s16``` or ```s32```, indicating 16-bit or 32-bit samples
	- Vorbis / AAC
	  - ```fltp``` only, indicating planar floating point samples
	- Opus
	  - ```flt``` only, indicating non-planar floating point samples
	- MP3
	  - ```s16p``` only, indicating planar 16-bit samples
  - This options ignored when using any of the PCM codecs

<br>

##### Video files (\*.usm)

```
nme <input> -vc <codec> -vq <quality> -vf <filters>
```

- ```<codec>```
  - The video codec to used. Supported values (case-insensitive):
    - ```h264```
    - ```h265``` or ```hevc```
    - ```vp9```

- ```<quality>```
  - The video quality to use
    - Variable bitrate (CRF): ```-q 16```
    - Specified bitrate: ```-q 8M```
    - Lossless: ```-q lossless```
      - Lossless translates to ```-crf 0``` with H.264 and H.265, and to ```-lossless 1``` with VP9

- ```<filters>```
  - Filters in the same format [ffmpeg uses](https://trac.ffmpeg.org/wiki/FilteringGuide)
  - Defaults to ```crop=1600:900:0:0``` to crop out 4 invalid lines at the bottom of the video
  - If ```crop=``` is not found in the user-supplied string the default value will be prepended