/*
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * http video filter
 */

#include "libavutil/avassert.h"
#include "libavutil/opt.h"
#include "libavutil/internal.h"
#include "libavutil/imgutils.h"
#include "libavutil/avstring.h"
#include "avfilter.h"
#include "formats.h"
#include "internal.h"
#include "video.h"
#include "stdio.h"
#include "curl/curl.h"

struct MemoryStruct {
  uint8_t *memory;
  size_t size;
};

typedef struct HttpContext {
    const AVClass *class;
    char *url;
    char *content_type;
    CURL *curl;
    struct curl_slist *headers;
} HttpContext;

#define OFFSET(x) offsetof(HttpContext, x)
#define FLAGS AV_OPT_FLAG_FILTERING_PARAM|AV_OPT_FLAG_VIDEO_PARAM

static const AVOption http_options[] = {
    { "url", "set remote url address", OFFSET(url), AV_OPT_TYPE_STRING, {.str=NULL}, FLAGS },
    { "content_type", "set 'Content-Type' request header", OFFSET(content_type), AV_OPT_TYPE_STRING, {.str="application/octet-stream"}, FLAGS },
    { NULL }
};

AVFILTER_DEFINE_CLASS(http);

static av_cold int init(AVFilterContext *ctx)
{
    HttpContext *http = ctx->priv;
    http->curl = curl_easy_init();
    return 0;
}

static av_cold void uninit(AVFilterContext *ctx)
{
    HttpContext *http = ctx->priv;
    
    curl_easy_cleanup(http->curl);
    curl_slist_free_all(http->headers);
    curl_global_cleanup();
}

static int query_formats(AVFilterContext *ctx)
{
    const HttpContext *http = ctx->priv;
    return ff_default_query_formats(ctx);
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
      /* out of memory! */ 
      return AVERROR(ENOMEM); 
    }
 
    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;
 
  return realsize;
}

struct res_headers {
  int width;
  int height;
  enum AVPixelFormat pix_fmt;
};

/* parse recieved response headers*/
static size_t header_callback(char *buffer, size_t size, size_t nitems, void *userdata)
{
  size_t numbytes = size * nitems;
  struct res_headers *h = (struct res_headers *)userdata;

    if(strstr(buffer, "frame_width: ")!=NULL){
        int r;
        int value=0;
        r = sscanf(buffer, "frame_width: %d\n", &value);
        if(r){       
           h->width=value;  
        }  
    }     
    if(strstr(buffer, "frame_height: ")!=NULL){
        int r;
        int value=0;
        r = sscanf(buffer, "frame_height: %d\n", &value);
        if(r){       
           h->height=value;  
        }  
    }    

    if(strstr(buffer, "frame_pix_fmt: ")!=NULL){
        int r;
        char *value[100];
        r = sscanf(buffer, "frame_pix_fmt: %s\n", &value);
        if(r){       
           h->pix_fmt=av_get_pix_fmt(value);  
        }  
    }

  return numbytes;
}

static int filter_frame(AVFilterLink *inlink, AVFrame *in)
{
    AVFilterContext *ctx = inlink->dst;
    AVFilterLink *outlink = ctx->outputs[0];
    HttpContext *http = inlink->dst->priv;
    AVFrame *out;

    CURL *curl=http->curl;
    CURLU *url= curl_url();
    CURLcode res;
    struct MemoryStruct chunk;
       chunk.memory = av_malloc(1);
       chunk.size = 0;    

    /* create buffer and copy input frame */
    int buff_size=av_image_get_buffer_size(in->format, in->width, in->height, 1);
    uint8_t *buffer=av_malloc(buff_size);    
    av_image_copy_to_buffer(buffer, buff_size, in->data, in->linesize, in->format, in->width, in->height, 1);
 
    curl_easy_setopt(curl, CURLOPT_URL, http->url);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buffer);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, buff_size);

    /* send all data to this function  */ 
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        
    /* we pass our 'chunk' struct to the callback function */ 
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* set request headers */
    http->headers=NULL;
    http->headers = curl_slist_append(http->headers, av_asprintf("Content-Type: %s",http->content_type));
    http->headers = curl_slist_append(http->headers, "Expect:");

    /* server must know info about current frame, so we set frame data in custom request headers*/
    http->headers = curl_slist_append(http->headers, av_asprintf("frame_width: %d",in->width));
    http->headers = curl_slist_append(http->headers, av_asprintf("frame_height: %d",in->height));
    http->headers = curl_slist_append(http->headers, av_asprintf("frame_pix_fmt: %s",av_get_pix_fmt_name(in->format)));
    http->headers = curl_slist_append(http->headers, av_asprintf("frame_aspect_ratio: %f",av_q2d(in->sample_aspect_ratio)));
    

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, http->headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);    
      
    struct res_headers headers_info = { 0, 0, -1 }; /* received  new frame info from server */
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headers_info);

    /* Perform the request, res will get the return code */ 
    res = curl_easy_perform(curl);

    /* Check for errors */ 
    if(res != CURLE_OK){
        av_log(NULL, AV_LOG_ERROR, "http filter failed: %s\n", curl_easy_strerror(res));
        av_frame_free(&in);
        // av_frame_free(&out);
        free(buffer);
    return AVERROR(EINVAL);
    }
    else{
    // printf(">>>>>>headers: width: %d, height: %d, pix_fmt: %d\n",headers_info.width,headers_info.height,headers_info.pix_fmt);
    if(headers_info.width==0||headers_info.height==0){
        av_log(NULL, AV_LOG_ERROR, "http filter failed: invalid frame size in response headers!\n");
        return AVERROR(EINVAL);        
    }
    if(headers_info.pix_fmt==-1){
           av_log(NULL, AV_LOG_ERROR, "http filter failed: invalid frame pixel format in response headers!\n");
    return AVERROR(EINVAL);        
    }

        /* fill frame data from received buffer */
        // av_image_fill_arrays(out->data, out->linesize, chunk.memory, out->format, out->width, out->height, 1);
        out = ff_get_video_buffer(outlink, headers_info.width, headers_info.height);
    if (!out) {
       av_frame_free(&in);
       return AVERROR(ENOMEM); 
    }
       av_frame_copy_props(out, in);
       av_image_fill_arrays(out->data, out->linesize, chunk.memory, headers_info.pix_fmt, headers_info.width, headers_info.height, 1);
    }
       
    /* cleanup */ 
    curl_url_cleanup(url);
    av_free(buffer);
    av_frame_free(&in);
    return ff_filter_frame(outlink, out);
} 

static const AVFilterPad avfilter_vf_http_inputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
        .filter_frame = filter_frame,        
    },
    { NULL }
};

static const AVFilterPad avfilter_vf_http_outputs[] = {
    {
        .name = "default",
        .type = AVMEDIA_TYPE_VIDEO,
    },
    { NULL }
};

AVFilter ff_vf_http = {
    .name          = "http",
    .description   = NULL_IF_CONFIG_SMALL("Send raw frame data to the remote server for postprocessing and await response as new frame."),
    .priv_size     = sizeof(HttpContext),
    .init          = init,
    .uninit        = uninit,
    .query_formats = query_formats,
    .inputs        = avfilter_vf_http_inputs,
    .outputs       = avfilter_vf_http_outputs,
    .priv_class    = &http_class,
};
