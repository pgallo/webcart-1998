package lab.webcart.store;

import java.io.StringReader;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.xml.sax.InputSource;

/** Encodes gateway requests and decodes responses (protocol in core/include/webcart/gateway_protocol.hpp). */
public final class XmlCodec {
    private XmlCodec() {}

    public static String request(String op, String... keyValues) {
        if (keyValues.length % 2 != 0) throw new IllegalArgumentException("key/value pairs expected");
        Map<String, String> attrs = new LinkedHashMap<>();
        attrs.put("op", op);
        for (int i = 0; i < keyValues.length; i += 2) attrs.put(keyValues[i], keyValues[i + 1]);
        StringBuilder sb = new StringBuilder("<request");
        attrs.forEach((k, v) -> sb.append(' ').append(k).append("=\"").append(escape(v)).append('"'));
        return sb.append("/>").toString();
    }

    public static String escape(String s) {
        StringBuilder sb = new StringBuilder(s.length());
        for (char c : s.toCharArray()) {
            switch (c) {
                case '&' -> sb.append("&amp;");
                case '<' -> sb.append("&lt;");
                case '>' -> sb.append("&gt;");
                case '"' -> sb.append("&quot;");
                case '\'' -> sb.append("&apos;");
                default -> sb.append(c < 0x20 && c != '\t' ? '?' : c);
            }
        }
        return sb.toString();
    }

    /** Returns the payload element of an ok response, or throws the business error it carries. */
    public static Element payload(String responseXml) throws StoreException {
        Element root;
        try {
            DocumentBuilderFactory f = DocumentBuilderFactory.newInstance();
            f.setFeature("http://apache.org/xml/features/disallow-doctype-decl", true);
            f.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, true);
            f.setXIncludeAware(false);
            f.setExpandEntityReferences(false);
            DocumentBuilder builder = f.newDocumentBuilder();
            builder.setErrorHandler(null);  // report through the exception, not stderr
            root = builder.parse(new InputSource(new StringReader(responseXml))).getDocumentElement();
        } catch (Exception e) {
            throw new StoreException("BAD_RESPONSE", "unparseable gateway response", e);
        }
        if (!"response".equals(root.getTagName())) throw new StoreException("BAD_RESPONSE", "expected <response>");
        if ("error".equals(root.getAttribute("status")))
            throw new StoreException(root.getAttribute("code"), root.getTextContent());
        List<Element> children = children(root, null);
        if (children.size() != 1) throw new StoreException("BAD_RESPONSE", "expected one payload element");
        return children.get(0);
    }

    public static List<Model.Product> catalog(Element e) {
        List<Model.Product> out = new ArrayList<>();
        for (Element p : children(e, "product"))
            out.add(new Model.Product(p.getAttribute("sku"), p.getAttribute("name"), p.getAttribute("category"),
                    num(p, "unitCents"), (int) num(p, "vatBp"), (int) num(p, "available")));
        return out;
    }

    public static Model.Cart cart(Element e) {
        List<Model.Line> lines = new ArrayList<>();
        for (Element l : children(e, "line"))
            lines.add(new Model.Line(l.getAttribute("sku"), l.getAttribute("name"), (int) num(l, "qty"),
                    num(l, "unitCents"), num(l, "netCents"), num(l, "discountCents"), (int) num(l, "vatBp"),
                    !"false".equals(l.getAttribute("reserved"))));
        List<String> notes = new ArrayList<>();
        for (Element n : children(e, "note")) notes.add(n.getTextContent());
        Element t = children(e, "totals").get(0);
        return new Model.Cart(e.getAttribute("id"), e.getAttribute("promo"), List.copyOf(lines),
                (int) num(t, "items"), num(t, "subtotalCents"), num(t, "discountCents"), num(t, "vatCents"),
                num(t, "totalCents"), List.copyOf(notes));
    }

    public static Model.Order order(Element e) {
        List<Model.OrderLine> lines = new ArrayList<>();
        for (Element l : children(e, "line"))
            lines.add(new Model.OrderLine(l.getAttribute("sku"), (int) num(l, "qty"), num(l, "unitCents"),
                    num(l, "discountCents")));
        return new Model.Order(e.getAttribute("id"), e.getAttribute("email"), e.getAttribute("promo"),
                num(e, "subtotalCents"), num(e, "discountCents"), num(e, "vatCents"), num(e, "totalCents"),
                List.copyOf(lines));
    }

    private static long num(Element e, String attr) {
        String v = e.getAttribute(attr);
        return v.isEmpty() ? 0 : Long.parseLong(v);
    }

    private static List<Element> children(Element parent, String name) {
        List<Element> out = new ArrayList<>();
        for (Node n = parent.getFirstChild(); n != null; n = n.getNextSibling())
            if (n instanceof Element el && (name == null || name.equals(el.getTagName()))) out.add(el);
        return out;
    }
}
