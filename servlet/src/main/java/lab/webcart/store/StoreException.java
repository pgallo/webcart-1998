package lab.webcart.store;

/** A business-tier error, carrying the same code name on every tier (OUT_OF_STOCK, UNKNOWN_CART, ...). */
public class StoreException extends Exception {
    private final String code;

    public StoreException(String code, String message) {
        super(message);
        this.code = code;
    }

    public StoreException(String code, String message, Throwable cause) {
        super(message, cause);
        this.code = code;
    }

    public String code() {
        return code;
    }
}
