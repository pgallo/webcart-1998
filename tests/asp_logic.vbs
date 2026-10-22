' Runs the ASP pages' shared VBScript (asp\lib) under cscript, without IIS
' or the COM server: validation, encoding, error mapping, and the cart flow
' against a scripted fake cart.
'   cscript //nologo tests\asp_logic.vbs
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

Include "asp\lib\validate.vbs"
Include "asp\lib\format.vbs"
Include "asp\lib\storefront.vbs"

' A stand-in for WebCart.Cart that records calls and can fail like the
' business tier does (Err.Description "CODE: message").
Class FakeCart
    Public Calls, NextError
    Private Sub Class_Initialize()
        Calls = ""
        NextError = ""
    End Sub
    Private Sub Record(text)
        Calls = Calls & text & ";"
        If NextError <> "" Then
            Dim e
            e = NextError
            NextError = ""
            Err.Raise vbObjectError + 516, "WebCart", e
        End If
    End Sub
    Public Function Open(id)
        Open = (id = "C0123456789abcdef")
    End Function
    Public Function Create()
        Create = "Cfedcba9876543210"
    End Function
    Public Sub AddItem(sku, qty)
        Record "add " & sku & " " & qty
    End Sub
    Public Sub SetQuantity(sku, qty)
        Record "set " & sku & " " & qty
    End Sub
    Public Sub RemoveItem(sku)
        Record "remove " & sku
    End Sub
    Public Sub ApplyPromotion(code)
        Record "promo " & code
    End Sub
End Class

' validation
Check IsValidSku("TEA-001"), "sku ok"
Check Not IsValidSku("tea-001"), "sku lower case"
Check Not IsValidSku("TEA<1"), "sku markup"
Check Not IsValidSku(""), "sku empty"
Check NormalizeSku(" tea-001 ") = "TEA-001", "sku normalize"
Check ParseQuantity("3", 1, 99) = 3, "qty 3"
Check ParseQuantity(" 12 ", 1, 99) = 12, "qty trimmed"
Check ParseQuantity("0", 1, 99) = -1, "qty below min"
Check ParseQuantity("100", 1, 99) = -1, "qty above max"
Check ParseQuantity("1e2", 1, 99) = -1, "qty exponent"
Check ParseQuantity("-1", 0, 99) = -1, "qty negative"
Check ParseQuantity(Empty, 1, 99) = -1, "qty empty"
Check IsValidEmail("a.shopper@example.org"), "email ok"
Check Not IsValidEmail("a@b"), "email no dot"
Check Not IsValidEmail("a b@example.org"), "email space"
Check Not IsValidEmail("<x>@example.org"), "email markup"
Check Not IsValidEmail("a@@example.org"), "email two at"
Check IsValidPromoCode("WELCOME10"), "promo ok"
Check IsValidPromoCode(""), "promo empty clears"
Check Not IsValidPromoCode("BAD CODE"), "promo space"
Check IsValidCartId("C0123456789abcdef"), "cart id"
Check Not IsValidCartId("C0123"), "cart id short"
Check IsValidOrderId("ORD-000042"), "order id"

' encoding and money
Check FormatCents(123456) = "1234.56", "cents"
Check FormatCents(5) = "0.05", "cents small"
Check FormatCents(-5) = "-0.05", "cents negative"
Check FormatCents(0) = "0.00", "cents zero"
Check HtmlText("<a href=""x"">&'") = "&lt;a href=&quot;x&quot;&gt;&amp;&#39;", "html encode"

' error mapping
Check ErrorCodeOf("OUT_OF_STOCK: only 1 left of POT-020") = "OUT_OF_STOCK", "error code"
Check ErrorMessageOf("OUT_OF_STOCK: only 1 left of POT-020") = "only 1 left of POT-020", "error message"
Check ErrorCodeOf("Type mismatch") = "INTERNAL", "foreign error"
Check FriendlyMessage("OUT_OF_STOCK: only 1 left of POT-020") = "Sorry: only 1 left of POT-020.", "friendly stock"
Check FriendlyMessage("INTERNAL: disk on fire") = "The shop is temporarily unavailable. Please try again.", "friendly internal"

' cart flow against the fake
Dim cart, message
Set cart = New FakeCart
Check BindCart(cart, "") = "Cfedcba9876543210", "bind creates"
Check BindCart(cart, "C0123456789abcdef") = "C0123456789abcdef", "bind reopens"
Check BindCart(cart, "C1111111111111111") = "Cfedcba9876543210", "bind expired creates"
Check BindCart(cart, "<script>") = "Cfedcba9876543210", "bind rejects junk id"

Check ApplyCartAction(cart, "add", "tea-001", "2", "") = "", "add ok"
Check ApplyCartAction(cart, "add", "TEA-001", "0", "") = "Please enter a quantity between 1 and 99.", "add zero"
Check ApplyCartAction(cart, "update", "TEA-001", "0", "") = "", "update to zero"
Check ApplyCartAction(cart, "remove", "TEA-001", "", "") = "", "remove"
Check ApplyCartAction(cart, "promo", "", "", " welcome10 ") = "", "promo"
Check ApplyCartAction(cart, "promo", "", "", "bad code!") = "That promotion code is not valid.", "promo junk"
Check ApplyCartAction(cart, "drop table", "TEA-001", "1", "") = "Unknown action.", "unknown action"
Check ApplyCartAction(cart, "add", "TEA<1", "1", "") = "That product is no longer available.", "bad sku"
Check cart.Calls = "add TEA-001 2;set TEA-001 0;remove TEA-001;promo WELCOME10;", "calls: " & cart.Calls
cart.NextError = "OUT_OF_STOCK: only 1 left of POT-020"
message = ApplyCartAction(cart, "add", "POT-020", "2", "")
Check message = "Sorry: only 1 left of POT-020.", "stock error surfaced: " & message
Check Err.Number = 0, "error cleared"

WScript.Echo "asp_logic: " & checks & " checks, " & failures & " failures"
WScript.Quit failures
