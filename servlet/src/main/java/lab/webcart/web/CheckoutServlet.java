package lab.webcart.web;

import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.net.URLEncoder;
import java.nio.charset.StandardCharsets;
import java.util.Optional;
import lab.webcart.store.Html;
import lab.webcart.store.Model;
import lab.webcart.store.StoreException;
import lab.webcart.store.StoreGateway;
import lab.webcart.store.Validation;

public class CheckoutServlet extends HttpServlet {
    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        String body;
        try {
            body = Html.checkout(Web.cart(req).view(Web.gateway(getServletContext())));
        } catch (StoreException e) {
            body = "<p>" + Html.esc(Validation.friendly(e)) + "</p>";
        }
        Web.render(req, resp, "Checkout", body);
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        Optional<String> email = Validation.email(req.getParameter("email"));
        if (email.isEmpty()) {
            Web.redirectWithFlash(req, resp, "checkout", "Please enter a valid e-mail address.");
            return;
        }
        StoreGateway gw = Web.gateway(getServletContext());
        try {
            Model.Order order = Web.cart(req).checkout(gw, email.get());
            req.getSession().setAttribute(ReceiptServlet.LAST_ORDER, order.id());
            resp.sendRedirect("receipt?order=" + URLEncoder.encode(order.id(), StandardCharsets.UTF_8));
        } catch (StoreException e) {
            Web.redirectWithFlash(req, resp, "checkout", Validation.friendly(e));
        }
    }
}
