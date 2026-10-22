' Late-bound smoke test of the registered COM local server, the way ASP and
' scripts use it: CreateObject by ProgID, IDispatch calls, Err mapping. It
' also runs the storefront flow and the ASP gateway logic (asp\lib) against
' the real objects.
'   cscript //nologo tests\com_smoke.vbs
Option Explicit

Dim fso, root, checks, failures
Set fso = CreateObject("Scripting.FileSystemObject")
root = fso.GetParentFolderName(fso.GetParentFolderName(WScript.ScriptFullName))
checks = 0
failures = 0

Sub Include(relativePath)
    Dim file
    Set file = fso.OpenTextFile(fso.BuildPath(root, relativePath), 1)
    ExecuteGlobal file.ReadAll()
    file.Close
End Sub

Sub Check(condition, label)
    checks = checks + 1
    If Not condition Then
        failures = failures + 1
        WScript.Echo "FAIL " & label
    End If
End Sub

Function ParseXml(text)
    Dim doc
    Set doc = CreateObject("MSXML2.DOMDocument.6.0")
    doc.async = False
    If Not doc.loadXML(text) Then
        WScript.Echo "unparseable XML: " & text
        Set ParseXml = Nothing
    Else
        Set ParseXml = doc.documentElement
    End If
End Function

Include "asp\lib\validate.vbs"
Include "asp\lib\format.vbs"
Include "asp\lib\storefront.vbs"
Include "asp\lib\gateway.vbs"

' ---- catalog -------------------------------------------------------------
Dim catalog, value
Set catalog = CreateObject("WebCart.Catalog")
Check catalog.Count = 8, "catalog count"
Check catalog.SkuAt(0) = "TEA-001", "first sku"
Check catalog.UnitPriceCents("TEA-001") = 650, "price"
Check catalog.VatRateBp("BOOK-100") = 400, "vat rate"
Check catalog.Category("POT-020") = "kitchen", "category"
On Error Resume Next
value = catalog.UnitPriceCents("NOPE-1")
Check Err.Number = &H80040201, "UNKNOWN_SKU HRESULT, got " & Hex(Err.Number)
Check ErrorCodeOf(Err.Description) = "UNKNOWN_SKU", "UNKNOWN_SKU description: " & Err.Description
Err.Clear
On Error GoTo 0

' ---- storefront flow (what cart.asp and checkout.asp do) ----------------
Dim cart, cartId, message
Set cart = CreateObject("WebCart.Cart")
cartId = BindCart(cart, "")
Check IsValidCartId(cartId), "new cart id " & cartId
Check ApplyCartAction(cart, "add", "TEA-001", "3", "") = "", "add tea"
message = ApplyCartAction(cart, "promo", "", "", "welcome10")
Check InStr(message, "30.00") > 0, "promotion below minimum: " & message
Check ApplyCartAction(cart, "add", "BOOK-100", "1", "") = "", "add book"
message = ApplyCartAction(cart, "add", "KIT-500", "3", "")
Check Left(message, 6) = "Sorry:" And InStr(message, "KIT-500") > 0, "out of stock message: " & message
Check ApplyCartAction(cart, "promo", "", "", "books5") = "", "promo BOOKS5"
Check cart.LineCount = 2, "line count"
Check cart.SubtotalCents = 3750, "subtotal " & cart.SubtotalCents
Check cart.DiscountCents = 1150, "discount " & cart.DiscountCents
Check cart.VatCents = 182, "vat " & cart.VatCents
Check cart.TotalCents = 2782, "total " & cart.TotalCents
Check cart.PromotionCode = "BOOKS5", "promotion code"
Check InStr(cart.Notes, "3 for 2") > 0, "multibuy note"

' A second handle (the next request) finds the same cart by id.
Dim cart2
Set cart2 = CreateObject("WebCart.Cart")
Check BindCart(cart2, cartId) = cartId, "rebind by id"
Check cart2.LineCount = 2, "rebound cart lines"

Dim cartXml
Set cartXml = ParseXml(cart.ToXml())
Check cartXml.getAttribute("id") = cartId, "cart xml id"
Check cartXml.selectSingleNode("totals").getAttribute("totalCents") = "2782", "cart xml total"

Dim order, orderId
Set order = CreateObject("WebCart.Order")
Check Checkout(order, cartId, "not-an-email", orderId) = "Please enter a valid e-mail address.", "checkout bad email"
message = Checkout(order, cartId, "vbs.smoke@example.org", orderId)
Check message = "", "checkout: " & message
Check IsValidOrderId(orderId), "order id " & orderId
Check order.TotalCents = 2782, "order total"
Check order.LineCount = 2, "order lines"
Check Not cart2.Open(cartId), "cart closed after order"

' ---- ASP gateway logic (what gateway.asp does) ----------------------------
Dim gCart, gOrder, resp, gId
Set gCart = CreateObject("WebCart.Cart")
Set gOrder = CreateObject("WebCart.Order")

Set resp = ParseXml(HandleGatewayRequest("<request op=""catalog""/>", catalog, gCart, gOrder))
Check resp.getAttribute("status") = "ok", "gateway catalog"
Check resp.selectNodes("catalog/product").length = 8, "gateway catalog products"

Set resp = ParseXml(HandleGatewayRequest("<request op=""createCart""/>", catalog, gCart, gOrder))
gId = resp.selectSingleNode("cart").getAttribute("id")
Check IsValidCartId(gId), "gateway createCart " & gId

Set resp = ParseXml(HandleGatewayRequest("<request op=""addItem"" cart=""" & gId & """ sku=""TEA-002"" qty=""2""/>", catalog, gCart, gOrder))
Check resp.getAttribute("status") = "ok", "gateway addItem"
Check resp.selectSingleNode("cart/totals").getAttribute("totalCents") = "1584", "gateway total"

Set resp = ParseXml(HandleGatewayRequest("<request op=""addItem"" cart=""" & gId & """ sku=""KIT-500"" qty=""3""/>", catalog, gCart, gOrder))
Check resp.getAttribute("code") = "OUT_OF_STOCK", "gateway out of stock"

Set resp = ParseXml(HandleGatewayRequest("<request op=""addItem"" cart=""" & gId & """ sku=""TEA-002"" qty=""-1""/>", catalog, gCart, gOrder))
Check resp.getAttribute("code") = "INVALID_QUANTITY", "gateway bad qty"

Set resp = ParseXml(HandleGatewayRequest("<request op=""getCart"" cart=""Cnope""/>", catalog, gCart, gOrder))
Check resp.getAttribute("code") = "UNKNOWN_CART", "gateway unknown cart"

Set resp = ParseXml(HandleGatewayRequest("<request op=", catalog, gCart, gOrder))
Check resp.getAttribute("code") = "BAD_REQUEST", "gateway malformed"

Set resp = ParseXml(HandleGatewayRequest("<request op=""placeOrder"" cart=""" & gId & """ email=""gw@example.org""/>", catalog, gCart, gOrder))
Check resp.getAttribute("status") = "ok", "gateway placeOrder"
orderId = resp.selectSingleNode("order").getAttribute("id")

Set resp = ParseXml(HandleGatewayRequest("<request op=""getOrder"" order=""" & orderId & """/>", catalog, gCart, gOrder))
Check resp.selectSingleNode("order").getAttribute("email") = "gw@example.org", "gateway getOrder"

WScript.Echo "com_smoke: " & checks & " checks, " & failures & " failures"
WScript.Quit failures
