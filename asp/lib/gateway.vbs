' HTTP/XML gateway logic behind gateway.asp: the IIS side of the bridge that
' lets the Java storefront reach the COM business tier. Same protocol as the
' portable webcart-gateway (see core/include/webcart/gateway_protocol.hpp).
' XML documents themselves come from the business tier's ToXml methods.

Function GatewayError(code, message)
    GatewayError = "<response status=""error"" code=""" & HtmlText(code) & """>" & _
                   HtmlText(message) & "</response>"
End Function

Function GatewayAttr(element, name)
    Dim value
    value = element.getAttribute(name)
    If IsNull(value) Then value = ""
    GatewayAttr = CStr(value)
End Function

Function HandleGatewayRequest(requestXml, catalog, cart, order)
    Dim doc, req, op, body, qty, id

    Set doc = CreateObject("MSXML2.DOMDocument.6.0")
    doc.async = False
    doc.validateOnParse = False
    doc.resolveExternals = False
    doc.setProperty "ProhibitDTD", True
    If Not doc.loadXML(CStr(requestXml & "")) Then
        HandleGatewayRequest = GatewayError("BAD_REQUEST", "malformed request: " & Trim(doc.parseError.reason))
        Exit Function
    End If
    Set req = doc.documentElement
    If req.nodeName <> "request" Then
        HandleGatewayRequest = GatewayError("BAD_REQUEST", "root must be <request>")
        Exit Function
    End If
    op = GatewayAttr(req, "op")

    ' Operations on an existing cart bind the handle first.
    Select Case op
        Case "getCart", "addItem", "setQuantity", "removeItem", "applyPromotion"
            id = GatewayAttr(req, "cart")
            If id = "" Then
                HandleGatewayRequest = GatewayError("BAD_REQUEST", "missing attribute cart")
                Exit Function
            End If
            If Not cart.Open(id) Then
                HandleGatewayRequest = GatewayError("UNKNOWN_CART", "no such cart " & id)
                Exit Function
            End If
    End Select
    If op = "addItem" Or op = "setQuantity" Then
        qty = ParseQuantity(GatewayAttr(req, "qty"), 0, 9999)
        If qty < 0 Then
            HandleGatewayRequest = GatewayError("INVALID_QUANTITY", "bad quantity")
            Exit Function
        End If
    End If

    On Error Resume Next
    Select Case op
        Case "catalog"
            body = catalog.ToXml()
        Case "createCart"
            cart.Create
            If Err.Number = 0 Then body = cart.ToXml()
        Case "getCart"
            body = cart.ToXml()
        Case "addItem"
            cart.AddItem GatewayAttr(req, "sku"), qty
            If Err.Number = 0 Then body = cart.ToXml()
        Case "setQuantity"
            cart.SetQuantity GatewayAttr(req, "sku"), qty
            If Err.Number = 0 Then body = cart.ToXml()
        Case "removeItem"
            cart.RemoveItem GatewayAttr(req, "sku")
            If Err.Number = 0 Then body = cart.ToXml()
        Case "applyPromotion"
            cart.ApplyPromotion GatewayAttr(req, "code")
            If Err.Number = 0 Then body = cart.ToXml()
        Case "placeOrder"
            id = GatewayAttr(req, "cart")
            If id = "" Then Err.Raise vbObjectError + 523, "WebCart", "BAD_REQUEST: missing attribute cart"
            If Err.Number = 0 Then order.Place id, GatewayAttr(req, "email")
            If Err.Number = 0 Then body = order.ToXml()
        Case "getOrder"
            id = GatewayAttr(req, "order")
            If Not order.Load(id) Then Err.Raise vbObjectError + 521, "WebCart", "UNKNOWN_ORDER: no such order " & id
            If Err.Number = 0 Then body = order.ToXml()
        Case Else
            Err.Raise vbObjectError + 523, "WebCart", "BAD_REQUEST: unknown op " & op
    End Select
    If Err.Number <> 0 Then
        HandleGatewayRequest = GatewayError(ErrorCodeOf(Err.Description), ErrorMessageOf(Err.Description))
    Else
        HandleGatewayRequest = "<response status=""ok"">" & body & "</response>"
    End If
    Err.Clear
End Function
