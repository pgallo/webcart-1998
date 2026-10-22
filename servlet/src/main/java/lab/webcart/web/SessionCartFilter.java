package lab.webcart.web;

import jakarta.servlet.FilterChain;
import jakarta.servlet.ServletException;
import jakarta.servlet.http.HttpFilter;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.servlet.http.HttpSession;
import java.io.IOException;
import lab.webcart.store.SessionCart;

/**
 * Gives every storefront request its SessionCart and writes it back to the
 * session afterwards. Setting the attribute again after the request tells a
 * replicating container that the (mutable) object changed.
 */
public class SessionCartFilter extends HttpFilter {
    @Override
    protected void doFilter(HttpServletRequest req, HttpServletResponse resp, FilterChain chain)
            throws IOException, ServletException {
        HttpSession session = req.getSession();
        SessionCart cart;
        // Brief global lock for get-or-create; the shopper's requests are
        // then serialized on their own cart object.
        synchronized (SessionCart.class) {
            cart = (SessionCart) session.getAttribute(SessionCart.SESSION_ATTRIBUTE);
            if (cart == null) {
                cart = new SessionCart();
                session.setAttribute(SessionCart.SESSION_ATTRIBUTE, cart);
            }
        }
        req.setAttribute(SessionCart.SESSION_ATTRIBUTE, cart);
        synchronized (cart) {
            chain.doFilter(req, resp);
        }
        try {
            session.setAttribute(SessionCart.SESSION_ATTRIBUTE, cart);
        } catch (IllegalStateException invalidated) {
            // checkout or logout invalidated the session: nothing to write back
        }
    }
}
