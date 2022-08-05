//
//  main.cpp
//  images_op
//
//  Created by bianchui on 2022/7/29.
//

#include <iostream>
#include "../lib/libpng-1.6.37/png.h"
#include "../lib/giflib-5.2.1/gif_lib.h"
#include <unistd.h>
#include <memory>
#include <string>
#include <vector>

bool writePng(const char* name, int w, int h, const void* data) {
    FILE *fp = fopen(name, "wb");
    if (!fp) {
        return false;
    }
    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;
    bool success = false;
    while (1) {
        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

        if (nullptr == png_ptr) {
            break;
        }

        info_ptr = png_create_info_struct(png_ptr);
        if (nullptr == info_ptr) {
            break;
        }
        if (setjmp(png_jmpbuf(png_ptr))) {
            break;
        }
        png_init_io(png_ptr, fp);
        png_set_IHDR(png_ptr, info_ptr, w, h, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
        png_set_compression_level(png_ptr, 9);
        png_write_info(png_ptr, info_ptr);
        png_set_packing(png_ptr);

        png_const_bytep ptr = (png_const_bytep)data;
        for (int i = 0; i < h; ++i) {
            png_write_row(png_ptr, ptr + i * w * 4);
        }

        png_write_end(png_ptr, info_ptr);

        break;
    }

    if (png_ptr) {
        png_destroy_write_struct(&png_ptr, &info_ptr);
    }

    fclose(fp);
    
    return success;
}

struct RGBA {
    uint8_t r,g,b,a;
};

void printGIFError(const char* name, int err) {
    printf("gif: %s: error: %d(%s)", name, err, GifErrorString(err));
}

void saveSubImage(const char* name, int i, int width, int height, const void* image) {
    std::string newName = name;
    char buf[32];
    sprintf(buf, "%d", i);
    newName.append(buf);
    newName.append(".png");
    
    writePng(newName.c_str(), width, height, image);
}

static const GifColorType kGIFWhite = {
    .Red = 255,
    .Green = 255,
    .Blue = 255,
};

static const GifColorType kGIFBlack = {
    .Red = 255,
    .Green = 255,
    .Blue = 255,
};

static GifColorType getBGColor(GifFileType* GifFile, const SavedImage& img) {
    auto bgColorIndex = GifFile->SBackGroundColor;
    auto* ColorMap = img.ImageDesc.ColorMap;
    if (ColorMap) {
        if (bgColorIndex < ColorMap->ColorCount) {
            return ColorMap->Colors[bgColorIndex];
        }
        fprintf(stderr, "Background color out of range for colormap\n");
        return kGIFWhite;
    }
    
    if (GifFile->SColorMap && bgColorIndex < GifFile->SColorMap->ColorCount) {
        return GifFile->SColorMap->Colors[bgColorIndex];
    }
    fprintf(stderr, "Background color out of range for colormap\n");

    return kGIFWhite;
}

// https://docstore.mik.ua/orelly/web2/wdesign/ch23_05.htm
void saveGIFFrames(GifFileType* GifFile, const char* name) {
    std::vector<std::unique_ptr<RGBA[]>> images;
    const int width = GifFile->SWidth, height = GifFile->SHeight;
    int doNotDispose = -1;
    for (int srcI = 0, dstI = 0; srcI < GifFile->ImageCount; ++srcI) {
        const auto& src = GifFile->SavedImages[srcI];
        
        /* Lets dump it - set the global variables required and do it: */
        const auto ColorMap = (src.ImageDesc.ColorMap ? src.ImageDesc.ColorMap : GifFile->SColorMap);
        if (ColorMap == NULL) {
            fprintf(stderr, "Gif Image does not have a colormap\n");
            continue;
        }
        
        RGBA* image;
        bool imgPrepared = false;
        if (dstI < images.size()) {
            image = images[dstI].get();
            saveSubImage(name, dstI + 100, width, height, image);
            imgPrepared = true;
        } else {
            image = new RGBA[width * height];
            images.push_back(std::unique_ptr<RGBA[]>(image));
        }
        ++dstI;
        
        GraphicsControlBlock gcb;
        const bool hasGCB = DGifSavedExtensionToGCB(GifFile, srcI, &gcb);
        const int transparentColor = hasGCB ? gcb.TransparentColor : NO_TRANSPARENT_COLOR;

        const GifColorType bgColor = getBGColor(GifFile, src);
        const GifByteType bgColorA = transparentColor != GifFile->SBackGroundColor ? 255 : 0;

        const int startX = std::min(src.ImageDesc.Left, width);
        const int endX = std::min(startX + src.ImageDesc.Width, width);
        const int startY = std::min(src.ImageDesc.Top, height);
        const int endY = std::min(startY + src.ImageDesc.Height, height);

        const RGBA bgRGBA = {
            .r = bgColor.Red,
            .g = bgColor.Green,
            .b = bgColor.Blue,
            .a = bgColorA,
        };
 
        if (!imgPrepared) {
            auto line0 = image;
            for (int y = 0; y < startY; ++y) {
                auto line = image + y * width;
                if (y != 0) {
                    memcpy(line, line0, width * sizeof(RGBA));
                    continue;
                }
                for (int x = 0; x < width; x++) {
                    line[x] = bgRGBA;
                }
            }
        }
        for (int y = 0, subheight = endY - startY; y < subheight; ++y) {
            auto line = image + (y + startY) * width;
            if (!imgPrepared) {
                for (int x = 0; x < startX; ++x) {
                    line[x] = bgRGBA;
                }
            }
            auto subLine = line + startX;
            auto gifLine = src.RasterBits + y * src.ImageDesc.Width;
            for (int x = 0, subWidth = endX - startX; x < subWidth; ++x) {
                const auto colorIndex = gifLine[x];
                if (colorIndex == transparentColor && imgPrepared) {
                    continue;
                }
                auto* pixel = subLine + x;
                const auto* color = ColorMap->Colors + colorIndex;
                pixel->r = color->Red;
                pixel->g = color->Green;
                pixel->b = color->Blue;
                pixel->a = colorIndex == transparentColor ? 0 : 255;
            }
            if (!imgPrepared) {
                for (int x = endX; x < width; ++x) {
                    line[x] = bgRGBA;
                }
            }
        }
        if (!imgPrepared) {
            auto line0 = image + endY * width;
            for (int y = endY; y < height; ++y) {
                auto line = image + y * width;
                if (y != endY) {
                    memcpy(line, line0, width * sizeof(RGBA));
                    continue;
                }
                for (int x = 0; x < width; x++) {
                    line[x] = bgRGBA;
                }
            }
        }
        
        saveSubImage(name, dstI, width, height, image);
                
        if (srcI >= GifFile->ImageCount) {
            break;
        }

        const int disposalMode = hasGCB ? gcb.DisposalMode : DISPOSAL_UNSPECIFIED;

        RGBA* fromImage = image;
        bool needCopy = false;
        switch (disposalMode) {
            case DISPOSE_DO_NOT:
                printf("%d: DISPOSE_DO_NOT\n", srcI);
                doNotDispose = (int)dstI - 1;
                needCopy = true;
                break;
            case DISPOSE_BACKGROUND:
                printf("%d: DISPOSE_BACKGROUND\n", srcI);
                needCopy = true;
                break;
            case DISPOSE_PREVIOUS:
                printf("%d: DISPOSE_PREVIOUS\n", srcI);
                needCopy = true;
                if (doNotDispose >= 0) {
                    fromImage = images[doNotDispose].get();
                }
                break;
            case DISPOSAL_UNSPECIFIED:
                printf("%d: DISPOSAL_UNSPECIFIED\n", srcI);
                break;
            default:
                printf("%d: DISPOSAL_UNKNOWN\n", srcI);
                break;
        }

        if (needCopy) {
            // copy to next image
            RGBA* nextImage = new RGBA[width * height];
            images.push_back(std::unique_ptr<RGBA[]>(nextImage));
            memcpy(nextImage, fromImage, width * height * sizeof(RGBA));
            if (disposalMode == DISPOSE_BACKGROUND) {
                for (int y = 0, subheight = endY - startY; y < subheight; ++y) {
                    auto line = nextImage + (y + startY) * width;
                    auto subLine = line + startX;
                    for (int x = 0, subWidth = endX - startX; x < subWidth; ++x) {
                        subLine[x] = bgRGBA;
                    }
                }
            }
        }
    }
    if (true) {
        printf("done.");
    }
}

bool readGIF(const char* name) {
    printf("%s\n", name);
    int Error;
    GifFileType* GifFile = DGifOpenFileName(name, &Error);
    if (!GifFile) {
        printGIFError("open", Error);
        return false;
    }
    printf("w: %d, h: %d\n", GifFile->SWidth, GifFile->SHeight);
    if (GifFile->SHeight == 0 || GifFile->SWidth == 0) {
        fprintf(stderr, "Image of width or height 0\n");
        return false;
    }
    Error = DGifSlurp(GifFile);
    if (Error == GIF_OK) {
        saveGIFFrames(GifFile, name);
        for (int i = 0; i < GifFile->ImageCount; ++i) {
            
        }
    } else {
        printGIFError("slurp", Error);
    }
    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
        printGIFError("close", Error);
    }
    return true;
}

int main(int argc, const char * argv[]) {
    RGBA data[4*4];
    
    for (int i = 0; i < 16; ++i) {
        data[i].r = 255;
        data[i].g = 0;
        data[i].b = 0;
        data[i].a = i * 255 / 15;
    }
    
    char cwd[1024];
    printf("cwd: %s\n", getcwd(cwd, 1024));
    //writePng("test.png", 4, 4, data);
    
    //readGIF("1.gif");
    //readGIF("2.gif");
    //readGIF("3.gif");
    //readGIF("4.gif");
    
    // test images from https://legacy.imagemagick.org/Usage/anim_basics/
    readGIF("img/anim_bgnd.gif");
    readGIF("img/anim_none.gif");
    readGIF("img/canvas_bgnd.gif");
    readGIF("img/canvas_none.gif");
    readGIF("img/canvas_prev.gif");

    return 0;
}
