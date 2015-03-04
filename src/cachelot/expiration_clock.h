#ifndef CACHELOT_EXPIRATION_CLOCK_H_INCLUDED
#define CACHELOT_EXPIRATION_CLOCK_H_INCLUDED

#include <chrono>   // std::chrono

/// @ingroup cache
/// @{

namespace cachelot {

    namespace cache {

        typedef std::chrono::duration<uint32> seconds;

        /// Simple std::chrono based monotonic clock to count item expiration time in secons
        /// Custom clock type allows to use uint32 time_point
        class ExpirationClock {
        public:
            typedef seconds duration;
            typedef duration::rep rep;
            typedef duration::period period;
            typedef std::chrono::time_point<ExpirationClock, seconds> time_point;
            static constexpr bool is_steady = true;

            static time_point now() noexcept {
                const auto now = std::chrono::steady_clock::now();
                return time_point(std::chrono::duration_cast<duration>(now.time_since_epoch()));
            }
       };


    } // namespace cache

} // namespace cachelot

/// @}

#endif // CACHELOT_EXPIRATION_CLOCK_H_INCLUDED
