package lab.webcart.store;

import java.util.List;

/**
 * The storefront's view of the business tier. The production implementation
 * is {@link HttpXmlGateway}; tests use an in-memory fake.
 */
public interface StoreGateway {
    String CONTEXT_ATTRIBUTE = "lab.webcart.gateway";

    List<Model.Product> catalog() throws StoreException;

    Model.Cart createCart() throws StoreException;

    Model.Cart getCart(String cartId) throws StoreException;

    Model.Cart addItem(String cartId, String sku, int qty) throws StoreException;

    Model.Cart setQuantity(String cartId, String sku, int qty) throws StoreException;

    Model.Cart removeItem(String cartId, String sku) throws StoreException;

    /** An empty code removes the current one. */
    Model.Cart applyPromotion(String cartId, String code) throws StoreException;

    Model.Order placeOrder(String cartId, String email) throws StoreException;

    Model.Order getOrder(String orderId) throws StoreException;
}
