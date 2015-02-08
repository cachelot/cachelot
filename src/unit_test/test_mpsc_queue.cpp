#include "unit_test.h"
#include <cachelot/mpsc_queue.h>
#include <thread>

using namespace cachelot;

namespace {
	static const ulong NumThreads = os::thread::hardware_concurrency() * 2;
	static const ulong NumItems = 1024;
	static const ulong NumIterations = 1024 * 1024;
	typedef cachelot::concurrency::concurrent_queue<ulong> test_queue_type;
	static test_queue_type in_q, out_q;
	static test_queue_type intermediate_q;

    struct QItem : public mpsc_queue<QItem>::node {
        uint64 data;
        QItem() = default;
    };

    void test_queue_fun() {
        for (ulong iter = 0; iter < NumIterations; ++iter) {
            ulong * i = in_q.pop();
            if (i) {
                intermediate_q.push(i);
            }
            i = intermediate_q.pop();
            if (i) {
                out_q.push(i);
            }
            i = out_q.pop();
            if (i) {
                in_q.push(i);
            }
            std::this_thread::yield();
        }
    }

    inline ulong q_size(test_queue_type & q) {
        ulong result = 0;
        while (q.pop()) {
            result += 1;
        }
        return result;
    }

    BOOST_AUTO_TEST_SUITE(test_concurrent_queue)

    BOOST_AUTO_TEST_CASE(test_empty)
    {
        test_queue_type empty_q;
        BOOST_CHECK( empty_q.empty() );
        ulong * i = empty_q.pop();
        BOOST_CHECK( i == nullptr );
    }

    BOOST_AUTO_TEST_CASE(test_stress)
    {
        BOOST_TEST_MESSAGE("concurrent_queue stress test with: " << NumItems << " items " << "\n"
                        << "                                   " << NumThreads << " threads " << "\n"
                        << "                                   " << NumIterations << " iterations " << "\n");
        BOOST_TEST_MESSAGE("May take a while...");
        std::unique_ptr<ulong[]> ulongs(new ulong[NumItems]);
        for (ulong itemNo = 0; itemNo < NumItems; ++ itemNo) {
            in_q.push(&ulongs[itemNo]);
        }
        std::list<std::thread> threads;
        for (ulong threadNo = 0; threadNo < NumThreads; ++threadNo) {
            threads.emplace_back(std::thread(&test_queue_fun));
        }
        for (std::thread& th : threads) {
            th.join();
        }
        ulong result_num_items = q_size(in_q) + q_size(intermediate_q) + q_size(out_q);
        BOOST_CHECK(result_num_items == NumItems);
    }

    BOOST_AUTO_TEST_SUITE_END()

    }
