<%@ Language="VBScript" CodePage=65001 %>
<% Option Explicit %>
<script language="VBScript" runat="server" src="lib/validate.vbs"></script>
<script language="VBScript" runat="server" src="lib/format.vbs"></script>
<script language="VBScript" runat="server" src="lib/gateway.vbs"></script>
<!-- #include file="include/store.inc" -->
<%
' POST /webcart/gateway.asp with <request op="..."/>; see lib/gateway.vbs.
Dim requestBytes, requestText, stream
Response.ContentType = "text/xml"
Response.CharSet = "utf-8"
Response.CacheControl = "no-store"
If Request.ServerVariables("REQUEST_METHOD") <> "POST" Or Request.TotalBytes > 65536 Then
    Response.Status = "400 Bad Request"
    Response.Write GatewayError("BAD_REQUEST", "POST at most 64 KB of XML")
    Response.End
End If
requestBytes = Request.BinaryRead(Request.TotalBytes)
Set stream = Server.CreateObject("ADODB.Stream")
stream.Type = 1 ' binary
stream.Open
stream.Write requestBytes
stream.Position = 0
stream.Type = 2 ' text
stream.Charset = "utf-8"
requestText = stream.ReadText()
stream.Close
Response.Write HandleGatewayRequest(requestText, WebCatalog, WebCart, WebOrder)
%>
