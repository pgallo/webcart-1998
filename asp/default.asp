<%@ Language="VBScript" CodePage=65001 %>
<% Option Explicit %>
<script language="VBScript" runat="server" src="lib/validate.vbs"></script>
<script language="VBScript" runat="server" src="lib/format.vbs"></script>
<script language="VBScript" runat="server" src="lib/storefront.vbs"></script>
<!-- #include file="include/store.inc" -->
<!-- #include file="include/header.inc" -->
<h2>Catalog</h2>
<table border="1" cellpadding="4">
<tr><th>Item</th><th>Category</th><th>Price (net)</th><th>VAT</th><th>Available</th><th></th></tr>
<%
Dim i, sku
For i = 0 To WebCatalog.Count - 1
    sku = WebCatalog.SkuAt(i)
%>
<tr>
  <td><%= HtmlText(WebCatalog.ProductName(sku)) %> <small>(<%= HtmlText(sku) %>)</small></td>
  <td><%= HtmlText(WebCatalog.Category(sku)) %></td>
  <td align="right"><%= FormatCents(WebCatalog.UnitPriceCents(sku)) %></td>
  <td align="right"><%= FormatCents(WebCatalog.VatRateBp(sku)) %>%</td>
  <td align="right"><%= WebCatalog.Available(sku) %></td>
  <td>
    <form method="post" action="cart.asp">
      <input type="hidden" name="action" value="add">
      <input type="hidden" name="sku" value="<%= HtmlText(sku) %>">
      <input type="text" name="qty" value="1" size="2" maxlength="2">
      <input type="submit" value="Add">
    </form>
  </td>
</tr>
<% Next %>
</table>
<!-- #include file="include/footer.inc" -->
