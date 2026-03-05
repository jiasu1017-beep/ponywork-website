$listener = New-Object System.Net.HttpListener
$listener.Prefixes.Add("http://localhost:8080/")
$listener.Start()
Write-Host "Server running at http://localhost:8080/"
Write-Host "Serving files from: $PSScriptRoot"
Write-Host "Press Ctrl+C to stop"

try {
    while ($listener.IsListening) {
        $context = $listener.GetContext()
        $request = $context.Request
        $response = $context.Response
        
        $url = $request.Url.LocalPath
        if ($url -eq "/") { $url = "/index.html" }
        
        $filePath = Join-Path $PSScriptRoot $url
        
        if (Test-Path $filePath) {
            $content = Get-Content $filePath -Raw -Encoding UTF8
            $contentType = switch -Regex ($filePath) {
                "\.html$" { "text/html; charset=utf-8" }
                "\.css$" { "text/css; charset=utf-8" }
                "\.js$" { "application/javascript; charset=utf-8" }
                "\.json$" { "application/json" }
                "\.png$" { "image/png" }
                "\.jpg$" { "image/jpeg" }
                "\.ico$" { "image/x-icon" }
                "\.svg$" { "image/svg+xml" }
                default { "text/plain" }
            }
            
            $response.ContentType = $contentType
            $buffer = [System.Text.Encoding]::UTF8.GetBytes($content)
            $response.ContentLength64 = $buffer.Length
            $response.OutputStream.Write($buffer, 0, $buffer.Length)
        } else {
            $response.StatusCode = 404
            $content = "<h1>404 - File Not Found</h1>"
            $buffer = [System.Text.Encoding]::UTF8.GetBytes($content)
            $response.ContentType = "text/html"
            $response.OutputStream.Write($buffer, 0, $buffer.Length)
        }
        
        $response.Close()
    }
} finally {
    $listener.Stop()
    $listener.Close()
}
