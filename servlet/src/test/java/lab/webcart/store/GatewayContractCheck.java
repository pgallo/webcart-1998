package lab.webcart.store;

import java.nio.file.Path;
import java.time.Duration;

/**
 * Runs HttpXmlGateway against the real C++ gateway and checks the figures
 * the other tiers agree on (same scenario as tests/com_smoke.vbs).
 * Usage: java ... GatewayContractCheck <path to webcart-gateway> <data dir>
 */
public final class GatewayContractCheck {
    private GatewayContractCheck() {}

    public static void run(StoreGateway gw) throws Exception {
        CartLogicChecks.expect(gw.catalog().size() == 8, "catalog size");
        SessionCart cart = new SessionCart();
        cart.add(gw, "TEA-001", 3);
        StoreException e = CartLogicChecks.expectError(() -> cart.applyPromotion(gw, "WELCOME10"));
        CartLogicChecks.expect(e.code().equals("PROMOTION_NOT_APPLICABLE"), "promo minimum: " + e.code());
        Model.Cart c = cart.add(gw, "BOOK-100", 1);
        CartLogicChecks.expect(c.discountCents() == 650, "multibuy applied: " + c.discountCents());
        e = CartLogicChecks.expectError(() -> cart.add(gw, "KIT-500", 3));
        CartLogicChecks.expect(e.code().equals("OUT_OF_STOCK") && e.getMessage().contains("KIT-500"), "out of stock");
        c = cart.applyPromotion(gw, "BOOKS5");
        CartLogicChecks.expect(c.subtotalCents() == 3750 && c.discountCents() == 1150 && c.vatCents() == 182
                && c.totalCents() == 2782, "totals " + c);
        String cartId = cart.cartId();
        Model.Order order = cart.checkout(gw, "contract@example.org");
        CartLogicChecks.expect(order.id().startsWith("ORD-") && order.totalCents() == 2782, "order");
        CartLogicChecks.expect(gw.getOrder(order.id()).email().equals("contract@example.org"), "getOrder");
        e = CartLogicChecks.expectError(() -> gw.getCart(cartId));
        CartLogicChecks.expect(e.code().equals("UNKNOWN_CART"), "cart closed after order");
        e = CartLogicChecks.expectError(() -> gw.addItem("Cdoesnotexist", "TEA-001", 1));
        CartLogicChecks.expect(e.code().equals("UNKNOWN_CART"), "unknown cart");
    }

    public static void main(String[] args) throws Exception {
        try (GatewayProcess gp = GatewayProcess.start(Path.of(args[0]), Path.of(args[1]))) {
            run(new HttpXmlGateway(gp.url(), Duration.ofSeconds(5)));
        }
        System.out.println("gateway contract check passed");
    }
}
