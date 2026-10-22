package lab.webcart.store;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * In-memory stand-in for the business tier: three products, no VAT, one
 * promotion code (BOOKS5, 5.00 off). Tests can expire carts and change
 * stock to drive the session replay logic.
 */
public final class FakeGateway implements StoreGateway {
    private record Item(String name, long unitCents) {}

    private final Map<String, Item> products = new LinkedHashMap<>();
    private final Map<String, Integer> stock = new HashMap<>();
    private final Map<String, LinkedHashMap<String, Integer>> carts = new HashMap<>();
    private final Map<String, String> promos = new HashMap<>();
    private final Map<String, Model.Order> orders = new HashMap<>();
    private int nextCart = 1;
    private int nextOrder = 1;
    public int cartsCreated = 0;

    public FakeGateway() {
        add("TEA-001", "Smoked black tea 100 g", 650, 40);
        add("BOOK-100", "Field guide to loose-leaf tea", 1800, 10);
        add("KIT-500", "Starter kit", 7990, 2);
    }

    private void add(String sku, String name, long cents, int qty) {
        products.put(sku, new Item(name, cents));
        stock.put(sku, qty);
    }

    public synchronized void expire(String cartId) {
        carts.remove(cartId);
        promos.remove(cartId);
    }

    public synchronized void setStock(String sku, int qty) {
        stock.put(sku, qty);
    }

    @Override
    public synchronized List<Model.Product> catalog() {
        List<Model.Product> out = new ArrayList<>();
        products.forEach((sku, p) -> out.add(new Model.Product(sku, p.name(), "demo", p.unitCents(), 0, stock.get(sku))));
        return out;
    }

    @Override
    public synchronized Model.Cart createCart() {
        String id = "F" + nextCart++;
        carts.put(id, new LinkedHashMap<>());
        cartsCreated++;
        return view(id);
    }

    @Override
    public synchronized Model.Cart getCart(String cartId) throws StoreException {
        lines(cartId);
        return view(cartId);
    }

    @Override
    public synchronized Model.Cart addItem(String cartId, String sku, int qty) throws StoreException {
        return setQuantity(cartId, sku, lines(cartId).getOrDefault(sku, 0) + qty);
    }

    @Override
    public synchronized Model.Cart setQuantity(String cartId, String sku, int qty) throws StoreException {
        Map<String, Integer> lines = lines(cartId);
        if (!products.containsKey(sku)) throw new StoreException("UNKNOWN_SKU", "unknown sku " + sku);
        if (qty > stock.get(sku)) throw new StoreException("OUT_OF_STOCK", "only " + stock.get(sku) + " left of " + sku);
        if (qty == 0) lines.remove(sku);
        else lines.put(sku, qty);
        return view(cartId);
    }

    @Override
    public synchronized Model.Cart removeItem(String cartId, String sku) throws StoreException {
        return setQuantity(cartId, sku, 0);
    }

    @Override
    public synchronized Model.Cart applyPromotion(String cartId, String code) throws StoreException {
        lines(cartId);
        if (code.isEmpty()) promos.remove(cartId);
        else if (code.equals("BOOKS5")) promos.put(cartId, code);
        else throw new StoreException("UNKNOWN_PROMOTION", code + " is not a valid code");
        return view(cartId);
    }

    @Override
    public synchronized Model.Order placeOrder(String cartId, String email) throws StoreException {
        Map<String, Integer> lines = lines(cartId);
        if (lines.isEmpty()) throw new StoreException("EMPTY_CART", "the cart is empty");
        Model.Cart c = view(cartId);
        List<Model.OrderLine> ol = new ArrayList<>();
        for (Model.Line l : c.lines()) {
            ol.add(new Model.OrderLine(l.sku(), l.qty(), l.unitCents(), l.discountCents()));
            stock.merge(l.sku(), -l.qty(), Integer::sum);
        }
        String id = String.format("ORD-%06d", nextOrder++);
        Model.Order order = new Model.Order(id, email, c.promo(), c.subtotalCents(), c.discountCents(), 0,
                c.totalCents(), List.copyOf(ol));
        orders.put(id, order);
        expire(cartId);
        return order;
    }

    @Override
    public synchronized Model.Order getOrder(String orderId) throws StoreException {
        Model.Order o = orders.get(orderId);
        if (o == null) throw new StoreException("UNKNOWN_ORDER", "no such order " + orderId);
        return o;
    }

    private Map<String, Integer> lines(String cartId) throws StoreException {
        LinkedHashMap<String, Integer> lines = carts.get(cartId);
        if (lines == null) throw new StoreException("UNKNOWN_CART", "no such cart " + cartId);
        return lines;
    }

    private Model.Cart view(String cartId) {
        List<Model.Line> out = new ArrayList<>();
        long subtotal = 0;
        int items = 0;
        for (Map.Entry<String, Integer> e : carts.get(cartId).entrySet()) {
            Item p = products.get(e.getKey());
            long net = p.unitCents() * e.getValue();
            subtotal += net;
            items += e.getValue();
            out.add(new Model.Line(e.getKey(), p.name(), e.getValue(), p.unitCents(), net, 0, 0, true));
        }
        String promo = promos.getOrDefault(cartId, "");
        long discount = promo.isEmpty() ? 0 : Math.min(500, subtotal);
        return new Model.Cart(cartId, promo, out, items, subtotal, discount, 0, subtotal - discount, List.of());
    }
}
