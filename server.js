const express = require('express')
const morgan = require('morgan')//logger

const app = express()
const port = 3000

app.use(morgan("combined"))

//handle post request with raw frame
app.post("/frame",(req,res)=>{
    const chunks=[]
    var buffer

    req.on("data",data=>chunks.push(data))

    req.on("end", e=>{
        buffer=Buffer.concat(chunks)//Uint8Array
        console.log(">>req finished, buffer size:",buffer.length)

        //Available pixel formats in ffmpeg: https://ffmpeg.org/doxygen/2.7/pixfmt_8h.html#a9a8e335cf3be472042bc9f0cf80cd4c5
        //Do something with frame data and send it back (for example reverse all data in buffer/frame)
        res.send(buffer.reverse())

    })//on

    // req.pipe(res)//echo server
})//post

app.listen(port, () => {
  console.log(`Server listening at http://localhost:${port}`)
})