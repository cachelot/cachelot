#include <cachelot/common.h>
#include <server/compression.h>
#include <blosc.h>

namespace cachelot {

    namespace compression {

        const size_t COMPRESSION_OVERHEAD = BLOSC_MAX_OVERHEAD;

        void init() {
            blosc_init();
        }


        void cleanup() {
            blosc_destroy();
        }


        size_t compress(const unsigned level, slice src, void * dest) {
            debug_assert(0 <= level && level <= 9);
            debug_assert(src.length() > 0); debug_assert(dest != nullptr);
            return blosc_compress(level, 1, sizeof(char), src.length(), src.begin(), dest, src.length() + COMPRESSION_OVERHEAD);
        }


        size_t decompress(slice src, void * dest, size_t destsize) {
            debug_assert(src.length() > 0); debug_assert(dest != nullptr); debug_assert(destsize > 0);
            auto rc = blosc_decompress(src.begin(), dest, destsize);
            if (rc > 0) {
                return rc;
            } else {
                throw decompression_error();
            }
        }

    }

} // namespace cachelot

