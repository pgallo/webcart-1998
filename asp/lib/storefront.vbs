' Storefront flow on top of the COM business tier. Used by the ASP pages and
' exercised against the real local server by tests\com_smoke.vbs.
' Functions return "" on success or a message for the shopper.

' Binds the cart handle to the session's cart, creating a new one when the
' id is missing or the server has expired it. Returns the id to keep in
' Session: the session holds a string, never a COM object.
Function BindCart(cart, sessionCartId)
    Dim id
    id = CStr(sessionCartId & "")
    If IsValidCartId(id) Then
        If cart.Open(id) Then
            BindCart = id
            Exit Function
        End If
    End If
    BindCart = cart.Create()
End Function

Function ApplyCartAction(cart, action, sku, qtyText, promoCode)
    Dim act, theSku, qty, code
    ApplyCartAction = ""
    act = CStr(action & "")
    theSku = NormalizeSku(sku)
    Select Case act
        Case "add", "update", "remove"
            If Not IsValidSku(theSku) Then
                ApplyCartAction = "That product is no longer available."
                Exit Function
            End If
        Case "promo"
            code = NormalizePromoCode(promoCode)
            If Not IsValidPromoCode(code) Then
                ApplyCartAction = "That promotion code is not valid."
                Exit Function
            End If
        Case Else
            ApplyCartAction = "Unknown action."
            Exit Function
    End Select

    If act = "add" Or act = "update" Then
        If act = "add" Then qty = ParseQuantity(qtyText, 1, MAX_LINE_QTY) Else qty = ParseQuantity(qtyText, 0, MAX_LINE_QTY)
        If qty < 0 Then
            ApplyCartAction = FriendlyMessage("INVALID_QUANTITY: bad quantity")
            Exit Function
        End If
    End If

    On Error Resume Next
    Select Case act
        Case "add": cart.AddItem theSku, qty
        Case "update": cart.SetQuantity theSku, qty
        Case "remove": cart.RemoveItem theSku
        Case "promo": cart.ApplyPromotion code
    End Select
    If Err.Number <> 0 Then ApplyCartAction = FriendlyMessage(Err.Description)
    Err.Clear
End Function

' Places the order; on success orderId receives the new order number.
Function Checkout(order, cartId, email, ByRef orderId)
    orderId = ""
    If Not IsValidEmail(email) Then
        Checkout = FriendlyMessage("INVALID_EMAIL: bad address")
        Exit Function
    End If
    On Error Resume Next
    orderId = order.Place(CStr(cartId & ""), Trim(CStr(email & "")))
    If Err.Number <> 0 Then
        Checkout = FriendlyMessage(Err.Description)
        orderId = ""
    Else
        Checkout = ""
    End If
    Err.Clear
End Function
