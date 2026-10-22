package lab.webcart.store;

import java.util.List;

/** JSP-free HTML rendering: plain string building with escaping at every interpolation. */
public final class Html {
    private Html() {}

    public static String esc(String s) {
        if (s == null) return "";
        StringBuilder sb = new StringBuilder(s.length() + 16);
        for (char c : s.toCharArray()) {
            switch (c) {
                case '&' -> sb.append("&amp;");
                case '<' -> sb.append("&lt;");
                case '>' -> sb.append("&gt;");
                case '"' -> sb.append("&quot;");
                case '\'' -> sb.append("&#39;");
                default -> sb.append(c);
            }
        }
        return sb.toString();
    }

    public static String cents(long value) {
        long abs = Math.abs(value);
        return (value < 0 ? "-" : "") + (abs / 100) + "." + String.format("%02d", abs % 100);
    }

    public static String page(String title, List<String> messages, String body) {
        StringBuilder sb = new StringBuilder();
        sb.append("<!DOCTYPE html>\n<html><head><meta charset=\"utf-8\"><title>")
          .append(esc(title)).append("</title></head><body>\n")
          .append("<h1>Webcart demo store (servlet)</h1>\n")
          .append("<p><a href=\"catalog\">Catalog</a> | <a href=\"cart\">Cart</a> | <a href=\"checkout\">Checkout</a></p>\n");
        for (String m : messages) sb.append("<p class=\"flash\"><strong>").append(esc(m)).append("</strong></p>\n");
        return sb.append(body).append("\n</body></html>\n").toString();
    }

    public static String catalog(List<Model.Product> products) {
        StringBuilder sb = new StringBuilder("<h2>Catalog</h2>\n<table border=\"1\" cellpadding=\"4\">\n")
                .append("<tr><th>Item</th><th>Category</th><th>Price (net)</th><th>VAT</th><th>Available</th><th></th></tr>\n");
        for (Model.Product p : products) {
            sb.append("<tr><td>").append(esc(p.name())).append(" <small>(").append(esc(p.sku())).append(")</small></td>")
              .append("<td>").append(esc(p.category())).append("</td>")
              .append("<td align=\"right\">").append(cents(p.unitCents())).append("</td>")
              .append("<td align=\"right\">").append(cents(p.vatBp())).append("%</td>")
              .append("<td align=\"right\">").append(p.available()).append("</td>")
              .append("<td><form method=\"post\" action=\"cart\">")
              .append("<input type=\"hidden\" name=\"action\" value=\"add\">")
              .append("<input type=\"hidden\" name=\"sku\" value=\"").append(esc(p.sku())).append("\">")
              .append("<input type=\"text\" name=\"qty\" value=\"1\" size=\"2\" maxlength=\"2\">")
              .append("<input type=\"submit\" value=\"Add\"></form></td></tr>\n");
        }
        return sb.append("</table>").toString();
    }

    public static String cart(Model.Cart cart) {
        if (cart.isEmpty()) return "<h2>Your cart</h2>\n<p>Your cart is empty.</p>";
        StringBuilder sb = new StringBuilder("<h2>Your cart</h2>\n<table border=\"1\" cellpadding=\"4\">\n")
                .append("<tr><th>Item</th><th>Qty</th><th>Net</th><th>Discount</th><th></th></tr>\n");
        for (Model.Line l : cart.lines()) {
            sb.append("<tr><td>").append(esc(l.name())).append(l.reserved() ? "" : " <em>(hold expired)</em>").append("</td>")
              .append("<td><form method=\"post\" action=\"cart\"><input type=\"hidden\" name=\"action\" value=\"update\">")
              .append("<input type=\"hidden\" name=\"sku\" value=\"").append(esc(l.sku())).append("\">")
              .append("<input type=\"text\" name=\"qty\" value=\"").append(l.qty()).append("\" size=\"2\" maxlength=\"2\">")
              .append("<input type=\"submit\" value=\"Update\"></form></td>")
              .append("<td align=\"right\">").append(cents(l.netCents())).append("</td>")
              .append("<td align=\"right\">").append(cents(l.discountCents())).append("</td>")
              .append("<td><form method=\"post\" action=\"cart\"><input type=\"hidden\" name=\"action\" value=\"remove\">")
              .append("<input type=\"hidden\" name=\"sku\" value=\"").append(esc(l.sku())).append("\">")
              .append("<input type=\"submit\" value=\"Remove\"></form></td></tr>\n");
        }
        sb.append("</table>\n<p>Subtotal ").append(cents(cart.subtotalCents()))
          .append("<br>Discount ").append(cents(cart.discountCents()))
          .append("<br>VAT ").append(cents(cart.vatCents()))
          .append("<br><strong id=\"total\">Total EUR ").append(cents(cart.totalCents())).append("</strong></p>\n");
        for (String n : cart.notes()) sb.append("<p><small>").append(esc(n)).append("</small></p>\n");
        sb.append("<form method=\"post\" action=\"cart\"><input type=\"hidden\" name=\"action\" value=\"promo\">")
          .append("Promotion code <input type=\"text\" name=\"code\" value=\"").append(esc(cart.promo()))
          .append("\" size=\"12\" maxlength=\"20\"><input type=\"submit\" value=\"Apply\"></form>\n")
          .append("<p><a href=\"checkout\">Proceed to checkout</a></p>");
        return sb.toString();
    }

    public static String checkout(Model.Cart cart) {
        if (cart.isEmpty()) return "<h2>Checkout</h2>\n<p>Your cart is empty. <a href=\"catalog\">Browse the catalog</a>.</p>";
        return "<h2>Checkout</h2>\n<p>" + cart.items() + " item(s), total EUR " + cents(cart.totalCents())
                + " including VAT.</p>\n<form method=\"post\" action=\"checkout\">E-mail "
                + "<input type=\"text\" name=\"email\" size=\"30\" maxlength=\"254\">"
                + "<input type=\"submit\" value=\"Place order\"></form>";
    }

    public static String receipt(Model.Order order) {
        StringBuilder sb = new StringBuilder("<h2>Thank you: order <span id=\"order\">")
                .append(esc(order.id())).append("</span></h2>\n<p>Confirmation goes to ")
                .append(esc(order.email())).append(".</p>\n<ul>\n");
        for (Model.OrderLine l : order.lines())
            sb.append("<li>").append(l.qty()).append(" x ").append(esc(l.sku())).append("</li>\n");
        return sb.append("</ul>\n<p>Total EUR ").append(cents(order.totalCents()))
                 .append(" (VAT ").append(cents(order.vatCents())).append(")</p>").toString();
    }
}
