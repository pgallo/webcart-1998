<%@ Language="VBScript" CodePage=65001 %>
<% Option Explicit %>
<script language="VBScript" runat="server" src="lib/validate.vbs"></script>
<script language="VBScript" runat="server" src="lib/format.vbs"></script>
<script language="VBScript" runat="server" src="lib/storefront.vbs"></script>
<!-- #include file="include/store.inc" -->
<!-- #include file="include/header.inc" -->
<%
Dim orderId, found, i
orderId = Request.QueryString("order") & ""
found = False
' Order numbers are sequential, so only the session that placed one may view it.
If IsValidOrderId(orderId) And orderId = Session("LastOrderId") & "" Then found = WebOrder.Load(orderId)
If Not found Then
%>
<p>Order not found.</p>
<% Else %>
<h2>Thank you: order <%= HtmlText(WebOrder.OrderId) %></h2>
<p>Confirmation goes to <%= HtmlText(WebOrder.Email) %>.</p>
<ul>
<% For i = 0 To WebOrder.LineCount - 1 %>
  <li><%= WebOrder.LineQuantity(i) %> x <%= HtmlText(WebOrder.LineSku(i)) %></li>
<% Next %>
</ul>
<p>Total EUR <%= FormatCents(WebOrder.TotalCents) %> (VAT <%= FormatCents(WebOrder.VatCents) %>)</p>
<% End If %>
<!-- #include file="include/footer.inc" -->
