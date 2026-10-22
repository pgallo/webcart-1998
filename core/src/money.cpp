#include "webcart/money.hpp"

#include <cstdio>
#include <stdexcept>

#include "webcart/errors.hpp"

namespace webcart {

Cents mul_div_round(Cents value, std::int64_t num, std::int64_t den) {
    if (den <= 0) throw std::invalid_argument("denominator must be positive");
    const std::int64_t product = value * num;
    const std::int64_t half = den / 2;
    return product >= 0 ? (product + half) / den : -((-product + half) / den);
}

std::string format_cents(Cents value) {
    const bool negative = value < 0;
    const Cents abs = negative ? -value : value;
    char buf[32];
    std::snprintf(buf, sizeof buf, "%s%lld.%02lld", negative ? "-" : "",
                  static_cast<long long>(abs / 100), static_cast<long long>(abs % 100));
    return buf;
}

const char* code_name(ErrorCode code) {
    switch (code) {
        case ErrorCode::UnknownSku: return "UNKNOWN_SKU";
        case ErrorCode::UnknownCart: return "UNKNOWN_CART";
        case ErrorCode::InvalidQuantity: return "INVALID_QUANTITY";
        case ErrorCode::OutOfStock: return "OUT_OF_STOCK";
        case ErrorCode::UnknownPromotion: return "UNKNOWN_PROMOTION";
        case ErrorCode::PromotionNotApplicable: return "PROMOTION_NOT_APPLICABLE";
        case ErrorCode::EmptyCart: return "EMPTY_CART";
        case ErrorCode::InvalidEmail: return "INVALID_EMAIL";
        case ErrorCode::UnknownOrder: return "UNKNOWN_ORDER";
        case ErrorCode::JournalFailure: return "JOURNAL_FAILURE";
        case ErrorCode::BadRequest: return "BAD_REQUEST";
    }
    return "UNKNOWN_ERROR";
}

}  // namespace webcart
