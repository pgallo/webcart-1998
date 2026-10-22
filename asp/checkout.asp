<%@ Language="VBScript" CodePage=65001 %>
<% Option Explicit %>
<script language="VBScript" runat="server" src="lib/validate.vbs"></script>
<script language="VBScript" runat="server" src="lib/format.vbs"></script>
<script language="VBScript" runat="server" src="lib/storefront.vbs"></script>
<!-- #include file="include/store.inc" -->
<%
UseSessionCart
Dim message, newOrderId
If Request.ServerVariables("REQUEST_METHOD") = "POST" Then
    message = Checkout(WebOrder, Session("CartId"), Request.Form("email"), newOrderId)
    If message = "" Then
        Session("CartId") = ""
        Session("LastOrderId") = newOrderId
        Response.Redirect "receipt.asp?order=" & Server.URLEncode(newOrderId)
    Else
        SetFlash message
        Response.Redirect "checkout.asp"
    End If
    Response.End
End If
%>
<!-- #include file="include/header.inc" -->
<h2>Checkout</h2>
<% If WebCart.LineCount = 0 Then %>
<p>Your cart is empty. <a href="default.asp">Browse the catalog</a>.</p>
<% Else %>
<p><%= WebCart.LineCount %> line(s), total EUR <%= FormatCents(WebCart.TotalCents) %> including VAT.</p>
<form method="post" action="checkout.asp">
  E-mail <input type="text" name="email" size="30" maxlength="254">
  <input type="submit" value="Place order">
</form>
<% End If %>
<!-- #include file="include/footer.inc" -->
