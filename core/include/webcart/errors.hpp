#pragma once

#include <stdexcept>
#include <string>

namespace webcart {

// Business errors cross three boundaries (COM HRESULT, gateway XML, servlet
// page), so each one has a stable code name that every tier can switch on.
enum class ErrorCode {
    UnknownSku = 1,
    UnknownCart,
    InvalidQuantity,
    OutOfStock,
    UnknownPromotion,
    PromotionNotApplicable,
    EmptyCart,
    InvalidEmail,
    UnknownOrder,
    JournalFailure,
    BadRequest,
};

const char* code_name(ErrorCode code);

class ShopError : public std::runtime_error {
public:
    ShopError(ErrorCode code, const std::string& message)
        : std::runtime_error(message), code_(code) {}
    ErrorCode code() const noexcept { return code_; }

private:
    ErrorCode code_;
};

}  // namespace webcart
