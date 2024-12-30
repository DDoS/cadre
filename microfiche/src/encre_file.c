#include "encre_file.h"

#include <pico/platform/compiler.h>

#include <stdint.h>
#include <string.h>

static uint8_t encre_magic[ENCRE_MAGIC_SIZE] = "encre";

void begin_encre_file(struct encre_file *file) {
    file->offset = 0;
    file->read_header = false;
    file->read_palette = false;
    file->read_colors = false;
}

bool continue_encre_file(struct encre_file *file, struct pbuf *buffer) {
    static const size_t header_size = sizeof(file->header);
    static const size_t palette_size = sizeof(file->palette);
    static const size_t colors_size = sizeof(file->colors);
    static const size_t file_size = header_size + palette_size + colors_size;

    if (file->offset >= file_size) {
        pbuf_free(buffer);
        return false;
    }

    for (u16_t chunk_size; file->offset < file_size; file->offset += chunk_size) {
        const size_t remaining_file_size = file_size - file->offset;
        chunk_size = MIN(4096, remaining_file_size);
        chunk_size = pbuf_copy_partial(buffer, (void*)file + file->offset, chunk_size, 0);
        if (!chunk_size) {
            break;
        }

        buffer = pbuf_free_header(buffer, chunk_size);
    }

    if (buffer) {
        pbuf_free(buffer);
        buffer = NULL;
    }

    if (!file->read_header && file->offset >= header_size) {
        file->read_header = true;
        if (memcmp(file->header.magic, encre_magic, ENCRE_MAGIC_SIZE) != 0 ||
                file->header.bits_per_color != ENCRE_BITS_PER_COLOR ||
                file->header.palette_size != ENCRE_PALETTE_SIZE ||
                file->header.width != ENCRE_WIDTH || file->header.height != ENCRE_HEIGHT) {
            return false;
        }
    }

    if (!file->read_palette && file->offset >= header_size + palette_size) {
        file->read_palette = true;
    }

    if (!file->read_colors && file->offset == header_size + palette_size + colors_size) {
        file->read_colors = true;
    }

    return true;
}
