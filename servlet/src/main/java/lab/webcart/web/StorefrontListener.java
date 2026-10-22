package lab.webcart.web;

import jakarta.servlet.ServletContext;
import jakarta.servlet.ServletContextEvent;
import jakarta.servlet.ServletContextListener;
import java.net.URI;
import java.time.Duration;
import lab.webcart.store.HttpXmlGateway;
import lab.webcart.store.StoreGateway;

/**
 * Wires the business-tier gateway into the servlet context. The URL comes
 * from the context parameter webcart.gatewayUrl (see web.xml) and can be
 * overridden with the system property of the same name. A gateway already
 * placed in the context (tests) is left alone.
 */
public class StorefrontListener implements ServletContextListener {
    public static final String GATEWAY_URL = "webcart.gatewayUrl";

    @Override
    public void contextInitialized(ServletContextEvent event) {
        ServletContext ctx = event.getServletContext();
        if (ctx.getAttribute(StoreGateway.CONTEXT_ATTRIBUTE) != null) return;
        String url = System.getProperty(GATEWAY_URL, ctx.getInitParameter(GATEWAY_URL));
        if (url == null || url.isBlank()) url = "http://127.0.0.1:8099/webcart/gateway";
        ctx.setAttribute(StoreGateway.CONTEXT_ATTRIBUTE, new HttpXmlGateway(URI.create(url), Duration.ofSeconds(5)));
        ctx.log("webcart storefront using business tier at " + url);
    }
}
