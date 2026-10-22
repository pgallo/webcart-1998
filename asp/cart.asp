<%@ Language="VBScript" CodePage=65001 %>
<% Option Explicit %>
<script language="VBScript" runat="server" src="lib/validate.vbs"></script>
<script language="VBScript" runat="server" src="lib/format.vbs"></script>
<script language="VBScript" runat="server" src="lib/storefront.vbs"></script>
<!-- #include file="include/store.inc" -->
<%
UseSessionCart
If Request.ServerVariables("REQUEST_METHOD") = "POST" Then
    SetFlash ApplyCartAction(WebCart, Request.Form("action"), Request.Form("sku"), _
                             Request.Form("qty"), Request.Form("code"))
    Response.Redirect "cart.asp"
    Response.End
End If
%>
<!-- #include file="include/header.inc" -->
<h2>Your cart</h2>
<% If WebCart.LineCount = 0 Then %>
<p>Your cart is empty.</p>
<% Else %>
<table border="1" cellpadding="4">
<tr><th>Item</th><th>Qty</th><th>Net</th><th>Discount</th><th></th></tr>
<%
Dim i, sku
For i = 0 To WebCart.LineCount - 1
    sku = WebCart.LineSku(i)
%>
<tr>
  <td><%= HtmlText(WebCart.LineName(i)) %></td>
  <td>
    <form method="post" action="cart.asp">
      <input type="hidden" name="action" value="update">
      <input type="hidden" name="sku" value="<%= HtmlText(sku) %>">
      <input type="text" name="qty" value="<%= WebCart.LineQuantity(i) %>" size="2" maxlength="2">
      <input type="submit" value="Update">
    </form>
  </td>
  <td align="right"><%= FormatCents(WebCart.LineNetCents(i)) %></td>
  <td align="right"><%= FormatCents(WebCart.LineDiscountCents(i)) %></td>
  <td>
    <form method="post" action="cart.asp">
      <input type="hidden" name="action" value="remove">
      <input type="hidden" name="sku" value="<%= HtmlText(sku) %>">
      <input type="submit" value="Remove">
    </form>
  </td>
</tr>
<% Next %>
</table>
<p>
Subtotal <%= FormatCents(WebCart.SubtotalCents) %><br>
Discount <%= FormatCents(WebCart.DiscountCents) %><br>
VAT <%= FormatCents(WebCart.VatCents) %><br>
<strong>Total EUR <%= FormatCents(WebCart.TotalCents) %></strong>
</p>
<% If WebCart.Notes <> "" Then %>
<p><small><%= Replace(HtmlText(WebCart.Notes), vbLf, "<br>") %></small></p>
<% End If %>
<form method="post" action="cart.asp">
  <input type="hidden" name="action" value="promo">
  Promotion code <input type="text" name="code" value="<%= HtmlText(WebCart.PromotionCode) %>" size="12" maxlength="20">
  <input type="submit" value="Apply">
</form>
<p><a href="checkout.asp">Proceed to checkout</a></p>
<% End If %>
<!-- #include file="include/footer.inc" -->
