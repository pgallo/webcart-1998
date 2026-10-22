package lab.webcart.store;

import java.io.Serializable;
import java.util.List;

/** Read-only views of business-tier documents. Amounts are integer cents. */
public final class Model {
    private Model() {}

    public record Product(String sku, String name, String category, long unitCents, int vatBp, int available)
            implements Serializable {}

    public record Line(String sku, String name, int qty, long unitCents, long netCents, long discountCents,
                       int vatBp, boolean reserved) implements Serializable {}

    public record Cart(String id, String promo, List<Line> lines, int items, long subtotalCents,
                       long discountCents, long vatCents, long totalCents, List<String> notes)
            implements Serializable {
        public boolean isEmpty() {
            return lines.isEmpty();
        }
    }

    public record OrderLine(String sku, int qty, long unitCents, long discountCents) implements Serializable {}

    public record Order(String id, String email, String promo, long subtotalCents, long discountCents,
                        long vatCents, long totalCents, List<OrderLine> lines) implements Serializable {}
}
