package lab.webcart.web;

import jakarta.servlet.http.HttpServlet;
import jakarta.servlet.http.HttpServletRequest;
import jakarta.servlet.http.HttpServletResponse;
import java.io.IOException;
import lab.webcart.store.Html;
import lab.webcart.store.StoreException;
import lab.webcart.store.Validation;

public class CatalogServlet extends HttpServlet {
    @Override
    protected void doGet(HttpServletRequest req, HttpServletResponse resp) throws IOException {
        try {
            Web.render(req, resp, "Catalog", Html.catalog(Web.gateway(getServletContext()).catalog()));
        } catch (StoreException e) {
            log("catalog failed: " + e.code() + " " + e.getMessage());
            resp.setStatus(HttpServletResponse.SC_SERVICE_UNAVAILABLE);
            Web.render(req, resp, "Catalog", "<p>" + Html.esc(Validation.friendly(e)) + "</p>");
        }
    }
}
