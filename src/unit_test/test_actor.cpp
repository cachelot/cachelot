#include "unit_test.h"
#include <cachelot/actor.h>
#include <cachelot/bytes.h>
#include <queue>

using namespace cachelot;

namespace {

    /// [Simple calculator actors]
    // Actor produces operation for the calculator and checks results
    class CalcProducer : public Actor {
    public:
        CalcProducer(ActorThread & th, Actor & calc_actor_ref)
            : Actor(th, &CalcProducer::main)
            , my_thread(th)
            , calc_actor(calc_actor_ref)
            {}
    private:
        bool main() noexcept {
            switch (my_state) {
                case 0: {
                    send_message(calc_actor, atom("add"), 1678235, 871236);
                    break;
                }
                case 1: {
                    auto msg = receive_message();
                    BOOST_CHECK(msg);
                    BOOST_CHECK_EQUAL(msg->type, atom("ok"));
                    int res;
                    msg->unpack(res);
                    BOOST_CHECK_EQUAL(res, 1678235 + 871236);
                    break;
                }
                case 2: {
                    send_message(calc_actor, atom("add"), std::numeric_limits<int>::max(), 1);
                    break;
                }
                case 3: {
                    auto msg = receive_message();
                    BOOST_CHECK(msg);
                    BOOST_CHECK_EQUAL(msg->type, atom("error"));
                    atom_type err;
                    msg->unpack(err);
                    BOOST_CHECK_EQUAL(err, atom("overflow"));
                    break;
                }
                case 4: {
                    send_message(calc_actor, atom("sub"), 0, 1);
                    break;
                }
                case 5: {
                    auto msg = receive_message();
                    BOOST_CHECK(msg);
                    BOOST_CHECK_EQUAL(msg->type, atom("ok"));
                    int res;
                    msg->unpack(res);
                    BOOST_CHECK_EQUAL(res, -1);
                    break;
                }
                case 6: {
                    send_message(calc_actor, atom("sub"), std::numeric_limits<int>::min(), 1);
                    break;
                }
                case 7: {
                    auto msg = receive_message();
                    BOOST_CHECK(msg);
                    BOOST_CHECK_EQUAL(msg->type, atom("error"));
                    atom_type err;
                    msg->unpack(err);
                    BOOST_CHECK_EQUAL(err, atom("underflow"));
                    break;
                }
                case 8: {
                    send_message(calc_actor, atom("neg"), 1);
                    break;
                }
                case 9: {
                    auto msg = receive_message();
                    BOOST_CHECK(msg);
                    BOOST_CHECK_EQUAL(msg->type, atom("ok"));
                    int res;
                    msg->unpack(res);
                    BOOST_CHECK_EQUAL(res, -1);
                    break;
                }
                default: {
                    // terminate whole thread
                    my_thread.stop();
                }

            }
                    
            return (++my_state < 10);
        }

    private:
        ActorThread & my_thread;
        Actor & calc_actor;
        unsigned my_state = 0;
    };

    // Simple calculator actor which support add / sub / neg commands
    class CalcActor : public Actor {
    public:
        CalcActor(ActorThread & th)
            : Actor(th, &CalcActor::main) {
        }

    private:
        // on_message
        bool main() noexcept {
            auto msg = receive_message();
            if (not msg) return false;
            switch (msg->type) {
                case atom("add"): {
                    int a, b;
                    msg->unpack(a, b);
                    if (std::numeric_limits<int>::max() - a >= b) {
                        reply_to(std::move(msg), atom("ok"), a+b);
                    } else {
                        reply_to(std::move(msg), atom("error"), atom("overflow"));
                    }
                    break;
                }
                case atom("sub"): {
                    int a, b;
                    msg->unpack(a, b);
                    if (std::numeric_limits<int>::min() + b <= a) {
                        reply_to(std::move(msg), atom("ok"), a - b);
                    } else {
                        reply_to(std::move(msg), atom("error"), atom("underflow"));
                    }
                    break;
                }
                case atom("neg"): {
                    int a;
                    msg->unpack(a);
                    reply_to(std::move(msg), atom("ok"), -a);
                }
                default:
                    break;
            }
            return true;
        }
    };
    /// [Simple calculator actors]


BOOST_AUTO_TEST_SUITE(test_actor)

BOOST_AUTO_TEST_CASE(test_message_arguments) {
    Actor::Message test_message(*(Actor *)(nullptr), atom("test"), 1, 2, 3u, std::numeric_limits<uint64>::max(), bytes::from_literal("This is Test!!!!"));
    int _1, _2;
    unsigned _3;
    uint64 _4;
    bytes _5;
    test_message.unpack(_1, _2, _3, _4, _5);
    BOOST_CHECK_EQUAL(_1, 1);
    BOOST_CHECK_EQUAL(_2, 2);
    BOOST_CHECK_EQUAL(_3, 3u);
    BOOST_CHECK_EQUAL(_4, std::numeric_limits<uint64>::max());
    BOOST_CHECK(_5 == (bytes::from_literal("This is Test!!!!")));
}

BOOST_AUTO_TEST_CASE(actor_basic_test) {
    ActorThread thread([]() -> bool { return false; });
    CalcActor calc(thread);
    CalcProducer tester(thread, calc);
    thread.start();
    thread.join();
}

} // anonymous namespace

BOOST_AUTO_TEST_SUITE_END()

