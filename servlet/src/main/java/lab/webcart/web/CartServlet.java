package lab.webcart.web;

import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import java.util.Optional;
import java.util.OptionalInt;
import lab.webcart.store.Html;
import lab.webcart.store.SessionCart;
import lab.webcart.store.StoreException;
import lab.webcart.store.StoreGateway;
import lab.webcart.store.Validation;

/** GET shows the cart; POST applies one action and redirects back (post/redirect/get). */
public class CartServlet extends HttpServlet {
    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        StoreGateway gw = Web.gateway(getServletContext());
        String body;
        try {
            body = Html.cart(Web.cart(req).view(gw));
        } catch (StoreException e) {
            body = "<p>" + Html.esc(Validation.friendly(e)) + "</p>";
        }
        Web.render(req, resp, "Cart", body);
    }

    @Override
    protected void doPost(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        Web.redirectWithFlash(req, resp, "cart", apply(Web.gateway(getServletContext()), Web.cart(req), req));
    }

    /** Returns null on success, else the message for the shopper. */
    static String apply(StoreGateway gw, SessionCart cart, HttpServletRequest req) {
        String action = String.valueOf(req.getParameter("action"));
        try {
            switch (action) {
                case "add", "update", "remove" -> {
                    Optional<String> sku = Validation.sku(req.getParameter("sku"));
                    if (sku.isEmpty()) return "That product is no longer available.";
                    if (action.equals("remove")) {
                        cart.remove(gw, sku.get());
                        return null;
                    }
                    OptionalInt qty = Validation.quantity(req.getParameter("qty"), action.equals("add") ? 1 : 0,
                            Validation.MAX_LINE_QTY);
                    if (qty.isEmpty())
                        return "Please enter a quantity between 1 and " + Validation.MAX_LINE_QTY + ".";
                    if (action.equals("add")) cart.add(gw, sku.get(), qty.getAsInt());
                    else cart.update(gw, sku.get(), qty.getAsInt());
                    return null;
                }
                case "promo" -> {
                    Optional<String> code = Validation.promo(req.getParameter("code"));
                    if (code.isEmpty()) return "That promotion code is not valid.";
                    cart.applyPromotion(gw, code.get());
                    return null;
                }
                default -> {
                    return "Unknown action.";
                }
            }
        } catch (StoreException e) {
            return Validation.friendly(e);
        }
    }
}
