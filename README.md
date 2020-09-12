## Simple HTTP server for testing FFmpeg http video filter
A simple server demonstrating ffmpeg "http" raw video remote frame filtering. This wiil help to postprocess frames outside of ffmpeg process.

## Demo server for frames postprocess
```
// install required dependencies
npm i

// start server
npm run start

```

## Start ffmpeg process with http filter
Run next command. Also, remember, that we need to escape url characters in command!
```
./ffmpeg -i input.mp4 -vf format=rgb24,http=url="http\\\://localhost\\\:3000/frame?param=abc" -t 10 -an -y out.mp4
```
After that, ffmpeg will be send every frame to `url` as post request for postprocessing frame and will be include additional info in request headers for every frame.
```
  frame_width: '1280',
  frame_height: '720',
  frame_pix_fmt: 'rgb24',
  frame_aspect_ratio: '1.000000'
```

Then you send frames back from server to ffmpeg process you must include new image params in request header, like that:
```
    //in this example we don't change image size or format, so we set original frame params
    res.header('frame_width',req.headers["frame_width"]);
    res.header('frame_height',req.headers["frame_height"]);
    res.header('frame_pix_fmt',req.headers["frame_pix_fmt"]);//yuv420p, rgb24, etc. (all ffmpeg supported pixel formats)
```