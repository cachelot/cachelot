#include <cachelot/common.h>
#include <cachelot/random.h>
#include <iostream>
#include <iomanip>

extern "C" {
  #include <blosc.h>
}

static const auto REPEAT_COMPRESSION = 100u;

static const auto DATALEN_MIN = 1024;
static const auto DATALEN_MAX = 1024*1024;
static const auto DATALEN_INCREASE_RATIO = 2;

static const auto MIN_COMPRESSION_LVL = 1u;
static const auto MAX_COMPRESSION_LVL = 9u;


using namespace cachelot;
using std::cout;
using std::endl;
typedef std::chrono::high_resolution_clock hires_clock;


inline hires_clock::time_point benchmark_start() {
    return hires_clock::now();
}

inline std::chrono::nanoseconds::rep benchmark_measure_ns(hires_clock::time_point start_time) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(hires_clock::now() - start_time).count();
}



inline void test_compression(unsigned level, bool do_shuffle, const string data) {
    const size_t out_buffer_length = data.length() + BLOSC_MAX_OVERHEAD;
    std::unique_ptr<char[]> out_buffer(new char [out_buffer_length]);
    cout << "Compression: " << level << " shuffle: " << do_shuffle << " length: " << data.length() << endl;
    int comp_total_bytes = 0;
    auto start_time = benchmark_start();
    for (unsigned repeat_no = 0u; repeat_no < REPEAT_COMPRESSION; ++repeat_no) {
        auto rc = blosc_compress(level, (int)do_shuffle, sizeof(char), data.length(), data.c_str(), out_buffer.get(), out_buffer_length);
        comp_total_bytes += rc;
    }
    auto took_ns = benchmark_measure_ns(start_time);
    const int src_total_bytes = data.length() * REPEAT_COMPRESSION;

    cout << "saved bytes            : " << comp_total_bytes << "/" << src_total_bytes << endl;
    cout << "compression ratio      : " << src_total_bytes - comp_total_bytes << std::setw(10) << std::left << " b"
                                        << ((double)(src_total_bytes - comp_total_bytes) / src_total_bytes) * 100 << " %" << endl;
    cout << "compression took (ns)  : " << took_ns << endl;
    start_time = benchmark_start();
    for (unsigned repeat_no = 0u; repeat_no < REPEAT_COMPRESSION; ++repeat_no) {
        std::memcpy(out_buffer.get(), data.c_str(), data.length());
    }
    took_ns = benchmark_measure_ns(start_time);
    cout << "memcpy took (ns)       : " << took_ns << endl;
    cout << endl;
}

void test_compression_for_every_level(bool do_shuffle, const string data) {
    for (unsigned comp_level = MIN_COMPRESSION_LVL; comp_level <= MAX_COMPRESSION_LVL; ++comp_level) {
        test_compression(comp_level, do_shuffle, data);
    }
}



int main() {
    blosc_init();
    blosc_set_compressor("lz4");
    size_t datalen = DATALEN_MIN;
    while (datalen <= DATALEN_MAX) {
        string comp_data = random_string(datalen, datalen);
        test_compression_for_every_level(true, comp_data);
        test_compression_for_every_level(false, comp_data);
        if (datalen == DATALEN_MAX) {
            break;
        } else {
            datalen = std::min<size_t>(datalen * DATALEN_INCREASE_RATIO, DATALEN_MAX);
        }
    }

    blosc_destroy();
    return 0;
}
