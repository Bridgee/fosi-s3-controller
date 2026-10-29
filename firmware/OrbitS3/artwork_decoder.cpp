#include "artwork_decoder.h"
#include <lvgl.h>
#include <extra/libs/sjpg/tjpgd.h>
#define LODEPNG_NO_COMPILE_CPP
extern "C" {
#include <extra/libs/png/lodepng.h>
}
#include <stdlib.h>
#include <string.h>
#ifndef ORBIT_HOST_TEST
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

// Bundled LodePNG uses lv_mem_* wrappers. With this pinned configuration they
// delegate to the thread-safe PSRAM allocator, rather than LVGL's TLSF pool.
#if LV_MEM_CUSTOM != 1 || LV_MEMCPY_MEMSET_STD != 1
#error "Background PNG decoding requires custom thread-safe allocation and standard memcpy"
#endif
static_assert(LV_COLOR_DEPTH==16 && LV_COLOR_16_SWAP==0,"RGB565 layout required");
#ifdef ORBIT_HOST_TEST
extern size_t test_psram_free();
extern size_t test_psram_largest();
#endif
namespace s3 {
namespace {
uint16_t rgb(unsigned r,unsigned g,unsigned b) { return uint16_t(((r>>3)<<11)|((g>>2)<<5)|(b>>3)); }
void *allocate(size_t bytes) {
#ifdef ORBIT_HOST_TEST
    return malloc(bytes);
#else
    return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
#endif
}
bool png_headroom() {
#ifdef ORBIT_HOST_TEST
    return ::test_psram_free()>=4*1024*1024 && ::test_psram_largest()>=1280*1024;
#else
    return heap_caps_get_free_size(MALLOC_CAP_SPIRAM)>=4*1024*1024 &&
        heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)>=1280*1024;
#endif
}
struct JpegContext {
    const uint8_t *data;size_t size,pos=0;
    uint16_t *output;
    unsigned sw=0,sh=0,dw=0,dh=0,ox=0,oy=0,side=ArtworkSide;
};
size_t jpeg_read(JDEC *decoder,uint8_t *dest,size_t count) {
    auto &c=*static_cast<JpegContext*>(decoder->device);
    if(count>c.size-c.pos) count=c.size-c.pos;
    if(dest) memcpy(dest,c.data+c.pos,count);
    c.pos+=count;return count;
}
int jpeg_write(JDEC *decoder,void *bitmap,JRECT *area) {
    auto &c=*static_cast<JpegContext*>(decoder->device);
    const auto *pixels=static_cast<const uint8_t*>(bitmap);
    unsigned rw=area->right-area->left+1;
    unsigned x0=(area->left*c.dw+c.sw-1)/c.sw;
    unsigned x1=((area->right+1)*c.dw+c.sw-1)/c.sw;
    unsigned y0=(area->top*c.dh+c.sh-1)/c.sh;
    unsigned y1=((area->bottom+1)*c.dh+c.sh-1)/c.sh;
    for(unsigned y=y0;y<y1 && y<c.dh;++y) for(unsigned x=x0;x<x1 && x<c.dw;++x) {
        unsigned sx=x*c.sw/c.dw,sy=y*c.sh/c.dh;
        const auto *p=pixels+((sy-area->top)*rw+(sx-area->left))*3;
        c.output[(c.oy+y)*c.side+c.ox+x]=rgb(p[0],p[1],p[2]);
    }
#ifndef ORBIT_HOST_TEST
    // Let Wi-Fi and commands run between decoder blocks.
    if((area->top&63)==0) vTaskDelay(1);
#endif
    return 1;
}
void fit(unsigned w,unsigned h,unsigned target,unsigned &dw,unsigned &dh) {
    unsigned side=w>h?w:h;
    dw=w*target/side;dh=h*target/side;
    if(!dw) dw=1;if(!dh) dh=1;
}
bool jpeg(const uint8_t *data,size_t size,uint16_t *out,unsigned target) {
    JpegContext context;context.data=data;context.size=size;context.output=out;context.side=target;
    // TJpgDec uses only this bounded work area and the tiny output bitmap.
    void *work=allocate(16384);
    if(!work) return false;
    JDEC decoder={};
    bool ok=jd_prepare(&decoder,jpeg_read,work,16384,&context)==JDR_OK;
    if(ok) ok=decoder.width>0 && decoder.height>0 && decoder.width<=1024 && decoder.height<=1024;
    if(ok) {
        unsigned scale=0,side=decoder.width>decoder.height?decoder.width:decoder.height;
        // Retain at least the requested detail before fitting the output.
        while(scale<3 && (side>>(scale+1))>=target) ++scale;
        context.sw=decoder.width>>scale;context.sh=decoder.height>>scale;
        if(!context.sw || !context.sh) ok=false;
        else {
            fit(context.sw,context.sh,target,context.dw,context.dh);
            context.ox=(target-context.dw)/2;context.oy=(target-context.dh)/2;
            ok=jd_decomp(&decoder,jpeg_write,scale)==JDR_OK;
        }
    }
    free(work);return ok;
}
bool png(const uint8_t *data,size_t size,uint16_t *out,unsigned target) {
    if(!png_headroom()) return false;
    LodePNGState state;lodepng_state_init(&state);
    // Do not decompress arbitrary ancillary text or retain unknown chunks.
    state.decoder.read_text_chunks=0;state.decoder.remember_unknown_chunks=0;
    unsigned w=0,h=0;
    bool ok=lodepng_inspect(&w,&h,&state,data,size)==0;
    ok=ok && w>0 && h>0 && w<=512 && h<=512 && state.info_png.color.bitdepth<=8 && state.info_png.interlace_method==0;
    unsigned char *pixels=nullptr;
    if(ok) {
        state.decoder.zlibsettings.max_output_size=(size_t(w)*4+1)*h;
        ok=lodepng_decode(&pixels,&w,&h,&state,data,size)==0;
    }
    if(ok) {
        unsigned dw,dh;fit(w,h,target,dw,dh);
        unsigned ox=(target-dw)/2,oy=(target-dh)/2;
        for(unsigned y=0;y<dh;++y) for(unsigned x=0;x<dw;++x) {
            auto *p=pixels+((y*h/dh)*w+x*w/dw)*4;
            unsigned a=p[3];
            out[(oy+y)*target+ox+x]=rgb((p[0]*a+13*(255-a))/255,
                (p[1]*a+21*(255-a))/255,(p[2]*a+21*(255-a))/255);
        }
    }
    lv_mem_free(pixels);lodepng_state_cleanup(&state);return ok;
}
}
uint8_t *decode_artwork(const uint8_t *data,size_t size,unsigned side) {
    if(side!=ArtworkSide && side!=ArtworkLargeSide)return nullptr;
    if(!data || size<8 || size>ArtworkDownloadLimit) return nullptr;
    bool isJpeg=data[0]==0xff && data[1]==0xd8;
    bool isPng=memcmp(data,"\x89PNG\r\n\x1a\n",8)==0;
    if(!isJpeg && !isPng) return nullptr;
    auto *output=static_cast<uint16_t*>(allocate(side*side*2));
    if(!output) return nullptr;
    for(unsigned i=0;i<side*side;++i) output[i]=rgb(13,21,21);
    bool ok=isJpeg?jpeg(data,size,output,side):png(data,size,output,side);
    if(!ok) {free(output);return nullptr;}
    return reinterpret_cast<uint8_t*>(output);
}
uint8_t *artwork_thumbnail(const uint8_t *large) {
    if(!large)return nullptr;
    auto *out=static_cast<lv_color_t*>(allocate(ArtworkBytes));
    if(!out)return nullptr;
    const auto *src=reinterpret_cast<const lv_color_t*>(large);
    for(unsigned y=0;y<ArtworkSide;++y)for(unsigned x=0;x<ArtworkSide;++x) {
        const unsigned sx=x*(ArtworkLargeSide-1)*256/(ArtworkSide-1),sy=y*(ArtworkLargeSide-1)*256/(ArtworkSide-1);
        const unsigned x0=sx>>8,y0=sy>>8,x1=x0+1<ArtworkLargeSide?x0+1:x0,y1=y0+1<ArtworkLargeSide?y0+1:y0;
        auto a=lv_color_mix(src[y0*ArtworkLargeSide+x0],src[y0*ArtworkLargeSide+x1],255-(sx&255));
        auto b=lv_color_mix(src[y1*ArtworkLargeSide+x0],src[y1*ArtworkLargeSide+x1],255-(sx&255));
        out[y*ArtworkSide+x]=lv_color_mix(a,b,255-(sy&255));
    }
    return reinterpret_cast<uint8_t*>(out);
}

}
