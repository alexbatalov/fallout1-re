#include "game/artload.h"

#include "plib/gnw/memory.h"

static int art_readSubFrameData(unsigned char* data, DB_FILE* stream, int count);
static int art_readFrameData(Art* art, DB_FILE* stream);

// 0x4193A0
static int art_readSubFrameData(unsigned char* data, DB_FILE* stream, int count)
{
    unsigned char* ptr = data;
    for (int index = 0; index < count; index++) {
        ArtFrame* frame = (ArtFrame*)ptr;

        if (db_freadShort(stream, &(frame->width)) == -1) return -1;
        if (db_freadShort(stream, &(frame->height)) == -1) return -1;
        if (db_freadInt(stream, &(frame->size)) == -1) return -1;
        if (db_freadShort(stream, &(frame->x)) == -1) return -1;
        if (db_freadShort(stream, &(frame->y)) == -1) return -1;
        if (db_fread(ptr + sizeof(ArtFrame), frame->size, 1, stream) != 1) return -1;

        ptr += sizeof(ArtFrame) + frame->size;
    }

    return 0;
}

// 0x41945C
static int art_readFrameData(Art* art, DB_FILE* stream)
{
    if (db_freadInt(stream, &(art->field_0)) == -1) return -1;
    if (db_freadShort(stream, &(art->framesPerSecond)) == -1) return -1;
    if (db_freadShort(stream, &(art->actionFrame)) == -1) return -1;
    if (db_freadShort(stream, &(art->frameCount)) == -1) return -1;
    if (db_freadShortCount(stream, art->xOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_freadShortCount(stream, art->yOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_freadIntCount(stream, art->dataOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_freadInt(stream, &(art->field_3A)) == -1) return -1;

    return 0;
}

// 0x419500
int load_frame(const char* path, Art** artPtr)
{
    dir_entry de;
    DB_FILE* stream;
    int index;

    if (db_dir_entry(path, &de) == -1) {
        return -2;
    }

    *artPtr = (Art*)mem_malloc(de.length);
    if (*artPtr == NULL) {
        return -1;
    }

    stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -2;
    }

    if (art_readFrameData(*artPtr, stream) != 0) {
        db_fclose(stream);
        mem_free(*artPtr);
        return -3;
    }

    for (index = 0; index < ROTATION_COUNT; index++) {
        if (index == 0 || (*artPtr)->dataOffsets[index - 1] != (*artPtr)->dataOffsets[index]) {
            if (art_readSubFrameData((unsigned char*)(*artPtr) + sizeof(Art) + (*artPtr)->dataOffsets[index], stream, (*artPtr)->frameCount) != 0) {
                break;
            }
        }
    }

    if (index < ROTATION_COUNT) {
        db_fclose(stream);
        mem_free(*artPtr);
        return -5;
    }

    db_fclose(stream);

    return 0;
}

// 0x419600
int load_frame_into(const char* path, unsigned char* data)
{
    DB_FILE* stream = db_fopen(path, "rb");
    if (stream == NULL) {
        return -2;
    }

    Art* art = (Art*)data;
    if (art_readFrameData(art, stream) != 0) {
        db_fclose(stream);
        return -3;
    }

    for (int index = 0; index < ROTATION_COUNT; index++) {
        if (index == 0 || art->dataOffsets[index - 1] != art->dataOffsets[index]) {
            if (art_readSubFrameData(data + sizeof(Art) + art->dataOffsets[index], stream, art->frameCount) != 0) {
                db_fclose(stream);
                return -5;
            }
        }
    }

    db_fclose(stream);
    return 0;
}

// 0x4196B0
int art_writeSubFrameData(unsigned char* data, DB_FILE* stream, int count)
{
    unsigned char* ptr = data;
    for (int index = 0; index < count; index++) {
        ArtFrame* frame = (ArtFrame*)ptr;

        if (db_fwriteShort(stream, frame->width) == -1) return -1;
        if (db_fwriteShort(stream, frame->height) == -1) return -1;
        if (db_fwriteInt(stream, frame->size) == -1) return -1;
        if (db_fwriteShort(stream, frame->x) == -1) return -1;
        if (db_fwriteShort(stream, frame->y) == -1) return -1;
        if (db_fwrite(ptr + sizeof(ArtFrame), frame->size, 1, stream) != 1) return -1;

        ptr += sizeof(ArtFrame) + frame->size;
    }

    return 0;
}

// 0x419778
int art_writeFrameData(Art* art, DB_FILE* stream)
{
    if (db_fwriteInt(stream, art->field_0) == -1) return -1;
    if (db_fwriteShort(stream, art->framesPerSecond) == -1) return -1;
    if (db_fwriteShort(stream, art->actionFrame) == -1) return -1;
    if (db_fwriteShort(stream, art->frameCount) == -1) return -1;
    if (db_fwriteShortCount(stream, art->xOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_fwriteShortCount(stream, art->yOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_fwriteIntCount(stream, art->dataOffsets, ROTATION_COUNT) == -1) return -1;
    if (db_fwriteInt(stream, art->field_3A) == -1) return -1;

    return 0;
}

// 0x419828
int save_frame(const char* path, unsigned char* data)
{
    if (data == NULL) {
        return -1;
    }

    DB_FILE* stream = db_fopen(path, "wb");
    if (stream == NULL) {
        return -1;
    }

    Art* art = (Art*)data;
    if (art_writeFrameData(art, stream) == -1) {
        db_fclose(stream);
        return -1;
    }

    for (int index = 0; index < ROTATION_COUNT; index++) {
        if (index == 0 || art->dataOffsets[index - 1] != art->dataOffsets[index]) {
            if (art_writeSubFrameData(data + sizeof(Art) + art->dataOffsets[index], stream, art->frameCount) != 0) {
                db_fclose(stream);
                return -1;
            }
        }
    }

    db_fclose(stream);
    return 0;
}
