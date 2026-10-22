<%
' Per-request business-tier handles. Objects are created per page and
' released at the end of the request; nothing COM goes into Session.
Dim WebCatalog, WebCart, WebOrder
Set WebCatalog = Server.CreateObject("WebCart.Catalog")
Set WebCart = Server.CreateObject("WebCart.Cart")
Set WebOrder = Server.CreateObject("WebCart.Order")

Sub UseSessionCart()
    Session("CartId") = BindCart(WebCart, Session("CartId"))
End Sub

' Post/redirect/get: a message survives exactly one redirect.
Sub SetFlash(message)
    Session("Flash") = message
End Sub

Function TakeFlash()
    TakeFlash = Session("Flash") & ""
    Session("Flash") = ""
End Function
%>
