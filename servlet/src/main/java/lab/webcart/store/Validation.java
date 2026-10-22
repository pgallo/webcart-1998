package lab.webcart.store;

import java.util.Locale;
import java.util.Optional;
import java.util.OptionalInt;
import java.util.regex.Pattern;

/** Whitelist validation of request parameters; same rules as asp/lib/validate.vbs. */
public final class Validation {
    public static final int MAX_LINE_QTY = 99;

    private static final Pattern SKU = Pattern.compile("[A-Z0-9-]{1,32}");
    private static final Pattern DIGITS = Pattern.compile("[0-9]{1,4}");
    private static final Pattern PROMO = Pattern.compile("[A-Z0-9]{0,20}");
    private static final Pattern EMAIL = Pattern.compile("[^@\\s<>\"&,]+@[^@\\s<>\"&,.]+(\\.[^@\\s<>\"&,.]+)+");
    private static final Pattern ORDER = Pattern.compile("ORD-[0-9]{6,}");

    private Validation() {}

    public static Optional<String> sku(String raw) {
        String s = raw == null ? "" : raw.trim().toUpperCase(Locale.ROOT);
        return SKU.matcher(s).matches() ? Optional.of(s) : Optional.empty();
    }

    public static OptionalInt quantity(String raw, int min, int max) {
        String s = raw == null ? "" : raw.trim();
        if (!DIGITS.matcher(s).matches()) return OptionalInt.empty();
        int q = Integer.parseInt(s);
        return q >= min && q <= max ? OptionalInt.of(q) : OptionalInt.empty();
    }

    /** Empty string is valid and means "remove the code". */
    public static Optional<String> promo(String raw) {
        String s = raw == null ? "" : raw.trim().toUpperCase(Locale.ROOT);
        return PROMO.matcher(s).matches() ? Optional.of(s) : Optional.empty();
    }

    public static Optional<String> email(String raw) {
        String s = raw == null ? "" : raw.trim();
        return s.length() <= 254 && EMAIL.matcher(s).matches() ? Optional.of(s) : Optional.empty();
    }

    public static boolean orderId(String raw) {
        return raw != null && ORDER.matcher(raw).matches();
    }

    /** What the shopper sees for a business error. */
    public static String friendly(StoreException e) {
        return switch (e.code()) {
            case "OUT_OF_STOCK", "PROMOTION_NOT_APPLICABLE", "CART_RESTORED" -> "Sorry: " + e.getMessage() + ".";
            case "UNKNOWN_PROMOTION" -> "That promotion code is not valid.";
            case "INVALID_QUANTITY" -> "Please enter a quantity between 1 and " + MAX_LINE_QTY + ".";
            case "INVALID_EMAIL" -> "Please enter a valid e-mail address.";
            case "EMPTY_CART" -> "Your cart is empty.";
            case "UNKNOWN_SKU" -> "That product is no longer available.";
            default -> "The shop is temporarily unavailable. Please try again.";
        };
    }
}
