# HTTP server


### this is a refresher project, exploring HTTP servers <br>

starting from a simple HTTP/1.1 server until i expand into HTTP/3 over QUIC
example of an HTTP/1.1 Message Structure:

<table>
  <!-- First line -->
  <tr>
    <td>Method</td>
    <td>Path</td>
    <td>Protocol</td>
    <td>CRLF</td>
  </tr>

  <!-- Headers -->
  <tr>
    <td colspan="3">Content-Length</td>
    <td>CRLF</td>
  </tr>
  <tr>
    <td colspan="3">Connection</td>
    <td>CRLF</td>
  </tr>
  <tr>
    <td colspan="3"> ... </td>
    <td>CRLF</td>
  </tr>

  <!-- Body -->
  <tr>
    <td colspan="3">Body</td>
    <td>CRLF</td>
  </tr>
</table>

STATUS: 

<table>
  <tr>
    <td>HTTP/1.1 parsing</td>
    <td><img src="https://img.shields.io/badge/90%25-brightgreen"></td>
  </tr>
  <tr>
    <td>fast message allocation using memory pool</td>
    <td><img src="https://img.shields.io/badge/70%25-green"></td>
  </tr>
  <tr>
    <td>multithreading</td>
    <td><img src="https://img.shields.io/badge/0%25-red"></td>
  </tr>
  <tr>
    <td>QUIC</td>
    <td><img src="https://img.shields.io/badge/0%25-red"></td>
  </tr>
  <tr>
    <td>HTTP/3 support</td>
    <td><img src="https://img.shields.io/badge/0%25-red"></td>
  </tr>
</table>

