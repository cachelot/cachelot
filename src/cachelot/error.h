#pragma once

#ifndef CACHELOT_ERROR_H_INCLUDED
#define CACHELOT_ERROR_H_INCLUDED

#include <boost/system/error_code.hpp>

/// @ingroup common
/// @{

namespace cachelot {

    using boost::system::error_code;
    using boost::system::error_category;

    // Application level errors defined here
    #define APPLICATION_ERROR_ENUM(apply) \
        apply(unknown_error, "Unknown error") \
        apply(out_of_memory, "Out of memory")

    /**
     * error handling mechanism based on `error_code` and `error_category`
     */
    namespace error {

        /// error_code identifies no error
        extern const error_code success;

        // unwrap macro packed errors enum
        #define APPLICATION_ERROR_ENUM_CODE(code, _) code,
        /// App-specific errors
        enum application_error_code {
            APPLICATION_ERROR_ENUM(APPLICATION_ERROR_ENUM_CODE)
        };
        #undef APPLICATION_ERROR_ENUM_CODE

        namespace internal {

            class application_error_category : public error_category {
            public:
                application_error_category() noexcept = default;

                virtual const char * name() const noexcept override { return "Application Error"; }

                virtual string message(int value) const override {
                    #define APPLICATION_ERROR_ENUM_CODE_MSG(code, msg) case code: return msg;
                    switch (static_cast<application_error_code>(value)) {
                    APPLICATION_ERROR_ENUM(APPLICATION_ERROR_ENUM_CODE_MSG)
                    default: return "Invalid error code";
                    }
                }
            };

        } // namespace internal

        inline const error_category & get_application_error_category() {
            static internal::application_error_category category_instance;
            return category_instance;
        }

        static const error_category & application_error_category = get_application_error_category();

        inline error_code make_error(application_error_code ec) {
            return error_code(static_cast<int>(ec), application_error_category);
        }

    } // namespace error

} // namespace cachelot

/// @}

namespace boost { namespace system {
    // register error_code type
    template <> struct is_error_code_enum<cachelot::error::application_error_code> {
        static const bool value = true;
    };
} } // namespace boost::system


#endif // CACHELOT_ERROR_H_INCLUDED
