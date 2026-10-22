package lab.webcart.store;

import java.io.Serial;
import java.io.Serializable;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * The shopper's cart as kept in the HttpSession. The business tier owns the
 * authoritative cart and its stock holds; the session keeps the cart id plus
 * a copy of what the shopper asked for. If the business tier has dropped the
 * cart (idle expiry, server restart), the session copy is replayed into a new
 * cart, so the shopper loses at most the items that are no longer in stock.
 */
public final class SessionCart implements Serializable {
    public static final String SESSION_ATTRIBUTE = "lab.webcart.cart";
    @Serial
    private static final long serialVersionUID = 1L;

    private String cartId;
    private final LinkedHashMap<String, Integer> remembered = new LinkedHashMap<>();
    private String promo = "";
    private final List<String> notices = new ArrayList<>();

    @FunctionalInterface
    private interface CartCall<T> {
        T apply(String cartId) throws StoreException;
    }

    public String cartId() {
        return cartId;
    }

    public Map<String, Integer> remembered() {
        return Map.copyOf(remembered);
    }

    /** Messages about a restored cart, shown once on the next page. */
    public List<String> drainNotices() {
        List<String> out = List.copyOf(notices);
        notices.clear();
        return out;
    }

    /** Viewing never creates a business-tier cart; the first add does. */
    public Model.Cart view(StoreGateway gw) throws StoreException {
        if (cartId == null && remembered.isEmpty())
            return new Model.Cart("", "", List.of(), 0, 0, 0, 0, 0, List.of());
        return sync(withCart(gw, gw::getCart));
    }

    public Model.Cart add(StoreGateway gw, String sku, int qty) throws StoreException {
        return sync(withCart(gw, id -> gw.addItem(id, sku, qty)));
    }

    public Model.Cart update(StoreGateway gw, String sku, int qty) throws StoreException {
        return sync(withCart(gw, id -> gw.setQuantity(id, sku, qty)));
    }

    public Model.Cart remove(StoreGateway gw, String sku) throws StoreException {
        return sync(withCart(gw, id -> gw.removeItem(id, sku)));
    }

    public Model.Cart applyPromotion(StoreGateway gw, String code) throws StoreException {
        return sync(withCart(gw, id -> gw.applyPromotion(id, code)));
    }

    /**
     * Places the order. A cart that had to be restored is not ordered
     * silently: prices or stock may have changed, so the shopper reviews it.
     */
    public Model.Order checkout(StoreGateway gw, String email) throws StoreException {
        if (cartId == null) throw new StoreException("EMPTY_CART", "the cart is empty");
        Model.Order order;
        try {
            order = gw.placeOrder(cartId, email);
        } catch (StoreException e) {
            if (!"UNKNOWN_CART".equals(e.code())) throw e;
            rebuild(gw);
            throw new StoreException("CART_RESTORED", "your cart had expired and was restored, please review it");
        }
        cartId = null;
        remembered.clear();
        promo = "";
        return order;
    }

    private <T> T withCart(StoreGateway gw, CartCall<T> call) throws StoreException {
        if (cartId == null) cartId = gw.createCart().id();
        try {
            return call.apply(cartId);
        } catch (StoreException e) {
            if (!"UNKNOWN_CART".equals(e.code())) throw e;
            rebuild(gw);
            return call.apply(cartId);
        }
    }

    private void rebuild(StoreGateway gw) throws StoreException {
        cartId = gw.createCart().id();
        for (Iterator<Map.Entry<String, Integer>> it = remembered.entrySet().iterator(); it.hasNext(); ) {
            Map.Entry<String, Integer> line = it.next();
            try {
                gw.addItem(cartId, line.getKey(), line.getValue());
            } catch (StoreException e) {
                if (!"OUT_OF_STOCK".equals(e.code()) && !"UNKNOWN_SKU".equals(e.code())) throw e;
                it.remove();
                notices.add(line.getKey() + " could not be restored: " + e.getMessage());
            }
        }
        if (!promo.isEmpty()) {
            try {
                gw.applyPromotion(cartId, promo);
            } catch (StoreException e) {
                notices.add("Promotion " + promo + " no longer applies: " + e.getMessage());
                promo = "";
            }
        }
        notices.add("Your cart had expired and has been restored.");
    }

    /** The business tier's answer is authoritative; the session copy follows it. */
    private Model.Cart sync(Model.Cart cart) {
        remembered.clear();
        for (Model.Line l : cart.lines()) remembered.put(l.sku(), l.qty());
        promo = cart.promo();
        return cart;
    }
}
