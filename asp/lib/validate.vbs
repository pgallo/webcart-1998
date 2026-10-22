' Input validation shared by the ASP pages, gateway.asp and the cscript
' tests. Plain VBScript, no ASP objects, so it also runs under cscript.
' Every value from Request is treated as untrusted text: coerce with & "",
' match a whitelist pattern, then convert.

Const MAX_LINE_QTY = 99

Function MatchesPattern(value, pattern)
    Dim re
    Set re = New RegExp
    re.Pattern = pattern
    re.IgnoreCase = False
    MatchesPattern = re.Test(CStr(value & ""))
End Function

Function NormalizeSku(value)
    NormalizeSku = UCase(Trim(CStr(value & "")))
End Function

Function IsValidSku(value)
    IsValidSku = MatchesPattern(value, "^[A-Z0-9-]{1,32}$")
End Function

' Returns the quantity as a Long, or -1 if the text is not a whole number
' between minQty and maxQty.
Function ParseQuantity(value, minQty, maxQty)
    Dim text, qty
    text = Trim(CStr(value & ""))
    ParseQuantity = -1
    If Not MatchesPattern(text, "^[0-9]{1,4}$") Then Exit Function
    qty = CLng(text)
    If qty >= minQty And qty <= maxQty Then ParseQuantity = qty
End Function

Function NormalizePromoCode(value)
    NormalizePromoCode = UCase(Trim(CStr(value & "")))
End Function

' Empty means "remove the code".
Function IsValidPromoCode(value)
    IsValidPromoCode = MatchesPattern(value, "^[A-Z0-9]{0,20}$")
End Function

' Same rule as the business tier: one @, a dotted domain, no whitespace or
' markup characters. The server checks again; this only saves a round trip.
Function IsValidEmail(value)
    Dim text
    text = Trim(CStr(value & ""))
    IsValidEmail = Len(text) <= 254 And _
        MatchesPattern(text, "^[^@\s<>""&,]+@[^@\s<>""&,.]+(\.[^@\s<>""&,.]+)+$")
End Function

Function IsValidCartId(value)
    IsValidCartId = MatchesPattern(value, "^C[0-9a-f]{16}$")
End Function

Function IsValidOrderId(value)
    IsValidOrderId = MatchesPattern(value, "^ORD-[0-9]{6,}$")
End Function
