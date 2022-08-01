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

bool readGIF(const char* name) {
    int Error;
    GifFileType* GifFile = DGifOpenFileName(name, &Error);
    if (!GifFile) {
        printGIFError("open", Error);
        return false;
    }
    GifRowType* ScreenBuffer = nullptr;
    bool success = readGIF_file(GifFile);
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
