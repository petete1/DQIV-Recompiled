#include "TextCanvas.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include <stb_truetype.h>
#include <cstdio>
#include <fstream>
#include <memory>
#endif
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace pc {
namespace {
#ifdef _WIN32
std::wstring wide(std::string_view utf8) {
    if(utf8.empty())return {};
    const int count=MultiByteToWideChar(CP_UTF8,0,utf8.data(),int(utf8.size()),nullptr,0);
    std::wstring result(std::size_t(count),L'\0');
    MultiByteToWideChar(CP_UTF8,0,utf8.data(),int(utf8.size()),result.data(),count);
    return result;
}
#else
// The system's sans-serif font (fontconfig's choice, else a common one), then
// one with Japanese for the characters it lacks.
struct Font {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
};
std::string fontMatch(const char* pattern) {
    std::string command=std::string("fc-match -f '%{file}' '")+pattern+"' 2>/dev/null",result;
    if(FILE* pipe=popen(command.c_str(),"r")) {
        char buffer[512];
        while(const auto count=std::fread(buffer,1,sizeof buffer,pipe))result.append(buffer,count);
        pclose(pipe);
    }
    return result;
}
std::unique_ptr<Font> loadFont(std::initializer_list<std::string> candidates) {
    for(const auto& path:candidates) {
        if(path.empty())continue;
        std::ifstream file(path,std::ios::binary);
        auto font=std::make_unique<Font>();
        font->data.assign(std::istreambuf_iterator<char>(file),{});
        const int offset=stbtt_GetFontOffsetForIndex(font->data.data(),0);
        if(!font->data.empty()&&offset>=0&&stbtt_InitFont(&font->info,font->data.data(),offset))return font;
    }
    return nullptr;
}
const std::vector<std::unique_ptr<Font>>& fonts(bool bold) {
    static const auto load=[](bool bold) {
        std::vector<std::unique_ptr<Font>> chain;
        if(auto font=bold?loadFont({fontMatch("sans-serif:bold"),"/usr/share/fonts/TTF/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf","/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf",
            "/usr/share/fonts/noto/NotoSans-Bold.ttf","/usr/share/fonts/liberation/LiberationSans-Bold.ttf"}):
            loadFont({fontMatch("sans-serif"),"/usr/share/fonts/TTF/DejaVuSans.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf","/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
            "/usr/share/fonts/noto/NotoSans-Regular.ttf","/usr/share/fonts/liberation/LiberationSans-Regular.ttf"}))
            chain.push_back(std::move(font));
        if(auto font=loadFont({fontMatch(bold?"sans-serif:lang=ja:bold":"sans-serif:lang=ja")}))chain.push_back(std::move(font));
        if(chain.empty())throw std::runtime_error("Interface text: no system font found (install fontconfig and a TrueType font)");
        return chain;
    };
    static const auto regular=load(false),strong=load(true);
    return bold?strong:regular;
}
std::u32string codepoints(std::string_view utf8) {
    std::u32string result;
    for(std::size_t i=0;i<utf8.size();) {
        const unsigned char lead=utf8[i];
        const int length=lead<0x80?1:lead<0xe0?2:lead<0xf0?3:4;
        char32_t value=length==1?lead:lead&(0x7f>>length);
        for(int k=1;k<length&&i+k<utf8.size();++k)value=(value<<6)|(utf8[i+k]&0x3f);
        result.push_back(value);i+=length;
    }
    return result;
}
#endif
std::uint32_t blend(std::uint32_t below,std::uint32_t argb,unsigned coverage) {
    // Straight colour over a premultiplied pixel, with coverage 0..255.
    const unsigned a=((argb>>24)*coverage+127)/255;
    std::uint32_t result=0;
    for(unsigned shift:{0u,8u,16u}) {
        const unsigned c=((argb>>shift)&255)*a/255,b=(below>>shift)&255;
        result|=std::min(255u,c+b*(255-a)/255)<<shift;
    }
    const unsigned alpha=std::min(255u,a+((below>>24)&255)*(255-a)/255);
    return result|(alpha<<24);
}
}
TextCanvas::TextCanvas(int width,int height):width_(std::max(1,width)),height_(std::max(1,height)),
    pixels_(std::size_t(width_)*height_) {
#ifndef _WIN32
    mask_=new std::uint32_t[pixels_.size()]();
#else
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(info.bmiHeader);info.bmiHeader.biWidth=width_;info.bmiHeader.biHeight=-height_;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    dc_=CreateCompatibleDC(nullptr);
    void* bits=nullptr;
    bitmap_=CreateDIBSection(static_cast<HDC>(dc_),&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!dc_||!bitmap_)throw std::runtime_error("Interface text surface unavailable");
    mask_=static_cast<std::uint32_t*>(bits);
    previous_=SelectObject(static_cast<HDC>(dc_),static_cast<HBITMAP>(bitmap_));
    SetBkMode(static_cast<HDC>(dc_),TRANSPARENT);SetTextColor(static_cast<HDC>(dc_),RGB(255,255,255));
#endif
}
TextCanvas::~TextCanvas() {
#ifndef _WIN32
    delete[] mask_;
#else
    SelectObject(static_cast<HDC>(dc_),static_cast<HGDIOBJ>(previous_));
    DeleteObject(static_cast<HBITMAP>(bitmap_));DeleteDC(static_cast<HDC>(dc_));
#endif
}
void TextCanvas::clear(std::uint32_t argb) {
    const unsigned a=argb>>24;
    std::uint32_t premultiplied=a<<24;
    for(unsigned shift:{0u,8u,16u})premultiplied|=(((argb>>shift)&255)*a/255)<<shift;
    std::fill(pixels_.begin(),pixels_.end(),premultiplied);
}
void TextCanvas::fill(int x,int y,int width,int height,std::uint32_t argb) {
    const int x0=std::max(0,x),y0=std::max(0,y),x1=std::min(width_,x+width),y1=std::min(height_,y+height);
    for(int row=y0;row<y1;++row)for(int column=x0;column<x1;++column) {
        auto& p=pixels_[std::size_t(row)*width_+column];p=blend(p,argb,255);
    }
}
void TextCanvas::text(int x,int y,std::string_view utf8,int size,std::uint32_t argb,bool bold,int align,int boxWidth) {
#ifndef _WIN32
    const auto string=codepoints(utf8);
    if(string.empty())return;
    struct Rect {int left,top,right,bottom;} rect{x,y,boxWidth>0?x+boxWidth:width_,std::min(height_,y+size*2)};
    for(int row=std::max(0,rect.top);row<std::min(height_,rect.bottom);++row)
        std::fill_n(mask_+std::size_t(row)*width_+std::max(0,rect.left),std::max(0,std::min(width_,rect.right)-std::max(0,rect.left)),0u);
    // Each character in the first font that has it, `size` pixels per em as GDI's.
    const auto& chain=fonts(bold);
    struct Glyph {const Font* font;int index;float scale;};
    std::vector<Glyph> glyphs;float width=0;
    for(std::size_t i=0;i<string.size();++i) {
        Glyph glyph{chain.front().get(),0,0};
        for(const auto& font:chain)
            if(const int index=stbtt_FindGlyphIndex(&font->info,int(string[i]))) {glyph={font.get(),index,0};break;}
        if(!glyph.index)glyph.index=stbtt_FindGlyphIndex(&glyph.font->info,int(string[i]));
        glyph.scale=stbtt_ScaleForMappingEmToPixels(&glyph.font->info,float(size));
        if(!glyphs.empty()&&glyphs.back().font==glyph.font)
            width+=glyph.scale*float(stbtt_GetGlyphKernAdvance(&glyph.font->info,glyphs.back().index,glyph.index));
        int advance=0,bearing=0;stbtt_GetGlyphHMetrics(&glyph.font->info,glyph.index,&advance,&bearing);
        width+=glyph.scale*float(advance);glyphs.push_back(glyph);
    }
    const auto& primary=*chain.front();
    int ascent=0,descent=0,gap=0;stbtt_GetFontVMetrics(&primary.info,&ascent,&descent,&gap);
    const float baseline=float(rect.top)+float(ascent)*stbtt_ScaleForMappingEmToPixels(&primary.info,float(size));
    float pen=align==1?float(rect.left)+(float(rect.right-rect.left)-width)/2:align==2?float(rect.right)-width:float(rect.left);
    std::vector<unsigned char> bitmap;
    for(std::size_t i=0;i<glyphs.size();++i) {
        const auto& glyph=glyphs[i];const auto* info=&glyph.font->info;
        if(i&&glyphs[i-1].font==glyph.font)pen+=glyph.scale*float(stbtt_GetGlyphKernAdvance(info,glyphs[i-1].index,glyph.index));
        const int origin=int(std::floor(pen));const float shift=pen-float(origin);
        int x0,y0,x1,y1;stbtt_GetGlyphBitmapBoxSubpixel(info,glyph.index,glyph.scale,glyph.scale,shift,0,&x0,&y0,&x1,&y1);
        if(x1>x0&&y1>y0) {
            bitmap.assign(std::size_t(x1-x0)*(y1-y0),0);
            stbtt_MakeGlyphBitmapSubpixel(info,bitmap.data(),x1-x0,y1-y0,x1-x0,glyph.scale,glyph.scale,shift,0,glyph.index);
            const int left=origin+x0,top=int(std::lround(baseline))+y0;
            for(int row=std::max({0,top,rect.top});row<std::min({height_,rect.bottom,top+y1-y0});++row)
                for(int column=std::max({0,left,rect.left});column<std::min({width_,rect.right,left+x1-x0});++column) {
                    const std::uint32_t coverage=bitmap[std::size_t(row-top)*(x1-x0)+(column-left)];
                    auto& m=mask_[std::size_t(row)*width_+column];
                    if(coverage>(m&255))m=coverage|(coverage<<8)|(coverage<<16);
                }
        }
        int advance=0,bearing=0;stbtt_GetGlyphHMetrics(info,glyph.index,&advance,&bearing);
        pen+=glyph.scale*float(advance);
    }
#else
    const auto string=wide(utf8);
    if(string.empty())return;
    HDC dc=static_cast<HDC>(dc_);
    HFONT font=CreateFontW(-size,0,0,0,bold?FW_BOLD:FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    HGDIOBJ previous=SelectObject(dc,font);
    RECT rect{x,y,boxWidth>0?x+boxWidth:width_,std::min(height_,y+size*2)};
    // The text pass: white on black in the mask, then the colour where it covers.
    for(int row=std::max(0,int(rect.top));row<std::min(height_,int(rect.bottom));++row)
        std::fill_n(mask_+std::size_t(row)*width_+std::max(0,int(rect.left)),std::max(0,std::min(width_,int(rect.right))-std::max(0,int(rect.left))),0u);
    GdiFlush();
    const UINT format=DT_SINGLELINE|DT_NOPREFIX|DT_TOP|DT_END_ELLIPSIS|(align==1?DT_CENTER:align==2?DT_RIGHT:DT_LEFT);
    DrawTextW(dc,string.c_str(),int(string.size()),&rect,format);
    GdiFlush();
    SelectObject(dc,previous);DeleteObject(font);
#endif
    for(int row=std::max(0,int(rect.top));row<std::min(height_,int(rect.bottom));++row)
        for(int column=std::max(0,int(rect.left));column<std::min(width_,int(rect.right));++column) {
            const auto m=mask_[std::size_t(row)*width_+column];
            const unsigned coverage=std::max({m&255,(m>>8)&255,(m>>16)&255});
            if(coverage){auto& p=pixels_[std::size_t(row)*width_+column];p=blend(p,argb,coverage);}
        }
}
void TextCanvas::image(int x,int y,int width,int height,const std::uint32_t* argb,int sourceWidth,int sourceHeight) {
    if(width<=0||height<=0||sourceWidth<=0||sourceHeight<=0)return;
    const auto at=[&](int column,int row) {
        return argb[std::size_t(std::clamp(row,0,sourceHeight-1))*sourceWidth+std::clamp(column,0,sourceWidth-1)];
    };
    for(int row=std::max(0,y);row<std::min(height_,y+height);++row)
        for(int column=std::max(0,x);column<std::min(width_,x+width);++column) {
            // Bilinear: the source point under the pixel's centre.
            const float u=(column-x+0.5f)*sourceWidth/width-0.5f,v=(row-y+0.5f)*sourceHeight/height-0.5f;
            const int u0=int(std::floor(u)),v0=int(std::floor(v));
            const float fu=u-u0,fv=v-v0;
            const std::uint32_t corners[4]={at(u0,v0),at(u0+1,v0),at(u0,v0+1),at(u0+1,v0+1)};
            const float weights[4]={(1-fu)*(1-fv),fu*(1-fv),(1-fu)*fv,fu*fv};
            float alpha=0,channels[3]={};
            for(int i=0;i<4;++i) {
                const float a=float(corners[i]>>24)*weights[i];
                alpha+=a;
                for(int c=0;c<3;++c)channels[c]+=float((corners[i]>>(c*8))&255)*a;
            }
            if(alpha<=0)continue;
            std::uint32_t colour=0;
            for(int c=0;c<3;++c)colour|=std::uint32_t(std::lround(channels[c]/alpha))<<(c*8);
            auto& p=pixels_[std::size_t(row)*width_+column];
            p=blend(p,colour|0xff000000u,unsigned(std::lround(alpha)));
        }
}
}
