#include "../include/zlib_utils.h"
#include <zlib.h>
#include <span>


bool decompressZlib(std::span<const std::byte> input, std::vector<std::byte>& output) {
    // Start with a reasonable guess for the output size.
    // Git objects often have good compression, so 3x is a safe starting point.
    size_t output_size = input.size() * 3;
    if (output_size < 1024) output_size = 1024; // Minimum buffer size
    output.resize(output_size);

    while (true) {
        uLongf destLen = output.size();
        int res = uncompress(
            reinterpret_cast<Bytef*>(output.data()), &destLen,
            reinterpret_cast<const Bytef*>(input.data()), input.size()
        );

        if (res == Z_OK) {
            output.resize(destLen); // Shrink buffer to actual decompressed size
            return true;
        }

        if (res == Z_BUF_ERROR) {
            // Output buffer was too small. Double it and try again.
            if (output.size() > 10 * 1024 * 1024) return false; // Safety break for huge files
            output.resize(output.size() * 2);
        } else {
            // Some other zlib error occurred
            return false;
        }
    }
}

bool compressZlib(std::span<const std::byte> input, std::vector<std::byte>& output) {
    uLong sourceLen = input.size();
    uLong destLen = compressBound(sourceLen);
    output.resize(destLen);

    int result = compress(
        reinterpret_cast<Bytef*>(output.data()), &destLen,
        reinterpret_cast<const Bytef*>(input.data()), sourceLen
    );

    if (result != Z_OK) {
        return false;
    }

    output.resize(destLen); // Shrink buffer to actual compressed size
    return true;
}