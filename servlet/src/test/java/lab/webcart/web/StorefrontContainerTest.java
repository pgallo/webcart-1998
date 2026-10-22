package lab.webcart.web;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import jakarta.servlet.DispatcherType;
import java.net.CookieManager;
import java.net.URI;
import java.net.URLEncoder;
import java.net.http.HttpClient;
import java.net.http.HttpRequest;
import java.net.http.HttpResponse;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.time.Duration;
import java.util.EnumSet;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.stream.Collectors;
import lab.webcart.store.FakeGateway;
import lab.webcart.store.GatewayProcess;
import lab.webcart.store.HttpXmlGateway;
import lab.webcart.store.StoreGateway;
import org.eclipse.jetty.ee10.servlet.FilterHolder;
import org.eclipse.jetty.ee10.servlet.ServletContextHandler;
import org.eclipse.jetty.ee10.servlet.ServletHolder;
import org.eclipse.jetty.server.Server;
import org.eclipse.jetty.server.ServerConnector;
import org.junit.jupiter.api.AfterAll;
import org.junit.jupiter.api.BeforeAll;
import org.junit.jupiter.api.Test;

/**
 * The servlet storefront in an embedded servlet container, driven over HTTP with a
 * cookie-keeping client. With -Dwebcart.gateway.bin it goes all the way to
 * the C++ business tier through the HTTP/XML gateway; otherwise it runs on
 * the in-memory fake.
 */
class StorefrontContainerTest {
    private static Server server;
    private static GatewayProcess gateway;
    private static URI base;
    private static boolean realTier;

    @BeforeAll
    static void start() throws Exception {
        StoreGateway gw;
        String bin = System.getProperty("webcart.gateway.bin", "");
        realTier = !bin.isBlank();
        if (realTier) {
            gateway = GatewayProcess.start(Path.of(bin), Path.of(System.getProperty("webcart.data.dir")));
            gw = new HttpXmlGateway(gateway.url(), Duration.ofSeconds(5));
        } else {
            gw = new FakeGateway();
        }

        ServletContextHandler ctx = new ServletContextHandler(ServletContextHandler.SESSIONS);
        ctx.setContextPath("/shop");
        ctx.setAttribute(StoreGateway.CONTEXT_ATTRIBUTE, gw);
        ctx.addEventListener(new StorefrontListener());
        ctx.addFilter(new FilterHolder(new EncodingFilter()), "/*", EnumSet.of(DispatcherType.REQUEST));
        ctx.addFilter(new FilterHolder(new SessionCartFilter()), "/cart", EnumSet.of(DispatcherType.REQUEST));
        ctx.addFilter(new FilterHolder(new SessionCartFilter()), "/checkout", EnumSet.of(DispatcherType.REQUEST));
        ctx.addServlet(new ServletHolder(new CatalogServlet()), "/catalog");
        ctx.addServlet(new ServletHolder(new CartServlet()), "/cart");
        ctx.addServlet(new ServletHolder(new CheckoutServlet()), "/checkout");
        ctx.addServlet(new ServletHolder(new ReceiptServlet()), "/receipt");

        server = new Server(0);
        server.setHandler(ctx);
        server.start();
        int port = ((ServerConnector) server.getConnectors()[0]).getLocalPort();
        base = URI.create("http://127.0.0.1:" + port + "/shop/");
    }

    @AfterAll
    static void stop() throws Exception {
        if (server != null) server.stop();
        if (gateway != null) gateway.close();
    }

    private static HttpClient shopper() {
        return HttpClient.newBuilder()
                .cookieHandler(new CookieManager())
                .followRedirects(HttpClient.Redirect.NORMAL)
                .build();
    }

    private static HttpResponse<String> get(HttpClient client, String path) throws Exception {
        return client.send(HttpRequest.newBuilder(base.resolve(path)).GET().build(), HttpResponse.BodyHandlers.ofString());
    }

    private static HttpResponse<String> post(HttpClient client, String path, String... form) throws Exception {
        Map<String, String> fields = new LinkedHashMap<>();
        for (int i = 0; i < form.length; i += 2) fields.put(form[i], form[i + 1]);
        String body = fields.entrySet().stream()
                .map(e -> e.getKey() + "=" + URLEncoder.encode(e.getValue(), StandardCharsets.UTF_8))
                .collect(Collectors.joining("&"));
        return client.send(HttpRequest.newBuilder(base.resolve(path))
                .header("Content-Type", "application/x-www-form-urlencoded")
                .POST(HttpRequest.BodyPublishers.ofString(body)).build(), HttpResponse.BodyHandlers.ofString());
    }

    @Test
    void shopperJourney() throws Exception {
        HttpClient client = shopper();

        HttpResponse<String> page = get(client, "catalog");
        assertEquals(200, page.statusCode());
        assertTrue(page.body().contains("TEA-001"), page.body());
        assertEquals("no-store", page.headers().firstValue("Cache-Control").orElse(""));

        page = get(client, "cart");
        assertTrue(page.body().contains("Your cart is empty."));

        post(client, "cart", "action", "add", "sku", "tea-001", "qty", "3");
        post(client, "cart", "action", "add", "sku", "BOOK-100", "qty", "1");
        page = post(client, "cart", "action", "promo", "code", "books5");
        assertEquals(200, page.statusCode());
        assertTrue(page.body().contains("Total EUR " + (realTier ? "27.82" : "32.50")), page.body());

        page = post(client, "cart", "action", "add", "sku", "KIT-500", "qty", "3");
        assertTrue(page.body().contains("Sorry: only 2 left of KIT-500."), page.body());
        page = post(client, "cart", "action", "add", "sku", "<script>", "qty", "1");
        assertTrue(page.body().contains("That product is no longer available."), page.body());
        assertTrue(!page.body().contains("<script>"));
        page = post(client, "cart", "action", "add", "sku", "TEA-001", "qty", "500");
        assertTrue(page.body().contains("Please enter a quantity between 1 and 99."), page.body());

        page = post(client, "checkout", "email", "not an email");
        assertTrue(page.body().contains("Please enter a valid e-mail address."), page.body());
        page = post(client, "checkout", "email", "shopper@example.org");
        assertEquals(200, page.statusCode());
        assertTrue(page.uri().getPath().endsWith("/receipt"), page.uri().toString());
        assertTrue(page.body().contains("<span id=\"order\">ORD-"), page.body());
        assertTrue(page.body().contains("shopper@example.org"));
        String receiptUri = page.uri().toString();

        page = get(client, "cart");
        assertTrue(page.body().contains("Your cart is empty."), "cart reset after order");

        // Another shopper cannot read the receipt by guessing the number.
        page = shopper().send(HttpRequest.newBuilder(URI.create(receiptUri)).GET().build(),
                HttpResponse.BodyHandlers.ofString());
        assertTrue(page.body().contains("Order not found."), page.body());
    }

    @Test
    void separateShoppersHaveSeparateCarts() throws Exception {
        HttpClient a = shopper();
        HttpClient b = shopper();
        post(a, "cart", "action", "add", "sku", "TEA-001", "qty", "1");
        assertTrue(get(a, "cart").body().contains("Smoked black tea"));
        assertTrue(get(b, "cart").body().contains("Your cart is empty."));
    }
}
