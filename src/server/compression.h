#ifndef CACHELOT_COMPRESSION_H_INCLUDED
#define CACHELOT_COMPRESSION_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#ifndef CACHELOT_BYTES_H_INCLUDED
#  include <cachelot/slice.h>
#endif


/// @defgroup compression
/// @{

namespace cachelot {

    namespace compression {

        extern const size_t COMPRESSION_OVERHEAD;

        class decompression_error : public std::runtime_error {
        public:
            decompression_error() : std::runtime_error("Failed to decompress") {}
        };

        /// Initialize compression prior to the first usage
        void init();

        /// De-initialize internal compressor environment
        void cleanup();

        /**
         * compress memory range and return compressed data size
         *
         * @param level - compression level from 0 (no compression) to the 9 (maximum compression)
         * @param src - source memory range to compress
         * @param dest - pointer to the destination buffer, caller is responsible for providing buffer of length(src) + COMPRESSION_OVERHEAD
         */
        size_t compress(const unsigned level, slice src, void * dest);


        /**
         * decompress previously compressed memory range and return its size
         *
         * @param src - source memory range to decompress
         * @param dest - pointer to the destination buffer
         * @param destsize - size of the destination buffer in bytes
         *
         * @note `src` and `dest` buffers must not overlap
         */
        size_t decompress(slice src, void * dest, size_t destsize);

    }

} // namespace cachelot

/// @}

#endif // CACHELOT_COMPRESSION_H_INCLUDED
