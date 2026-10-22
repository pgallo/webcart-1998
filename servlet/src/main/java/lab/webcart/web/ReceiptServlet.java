package lab.webcart.web;

import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import jakarta.servlet.http.HttpSession;
import java.io.IOException;
import lab.webcart.store.Html;
import lab.webcart.store.StoreException;
import lab.webcart.store.Validation;

/** Shows an order only to the session that placed it: order numbers are sequential and guessable. */
public class ReceiptServlet extends HttpServlet {
    static final String LAST_ORDER = "lab.webcart.lastOrder";

    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        String id = req.getParameter("order");
        HttpSession session = req.getSession(false);
        boolean mine = session != null && id != null && id.equals(session.getAttribute(LAST_ORDER));
        String body = "<p>Order not found.</p>";
        if (mine && Validation.orderId(id)) {
            try {
                body = Html.receipt(Web.gateway(getServletContext()).getOrder(id));
            } catch (StoreException e) {
                log("receipt lookup failed: " + e.code());
            }
        }
        Web.render(req, resp, "Receipt", body);
    }
}
