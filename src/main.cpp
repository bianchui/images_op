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

bool readGIF_file(GifFileType* GifFile) {
    printf("w: %d, h: %d\n", GifFile->SWidth, GifFile->SHeight);
    if (GifFile->SHeight == 0 || GifFile->SWidth == 0) {
        fprintf(stderr, "Image of width or height 0\n");
        return false;
    }
    int ImageNum = 0;
    GifRecordType RecordType;
    while (1) {
        if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
            printGIFError("record", GifFile->Error);
            return false;
        }
        printf("record: %d\n", RecordType);
        if (RecordType == TERMINATE_RECORD_TYPE) {
            break;
        }
        switch (RecordType) {
            case IMAGE_DESC_RECORD_TYPE:
                if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
                    printGIFError("desc", GifFile->Error);
                    return false;
                }
                ++ImageNum;
                printf("Image: (%d, %d), (%d, %d)\n", GifFile->Image.Top, GifFile->Image.Left, GifFile->Image.Width, GifFile->Image.Height);
                if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
                   GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
                    fprintf(stderr, "Image %d is not confined to screen dimension, aborted.\n", ImageNum);
                    return false;
                }
                if (GifFile->Image.Interlace) {
                    /* Need to perform 4 passes on the images: */
#if 0
                    for (Count = i = 0; i < 4; i++) {
                        for (j = Row + InterlacedOffset[i]; j < Row + Height;
                                     j += InterlacedJumps[i]) {
                            GifQprintf("\b\b\b\b%-4d", Count++);
                            if (DGifGetLine(GifFile, &ScreenBuffer[j][Col],
                            Width) == GIF_ERROR) {
                            PrintGifError(GifFile->Error);
                            exit(EXIT_FAILURE);
                            }
                        }
                    }
#endif//0
                } else {
#if 0
                    for (i = 0; i < Height; i++) {
                        GifQprintf("\b\b\b\b%-4d", i);
                        if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col],
                            Width) == GIF_ERROR) {
                            PrintGifError(GifFile->Error);
                            exit(EXIT_FAILURE);
                        }
                    }
#endif//0
                }
                break;
            case EXTENSION_RECORD_TYPE: {
                GifByteType *Extension;
                int ExtCode;
                /* Skip any extension blocks in file: */
                if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    printGIFError("extension", GifFile->Error);
                    return false;
                }
                while (Extension != NULL) {
                    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
                        printGIFError("extension next", GifFile->Error);
                        return false;
                    }
                }
                break; }
            case TERMINATE_RECORD_TYPE:
                break;
            default:            /* Should be trapped by DGifGetRecordType. */
                break;
        }
    }
    
    return true;
}

void saveGIFFrames(GifFileType* GifFile, const char* name) {
    std::unique_ptr<RGBA[]> image(new RGBA[GifFile->SWidth * GifFile->SHeight]);
    const int width = GifFile->SWidth, height = GifFile->SHeight;
    for (int i = 0; i < GifFile->ImageCount; ++i) {
        const auto& img = GifFile->SavedImages[i];
        
        /* Lets dump it - set the global variables required and do it: */
        auto ColorMap = (img.ImageDesc.ColorMap ? img.ImageDesc.ColorMap : GifFile->SColorMap);
        if (ColorMap == NULL) {
            fprintf(stderr, "Gif Image does not have a colormap\n");
            continue;
        }
        
        /* check that the background color isn't garbage (SF bug #87) */
        auto bgColorIndex = GifFile->SBackGroundColor;
        if (bgColorIndex < 0 || bgColorIndex >= ColorMap->ColorCount) {
            fprintf(stderr, "Background color out of range for colormap\n");
            bgColorIndex = 0;
        }
        
        GraphicsControlBlock gcb;
        const bool hasGCB = DGifSavedExtensionToGCB(GifFile, i, &gcb);
        const int transparentColor = hasGCB ? gcb.TransparentColor : NO_TRANSPARENT_COLOR;

        GifColorType bgColor = ColorMap->Colors[bgColorIndex];
        GifByteType bgColorA = transparentColor != bgColorIndex ? 255 : 0;
        int y = 0;
        for (; y < img.ImageDesc.Top; ++y) {
            auto line = image.get() + y * width;
            for (int x = 0; x < width; x++) {
                auto& rgba = line[x];
                rgba.r = bgColor.Red;
                rgba.g = bgColor.Green;
                rgba.b = bgColor.Blue;
                rgba.a = bgColorA;
            }
        }
        for (int y = 0; y < GifFile->SHeight; ++y) {
            for (int x = 0; x < )
            for (int x = 0; x < GifFile->SWidth ; x++) {
                ColorMapEntry = &ColorMap->Colors[GifRow[j]];
                *BufferP++ = ColorMapEntry->Red;
                *BufferP++ = ColorMapEntry->Green;
                *BufferP++ = ColorMapEntry->Blue;
            }
            if (fwrite(Buffer, ScreenWidth * 3, 1, rgbfp[0]) != 1)
                GIF_EXIT("Write to file(s) failed.");
        }
        for (; y < height; ++y) {
            auto line = image.get() + y * width;
            for (int x = 0; x < width; x++) {
                auto& rgba = line[x];
                rgba.r = bgColor.Red;
                rgba.g = bgColor.Green;
                rgba.b = bgColor.Blue;
                rgba.a = bgColorA;
            }
        }
        
        std::string newName = name;
        char buf[32];
        sprintf(buf, "%d", i);
        newName.append(buf);
        newName.append(".png");
        
        
    }
}

bool readGIF(const char* name) {
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
    writePng("test.png", 4, 4, data);
    
    readGIF("1.gif");
    
    return 0;
}
