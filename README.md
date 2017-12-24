# NME2
Extracts NieR:Automata™ media files. Requires [ffmpeg](https://ffmpeg.org/).
Currently only works on the game's cutscene (\*.usm) files

### Usage
```
nme <input> -c <codec> -q <quality> -vf <filters> (-p <pattern>)
```
All arguments except ```<input>``` are optional and have default values
- ```<input>```
  - Relative or absolute path to a file
  - Relative or absolute path to a directory
- ```<codec>```
  - The video codec to used. Supported values:
    - H264
    - H265 / HEVC
    - VP9
- ```<quality>```
  - The quality to use
    - Variable bitrate (CRF): ```-q 16```
    - Specified bitrate: ```-q 8M```
    - Lossless: ```-q lossless```
      - Lossless translates to ```-crf 0``` with H.264 and H.265, and to ```-lossless 1``` with VP9
- ```<filters>```
  - Filters in the same format [ffmpeg uses](https://trac.ffmpeg.org/wiki/FilteringGuide)
  - Defaults to ```crop=1600:900:0:0``` to crop out 4 invalid lines at the bottom of the video
  - If ```crop=``` is not found in the user-supplied string the default value will be prepended
- ```<pattern>```
  - Only used when ```<input>``` points to a directory.
  - The pattern which is used to search for files
  - Can contain wildcards: ```*```, ```?```
  
