package lab.webcart.store;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.List;

/**
 * Storefront logic checks with no servlet or test-framework dependency, so
 * they run with plain javac/java (scripts/check-java.sh) as well as under
 * JUnit (CartLogicTest).
 */
public final class CartLogicChecks {
    public interface Body {
        void run() throws Exception;
    }

    public record Check(String name, Body body) {}

    private CartLogicChecks() {}

    public static List<Check> all() {
        return List.of(
                new Check("validation", CartLogicChecks::validation),
                new Check("html escaping and money", CartLogicChecks::html),
                new Check("xml codec", CartLogicChecks::xmlCodec),
                new Check("viewing does not create a cart", CartLogicChecks::lazyCart),
                new Check("expired cart is replayed", CartLogicChecks::replay),
                new Check("replay drops lines now out of stock", CartLogicChecks::replayOutOfStock),
                new Check("checkout after expiry asks for review", CartLogicChecks::checkoutAfterExpiry),
                new Check("session cart is serializable", CartLogicChecks::serializable));
    }

    static void expect(boolean condition, String message) {
        if (!condition) throw new AssertionError(message);
    }

    static StoreException expectError(Body body) throws Exception {
        try {
            body.run();
        } catch (StoreException e) {
            return e;
        }
        throw new AssertionError("expected StoreException");
    }

    static void validation() {
        expect(Validation.sku(" tea-001 ").orElse("").equals("TEA-001"), "sku normalized");
        expect(Validation.sku("TEA<1").isEmpty(), "sku markup rejected");
        expect(Validation.quantity("3", 1, 99).orElse(-1) == 3, "qty");
        expect(Validation.quantity("0", 1, 99).isEmpty(), "qty below min");
        expect(Validation.quantity("1e2", 1, 99).isEmpty(), "qty exponent");
        expect(Validation.quantity(null, 1, 99).isEmpty(), "qty null");
        expect(Validation.email("a.shopper@example.org").isPresent(), "email");
        expect(Validation.email("a@b").isEmpty(), "email without dot");
        expect(Validation.email("<x>@example.org").isEmpty(), "email markup");
        expect(Validation.promo("").orElse("x").isEmpty(), "empty promo clears");
        expect(Validation.promo("bad code").isEmpty(), "promo space");
        expect(Validation.orderId("ORD-000001") && !Validation.orderId("ORD-1"), "order id");
        expect(Validation.friendly(new StoreException("OUT_OF_STOCK", "only 1 left of X")).equals("Sorry: only 1 left of X."),
                "friendly stock");
        expect(Validation.friendly(new StoreException("INTERNAL", "stack trace")).startsWith("The shop is"),
                "internal details hidden");
    }

    static void html() {
        expect(Html.esc("<a href=\"x\">&'").equals("&lt;a href=&quot;x&quot;&gt;&amp;&#39;"), "escape");
        expect(Html.cents(123456).equals("1234.56") && Html.cents(-5).equals("-0.05") && Html.cents(0).equals("0.00"),
                "cents");
        String page = Html.page("t", List.of("<script>"), "");
        expect(page.contains("&lt;script&gt;") && !page.contains("<script>"), "flash escaped");
    }

    static void xmlCodec() throws Exception {
        expect(XmlCodec.request("addItem", "sku", "A\"<&").equals("<request op=\"addItem\" sku=\"A&quot;&lt;&amp;\"/>"),
                "request escaping");
        Model.Cart cart = XmlCodec.cart(XmlCodec.payload(
                "<response status=\"ok\"><cart id=\"C1\" promo=\"BOOKS5\">"
                        + "<line sku=\"BOOK-100\" name=\"Guide &amp; more\" qty=\"1\" unitCents=\"1800\" netCents=\"1800\""
                        + " discountCents=\"500\" vatBp=\"400\" reserved=\"false\"/>"
                        + "<vat rateBp=\"400\" baseCents=\"1300\" vatCents=\"52\"/>"
                        + "<totals items=\"1\" subtotalCents=\"1800\" discountCents=\"500\" vatCents=\"52\" totalCents=\"1352\"/>"
                        + "<note>5.00 off books</note></cart></response>"));
        expect(cart.id().equals("C1") && cart.promo().equals("BOOKS5"), "cart attrs");
        expect(cart.lines().get(0).name().equals("Guide & more") && !cart.lines().get(0).reserved(), "line");
        expect(cart.totalCents() == 1352 && cart.notes().equals(List.of("5.00 off books")), "totals");
        StoreException e = expectError(() -> XmlCodec.payload(
                "<response status=\"error\" code=\"OUT_OF_STOCK\">only 1 left of POT-020</response>"));
        expect(e.code().equals("OUT_OF_STOCK") && e.getMessage().equals("only 1 left of POT-020"), "error mapped");
        e = expectError(() -> XmlCodec.payload("<!DOCTYPE r [<!ENTITY x \"y\">]><response status=\"ok\">&x;</response>"));
        expect(e.code().equals("BAD_RESPONSE"), "doctype rejected");
    }

