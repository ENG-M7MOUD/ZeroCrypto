Write-Host "[*] Compiling Resources..." -ForegroundColor Cyan


windres info.rc -o info.o


if (-not (Test-Path "info.o")) {
    Write-Host "[!] Resource compilation failed. Continuing without icon..." -ForegroundColor Yellow
}

Write-Host "[*] Compiling ZeroCrypto v8.0..." -ForegroundColor Cyan


g++ -o "ZeroCrypto v8.0.exe" `
    info.o `
    src/main.cpp `
    src/core/VaultRegistry.cpp `
    src/core/SystemUtils.cpp `
    src/Crypto.cpp `
    imgui/imgui.cpp `
    imgui/imgui_draw.cpp `
    imgui/imgui_tables.cpp `
    imgui/imgui_widgets.cpp `
    imgui/backends/imgui_impl_win32.cpp `
    imgui/backends/imgui_impl_dx11.cpp `
    -I. -I./src -I./imgui -I./imgui/backends `
    -ld3d11 -ld3dcompiler -ldwmapi -lcomdlg32 -lole32 -lcrypt32 -mwindows -static

if ($LASTEXITCODE -eq 0) {
    Write-Host "[+] Build Successful! Run ZeroCrypto v8.0.exe" -ForegroundColor Green
    
    Remove-Item info.o -ErrorAction SilentlyContinue
} else {
    Write-Host "[!] Build failed." -ForegroundColor Red
}
