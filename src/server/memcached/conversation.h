#ifndef CACHELOT_CONVERSATION_H_INCLUDED
#define CACHELOT_CONVERSATION_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


/// @ingroup memcached
/// @{

#include <server/memcached/memcached.h>

namespace cachelot {

    namespace memcached {

        /// Memcached protocol conversation
        class Conversation {
        public:
            /// constructor
            Conversation(cache::Cache & the_cache);

        protected:
            /// @copydoc basic_conversation::handle_data()
            Reply handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept override;
        private:
            Reply handle_retrieval_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);
            Reply handle_storage_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);
            Reply handle_delete_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);
            Reply handle_arithmetic_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);
            Reply handle_touch_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);
            Reply handle_statistics_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf);

            Reply reply_with_error(io_buffer & send_buf, const error_code error) noexcept;
            Reply reply_with_server_error(io_buffer & send_buf, const std::exception exception) noexcept;
            Reply reply_with_response(io_buffer & send_buf, error_code error, cache::Response response, bool noreply);

            /// choose ascii or binary protocol parser
            static std::unique_ptr<basic_protocol_parser> determine_protocol(io_buffer & recv_buf);

        private:
            cache::Cache & cache;
            std::unique_ptr<basic_protocol_parser> m_protocol_parser;
            const cache::HashFunction calc_hash;
        };


        inline Conversation::Conversation(cache::Cache & the_cache)
            : cache(the_cache)
            , m_protocol_parser()
            , calc_hash() {
        }


        inline Conversation::Reply Conversation::handle_data(io_buffer & recv_buf, io_buffer & send_buf) noexcept {
            try {
                if (m_protocol_parser == nullptr) {
                    m_protocol_parser = basic_protocol_parser::determine_protocol(recv_buf);
                }
                auto cmd = m_protocol_parser->parse_command_name(recv_buf);
                switch (cmd) {
                    case cache::ADD:
                    case cache::APPEND:
                    case cache::CAS:
                    case cache::PREPEND:
                    case cache::REPLACE:
                    case cache::SET:
                        return handle_storage_command(cmd, recv_buf, send_buf);
                    case cache::DELETE:
                        return handle_delete_command(cmd, recv_buf, send_buf);
                    case cache::INCR:
                    case cache::DECR:
                        return handle_arithmetic_command(cmd, recv_buf, send_buf);
                    case cache::TOUCH:
                        return handle_touch_command(cmd, recv_buf, send_buf);
                    case cache::GET:
                    case cache::GETS:
                        return handle_retrieval_command(cmd, recv_buf, send_buf);
                    case cache::STATS:
                        return handle_statistics_command(cmd, recv_buf, send_buf);
                    case cache::QUIT:
                        return CLOSE_IMMEDIATELY;
                    default:
                        m_protocol_parser->write_unknown_command_error(send_buf);
                        recv_buf.discard_all();
                        return SEND_REPLY;
                }
            } catch (const system_error & ex) {
                send_buf.discard_written();
                return reply_with_error(send_buf, ex.code());
            } catch (const std::exception & exc) {
                return reply_with_server_error(send_buf, exc);
            }
        }


        inline Conversation::Reply Conversation::handle_retrieval_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf) {
            auto args = m_protocol_parser->parse_retrieval_command(recv_buf);
            do {
                bytes key;
                tie(key, args) = m_protocol_parser->parse_retrieval_command_keys(args);
                auto found_item = cache.do_get(key, calc_hash(key));
                if (found_item) {
                    m_protocol_parser->write_item(send_buf, found_item, cmd == cache::GETS);
                }
            } while (args);
            m_protocol_parser->finalize_batch(send_buf);
            return SEND_REPLY;
        }


        inline Conversation::Reply Conversation::handle_storage_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf) {
            bytes key; bytes value; cache::opaque_flags_type flags; cache::expiration_time_point expiration; cache::version_type version; bool noreply;
            cachelot::tie(key, value, flags, expiration, version, noreply) = m_protocol_parser->parse_storage_command(cmd, recv_buf);
            // create a new Item
            error_code error; cache::Item * new_item;
            tie(error, new_item) = cache.create_item(key, calc_hash(key), value.length(), flags, expiration, version);
            if (error) {
                return reply_with_error(send_buf, error);
            }
            new_item->assign_value(value);
            cache::Response response;
            tie(error, response) = cache.do_storage(cmd, new_item);
            if (error) {
                cache.destroy_item(new_item);
            }
            return reply_with_response(send_buf, error, response, noreply);
        }


        inline Conversation::Reply Conversation::handle_delete_command(cache::Command, io_buffer & recv_buf, io_buffer & send_buf) {
            bytes key; bool noreply;
            tie(key, noreply) = m_protocol_parser->parse_delete_command(recv_buf);
            error_code error; cache::Response response;
            tie(error, response) = cache.do_delete(key, calc_hash(key));
            return reply_with_response(send_buf, error, response, noreply);
        }


        inline Conversation::Reply Conversation::handle_arithmetic_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf) {

        }


        inline Conversation::Reply Conversation::handle_touch_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf) {

        }


        inline Conversation::Reply Conversation::handle_statistics_command(cache::Command cmd, io_buffer & recv_buf, io_buffer & send_buf) {

        }


        inline Conversation::Reply Conversation::reply_with_error(io_buffer & send_buf, const error_code error) noexcept {

        }


        inline Conversation::Reply Conversation::reply_with_server_error(io_buffer & send_buf, const std::exception exception) noexcept {

        }

        inline Conversation::Reply Conversation::reply_with_response(io_buffer & send_buf, error_code error, cache::Response response, bool noreply) {
            if (not error) {
                if (not noreply) {
                    m_protocol_parser->write_response(send_buf, response);
                    return SEND_REPLY;
                } else {
                    return READ_MORE;
                }
            } else {
                return reply_with_error(send_buf, error);
            }
        }



    } // namespace memcached

} // namespace cachelot

/// @}

#endif // CACHELOT_CONVERSATION_H_INCLUDED