    static void lazyCart() throws Exception {
        FakeGateway gw = new FakeGateway();
        SessionCart cart = new SessionCart();
        expect(cart.view(gw).isEmpty() && gw.cartsCreated == 0, "no cart for a look");
        cart.add(gw, "TEA-001", 2);
        expect(gw.cartsCreated == 1 && cart.view(gw).items() == 2, "first add creates");
    }

    static void replay() throws Exception {
        FakeGateway gw = new FakeGateway();
        SessionCart cart = new SessionCart();
        cart.add(gw, "TEA-001", 3);
        cart.add(gw, "BOOK-100", 1);
        cart.applyPromotion(gw, "BOOKS5");
        String before = cart.cartId();
        gw.expire(before);
        Model.Cart restored = cart.view(gw);
        expect(!restored.id().equals(before), "new business-tier cart");
        expect(restored.items() == 4 && restored.promo().equals("BOOKS5"), "lines and promo replayed");
        expect(restored.totalCents() == 3 * 650 + 1800 - 500, "same total");
        expect(cart.drainNotices().contains("Your cart had expired and has been restored."), "notice");
        expect(cart.drainNotices().isEmpty(), "notices shown once");
    }

    static void replayOutOfStock() throws Exception {
        FakeGateway gw = new FakeGateway();
        SessionCart cart = new SessionCart();
        cart.add(gw, "KIT-500", 2);
        cart.add(gw, "TEA-001", 1);
        gw.expire(cart.cartId());
        gw.setStock("KIT-500", 1);
        Model.Cart restored = cart.add(gw, "TEA-001", 1);  // the add itself triggers the replay
        expect(restored.lines().size() == 1 && restored.lines().get(0).qty() == 2, "kit dropped, tea added on top");
        expect(cart.drainNotices().get(0).startsWith("KIT-500 could not be restored"), "drop explained");
        expect(!cart.remembered().containsKey("KIT-500"), "session copy follows");
    }

    static void checkoutAfterExpiry() throws Exception {
        FakeGateway gw = new FakeGateway();
        SessionCart cart = new SessionCart();
        cart.add(gw, "BOOK-100", 1);
        gw.expire(cart.cartId());
        StoreException e = expectError(() -> cart.checkout(gw, "x@example.org"));
        expect(e.code().equals("CART_RESTORED"), "not ordered silently");
        Model.Order order = cart.checkout(gw, "x@example.org");
        expect(order.id().equals("ORD-000001") && order.totalCents() == 1800, "ordered after review");
        expect(cart.cartId() == null && cart.remembered().isEmpty(), "session cart reset");
        expect(expectError(() -> cart.checkout(gw, "x@example.org")).code().equals("EMPTY_CART"), "empty after order");
    }

    static void serializable() throws Exception {
        FakeGateway gw = new FakeGateway();
        SessionCart cart = new SessionCart();
        cart.add(gw, "TEA-001", 2);
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        try (ObjectOutputStream out = new ObjectOutputStream(bytes)) {
            out.writeObject(cart);
        }
        SessionCart copy;
        try (ObjectInputStream in = new ObjectInputStream(new ByteArrayInputStream(bytes.toByteArray()))) {
            copy = (SessionCart) in.readObject();
        }
        expect(copy.cartId().equals(cart.cartId()) && copy.remembered().equals(cart.remembered()), "round trip");
    }

    public static void main(String[] args) {
        int failed = 0;
        for (Check c : all()) {
            try {
                c.body().run();
                System.out.println("PASS " + c.name());
            } catch (Throwable t) {
                failed++;
                System.out.println("FAIL " + c.name() + ": " + t);
            }
        }
        System.out.println(all().size() - failed + "/" + all().size() + " cart logic checks passed");
        System.exit(failed == 0 ? 0 : 1);
    }
}
