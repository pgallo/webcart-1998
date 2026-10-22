' Output encoding and money formatting shared by pages and tests.

' 123456 -> "1234.56"; amounts are integer cents end to end.
Function FormatCents(cents)
    Dim amount, text
    amount = CLng(cents)
    text = CStr(Abs(amount) \ 100) & "." & Right("0" & CStr(Abs(amount) Mod 100), 2)
    If amount < 0 Then text = "-" & text
    FormatCents = text
End Function

' Encodes text for an HTML body or a quoted attribute (also valid for XML).
Function HtmlText(value)
    Dim s
    s = CStr(value & "")
    s = Replace(s, "&", "&amp;")
    s = Replace(s, "<", "&lt;")
    s = Replace(s, ">", "&gt;")
    s = Replace(s, """", "&quot;")
    s = Replace(s, "'", "&#39;")
    HtmlText = s
End Function

' Business errors arrive as Err.Description "CODE: message".
Function ErrorCodeOf(description)
    Dim text, colon
    text = CStr(description & "")
    colon = InStr(text, ": ")
    ErrorCodeOf = "INTERNAL"
    If colon > 1 Then
        If MatchesPattern(Left(text, colon - 1), "^[A-Z_]+$") Then ErrorCodeOf = Left(text, colon - 1)
    End If
End Function

Function ErrorMessageOf(description)
    Dim text, colon
    text = CStr(description & "")
    colon = InStr(text, ": ")
    If colon > 1 And ErrorCodeOf(text) <> "INTERNAL" Then text = Mid(text, colon + 2)
    ErrorMessageOf = text
End Function

' What the shopper sees. Internal details stay in the server log.
Function FriendlyMessage(description)
    Select Case ErrorCodeOf(description)
        Case "OUT_OF_STOCK", "PROMOTION_NOT_APPLICABLE"
            FriendlyMessage = "Sorry: " & ErrorMessageOf(description) & "."
        Case "UNKNOWN_PROMOTION"
            FriendlyMessage = "That promotion code is not valid."
        Case "INVALID_QUANTITY"
            FriendlyMessage = "Please enter a quantity between 1 and " & MAX_LINE_QTY & "."
        Case "INVALID_EMAIL"
            FriendlyMessage = "Please enter a valid e-mail address."
        Case "EMPTY_CART"
            FriendlyMessage = "Your cart is empty."
        Case "UNKNOWN_CART"
            FriendlyMessage = "Your cart has expired. Please add the items again."
        Case "UNKNOWN_SKU"
            FriendlyMessage = "That product is no longer available."
        Case Else
            FriendlyMessage = "The shop is temporarily unavailable. Please try again."
    End Select
End Function
