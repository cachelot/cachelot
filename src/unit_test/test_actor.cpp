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
        CalcProducer(Actor & calc_actor_ref)
            : Actor(&CalcProducer::handle_message, &CalcProducer::main)
            , calc_actor(calc_actor_ref)
            {}
    private:
        Actor::Reply handle_message(Actor::Message & msg) noexcept {
            expected expected_result;
            expected_result = expected_results.front();
            expected_results.pop_front();
            BOOST_CHECK_EQUAL(msg.type, expected_result.reply);
            if (msg.type == atom("ok")) {
                int result;
                msg.unpack(result);
                BOOST_CHECK_EQUAL(result, expected_result.result);
            } else {
                atom_type err;
                msg.unpack(err);
                BOOST_CHECK_EQUAL(err, expected_result.error_type);
            }
            if (expected_results.empty()) {
                this->stop();
            }
            return noreply();
        }

        bool main() noexcept {
            expected_results.push_back(expected(1678235+871236));
            send_message(calc_actor, atom("add"), 1678235, 871236);
            expected_results.push_back(expected(atom("overflow")));
            send_message(calc_actor, atom("add"), std::numeric_limits<int>::max(), 1);
            expected_results.push_back(expected(-1));
            send_message(calc_actor, atom("sub"), 0, 1);
            expected_results.push_back(expected(atom("underflow")));
            send_message(calc_actor, atom("sub"), std::numeric_limits<int>::min(), 1);
            expected_results.push_back(expected(-1));
            send_message(calc_actor, atom("neg"), 1);
            return false; // nothing to do
        }
    private:
        Actor & calc_actor;
        struct expected {
            expected() {}
            expected(int res) : reply(atom("ok")), result(res) {}
            expected(atom_type err) : reply(atom("error")), error_type(err) {}
            atom_type reply;
            union{
                int result;
                atom_type error_type;
            };
        };
        std::deque<expected> expected_results;
    };

    // Simple calculator actor which support add / sub / neg commands
    class CalcActor : public Actor {
    public:
        CalcActor()
            : Actor(&CalcActor::handle_message, &CalcActor::do_nothing)
        {
        }
    private:
        // on_message
        Actor::Reply handle_message(Actor::Message & msg) noexcept {
            int a, b;
            switch (msg.type) {
                case atom("add"):
                    msg.unpack(a, b);
                    if (std::numeric_limits<int>::max() - a >= b) {
                        return reply(msg, atom("ok"), a+b);
                    } else {
                        return reply(msg, atom("error"), atom("overflow"));
                    }
                case atom("sub"):
                    msg.unpack(a, b);
                    if (std::numeric_limits<int>::min() + b <= a) {
                        return reply(msg, atom("ok"), a - b);
                    } else {
                        return reply(msg, atom("error"), atom("underflow"));
                    }
                case atom("neg"):
                    msg.unpack(a);
                    return reply(msg, atom("ok"), -a);
                default:
                    return noreply();
            }
        }

        // on_idle
        bool do_nothing() noexcept {
            return false;
        }
    };
    /// [Simple calculator actors]

    struct DummyActor : Actor {
        template <typename ... Args>
        Actor::Message * CreateMsg(atom_type type, Args && ... args) {
            return Actor::Message::Create(*this, type, std::forward<Args>(args)...);
        }
        
        void DestroyMsg(Actor::Message * msg) {
            Actor::Message::Dispose(msg);
        }
    };

} // anonymous namespace

BOOST_AUTO_TEST_SUITE(test_actor)

BOOST_AUTO_TEST_CASE(test_message_arguments) {
    DummyActor dummy;
    Actor::Message & test_message = *dummy.CreateMsg(atom("test"), 1, 2, 3u, std::numeric_limits<uint64>::max(), bytes::from_literal("This is Test!!!!"));
    int _1, _2;
    unsigned _3;
    uint64 _4;
    bytes _5;
    test_message.unpack(_1, _2, _3, _4, _5);
    BOOST_CHECK_EQUAL(_1, 1);
    BOOST_CHECK_EQUAL(_2, 2);
    BOOST_CHECK_EQUAL(_3, 3u);
    BOOST_CHECK_EQUAL(_4, std::numeric_limits<uint64>::max());
    BOOST_CHECK(_5.equals(bytes::from_literal("This is Test!!!!")));
}

BOOST_AUTO_TEST_CASE(basic_actor_test) {
    CalcActor calc_srv;
    CalcProducer prod1(calc_srv), prod2(calc_srv);
    calc_srv.start();
    prod1.start(); prod2.start();
    prod1.wait_complete();
    prod2.wait_complete();
    calc_srv.stop();
    calc_srv.wait_complete();
}


BOOST_AUTO_TEST_SUITE_END()

