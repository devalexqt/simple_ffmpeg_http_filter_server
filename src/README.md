# This is source code of ffmpeg http video filter

## Install dependency

To install `libcurl >= 7.68` follow the instruction for example: https://www.mysterydata.com/install-latest-curl-version-on-ubuntu-vestacp/


## Copy and modify files
Copy file `vf_http.c` to `ffmpeg/libavfilter/vf_http.c`

Add to to `Makefile` neer line 470:
```
OBJS-$(CONFIG_HTTP_FILTER)                   += vf_http.o

```
Add to `configure`:

neer line 6383:
```
enabled libcurl           && require "libcurl >= 7.68.0" curl/curl.h curl_easy_init -lcurl

```

neer line 328:
```
  --enable-libcurl         enable http filter that send raw frames to remote server

```

neer line 1831:

```
libcurl
```

neer line 3656:
```
http_filter_deps="libcurl"
```

Add to `ffmpeg/libavfilter/allfilters.c` neer line :
```
extern AVFilter ff_vf_http;

```

## Build

```
./configure --enable-libx264 --enable-nonfree --enable-gpl --disable-doc --enable-libcurl
make -j`nproc`
make install
```

## Testing
To specify url we need to escape characters in remote url.
```
ffmpeg -i video.mp4 -vf format=rgb24,http=url="http\\\://localhost\\\:3000/frame?type=ffmpeg" -t 10 -an -y out.mp4
```
This command will send raw frames to url `POST http://localhost:3000/frame?type=ffmpeg&width=1280&height=720&format=2&linesize=344336064&size=2764800&pts=22012` and you can postprocess it on server and return result frame in response.

