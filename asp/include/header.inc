<%
Response.CharSet = "utf-8"
Response.CacheControl = "no-cache"
Response.Expires = -1
%><!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title><%= HtmlText(Application("StoreName")) %></title>
</head>
<body>
<h1><%= HtmlText(Application("StoreName")) %></h1>
<p><a href="default.asp">Catalog</a> | <a href="cart.asp">Cart</a> | <a href="checkout.asp">Checkout</a></p>
<%
Dim flashText
flashText = TakeFlash()
If flashText <> "" Then
%>
<p class="flash"><strong><%= HtmlText(flashText) %></strong></p>
<% End If %>
