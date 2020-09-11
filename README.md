## Simple HTTP server for testing FFmpeg http video filter
A simple server demonstrating ffmpeg "http" raw video remote frame filtering.

## Usage
```
// install required dependencies
npm i

// start server
npm run start

```

## Testing
To specify url we need to escape characters in remote url.
```
ffmpeg -i video.mp4 -vf format=rgb24,http=url="http\\\://localhost\\\:3000/frame?type=ffmpeg" -t 10 -an -y out.mp4
```
