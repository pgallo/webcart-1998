package lab.webcart.web;

import jakarta.servlet.ServletContext;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.servlet.http.HttpSession;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import lab.webcart.store.Html;
import lab.webcart.store.SessionCart;
import lab.webcart.store.StoreGateway;

/** Small helpers shared by the servlets. */
final class Web {
    static final String FLASH = "lab.webcart.flash";

    private Web() {}

    static StoreGateway gateway(ServletContext ctx) {
        Object gw = ctx.getAttribute(StoreGateway.CONTEXT_ATTRIBUTE);
        if (gw == null) throw new IllegalStateException("no StoreGateway configured");
        return (StoreGateway) gw;
    }

    /** Set by {@link SessionCartFilter} for every storefront request. */
    static SessionCart cart(HttpServletRequest req) {
        return (SessionCart) req.getAttribute(SessionCart.SESSION_ATTRIBUTE);
    }

    /** Post/redirect/get: the message survives exactly one redirect. */
    static void redirectWithFlash(HttpServletRequest req, HttpServletResponse resp, String location, String message)
            throws IOException {
        if (message != null && !message.isEmpty()) req.getSession().setAttribute(FLASH, message);
        resp.sendRedirect(location);
    }

    static void render(HttpServletRequest req, HttpServletResponse resp, String title, String body) throws IOException {
        List<String> messages = new ArrayList<>();
        HttpSession session = req.getSession(false);
        if (session != null) {
            Object flash = session.getAttribute(FLASH);
            if (flash != null) messages.add(flash.toString());
            session.removeAttribute(FLASH);
        }
        SessionCart cart = cart(req);
        if (cart != null) messages.addAll(cart.drainNotices());
        resp.setContentType("text/html;charset=UTF-8");
        resp.getWriter().write(Html.page(title, messages, body));
    }
}
