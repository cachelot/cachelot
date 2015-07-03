#ifndef CACHELOT_ERROR_H_INCLUDED
#define CACHELOT_ERROR_H_INCLUDED

//
//  (C) Copyright 2015 Iurii Krasnoshchok
//
//  Distributed under the terms of Simplified BSD License
//  see LICENSE file


#include <boost/system/error_code.hpp>   // error_code, error_category
#include <boost/system/system_error.hpp> // system_error exception

/// @ingroup common
/// @{

namespace cachelot {

    // error handling mechanism based on `error_code` and `error_category`

    // Application level errors defined here
    // Note: all error codes declared in `error` namespace, so use as `error::unknown_error`
    #define CACHELOT_ERROR_ENUM(x)                                  \
        x(unknown_error,        "Unknown error")                    \
        x(out_of_memory,        "Out of memory")                    \
        x(numeric_convert,      "Numeric conversion error")         \
        x(numeric_overflow,     "Numeric value is out of range")    \
        x(not_implemented,      "Not implemented")                  \
        x(incomplete_request,   "Request packet is incomplete")     \
        x(brocken_request,      "Request packet is brocken")

    /// system error handling
    using boost::system::error_code;      // boost::error is used rather than std's because it is used by boost::asio
    using boost::system::error_category;
    using boost::system::system_error;


    /// @defgroup error Error codes definition
    /// @{
    namespace error {

        /// error_code identifies no error
        extern const error_code success;

        /// App-specific errors
        enum cachelot_error_code {
            // unwrap macro
            #define CACHELOT_ERROR_ENUM_CODE(code, _) code,
            CACHELOT_ERROR_ENUM(CACHELOT_ERROR_ENUM_CODE)
            #undef CACHELOT_ERROR_ENUM_CODE
        };


        namespace internal {

            class cachelot_error_category : public error_category {
            public:
                cachelot_error_category() = default;

                virtual const char * name() const noexcept override { return "Application error"; }

                virtual string message(int value) const override {
                    // unwrap macro
                    #define CACHELOT_ERROR_ENUM_CODE_MSG(code, msg) case code: return msg;
                    switch (static_cast<cachelot_error_code>(value)) {
                        CACHELOT_ERROR_ENUM(CACHELOT_ERROR_ENUM_CODE_MSG)
                        default: return "Invalid error code";
                    }
                    #undef CACHELOT_ERROR_ENUM_CODE_MSG
                }
            };

        } // namespace internal


        inline const error_category & get_cachelot_error_category() noexcept {
            static const internal::cachelot_error_category category_instance {};
            return category_instance;
        }


        inline error_code make_error_code(cachelot_error_code ec) noexcept {
            return error_code(static_cast<int>(ec), get_cachelot_error_category());
        }


    } // namespace error

    using error::get_cachelot_error_category;

    /// @}

} // namespace cachelot

/// @}

namespace boost { namespace system {
    // register error_code type
    template <> struct is_error_code_enum<cachelot::error::cachelot_error_code> {
        static const bool value = true;
    };
} } // namespace boost::system


#endif // CACHELOT_ERROR_H_INCLUDED
