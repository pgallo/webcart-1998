package lab.webcart.store;

import java.io.IOException;
import java.net.URI;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.time.Duration;
import java.util.List;
import org.w3c.dom.Element;

/**
 * Talks to the business tier over HTTP/XML. Java cannot activate a DCOM
 * object, so the bridge (webcart-gateway, or gateway.asp under IIS) owns the
 * COM side and this class only speaks plain HTTP.
 */
public final class HttpXmlGateway implements StoreGateway {
    private final URI endpoint;
    private final HttpClient http;
    private final Duration timeout;

    public HttpXmlGateway(URI endpoint, Duration timeout) {
        this.endpoint = endpoint;
        this.timeout = timeout;
        this.http = HttpClient.newBuilder().connectTimeout(timeout).version(HttpClient.Version.HTTP_1_1).build();
    }

    Element call(String requestXml) throws StoreException {
        HttpRequest req = HttpRequest.newBuilder(endpoint)
                .timeout(timeout)
                .header("Content-Type", "text/xml; charset=utf-8")
                .POST(HttpRequest.BodyPublishers.ofString(requestXml, StandardCharsets.UTF_8))
                .build();
        HttpResponse<String> resp;
        try {
            resp = http.send(req, HttpResponse.BodyHandlers.ofString(StandardCharsets.UTF_8));
        } catch (IOException e) {
            throw new StoreException("GATEWAY_UNAVAILABLE", "business tier unreachable: " + e.getMessage(), e);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
            throw new StoreException("GATEWAY_UNAVAILABLE", "interrupted", e);
        }
        if (resp.statusCode() != 200)
            throw new StoreException("GATEWAY_UNAVAILABLE", "gateway answered HTTP " + resp.statusCode());
        return XmlCodec.payload(resp.body());
    }

    @Override
    public List<Model.Product> catalog() throws StoreException {
        return XmlCodec.catalog(call(XmlCodec.request("catalog")));
    }

    @Override
    public Model.Cart createCart() throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("createCart")));
    }

    @Override
    public Model.Cart getCart(String cartId) throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("getCart", "cart", cartId)));
    }

    @Override
    public Model.Cart addItem(String cartId, String sku, int qty) throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("addItem", "cart", cartId, "sku", sku, "qty", Integer.toString(qty))));
    }

    @Override
    public Model.Cart setQuantity(String cartId, String sku, int qty) throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("setQuantity", "cart", cartId, "sku", sku, "qty", Integer.toString(qty))));
    }

    @Override
    public Model.Cart removeItem(String cartId, String sku) throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("removeItem", "cart", cartId, "sku", sku)));
    }

    @Override
    public Model.Cart applyPromotion(String cartId, String code) throws StoreException {
        return XmlCodec.cart(call(XmlCodec.request("applyPromotion", "cart", cartId, "code", code)));
    }

    @Override
    public Model.Order placeOrder(String cartId, String email) throws StoreException {
        return XmlCodec.order(call(XmlCodec.request("placeOrder", "cart", cartId, "email", email)));
    }

    @Override
    public Model.Order getOrder(String orderId) throws StoreException {
        return XmlCodec.order(call(XmlCodec.request("getOrder", "order", orderId)));
    }
}
