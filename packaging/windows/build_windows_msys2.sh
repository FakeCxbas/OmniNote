#!/usr/bin/env bash
set -euo pipefail

echo "=========================================================="
echo " OmniNote STEM Engine — Windows Native MinGW-w64 Builder"
echo "=========================================================="

BUILD_DIR="build-windows"
DIST_DIR="dist/omninote"

echo "[1/4] Configurando CMake en modo Release..."
cmake -B "${BUILD_DIR}" -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${DIST_DIR}"

echo "[2/4] Compilando OmniNote STEM..."
cmake --build "${BUILD_DIR}" --config Release -j$(nproc)

echo "[2b/4] Ejecutando pruebas unitarias en Windows (offscreen)..."
if [ -f "${BUILD_DIR}/test_omninote.exe" ]; then
    export QT_QPA_PLATFORM=offscreen
    "./${BUILD_DIR}/test_omninote.exe"
fi

echo "[3/4] Desplegando binario y dependencias de Windows..."
rm -rf dist
mkdir -p "${DIST_DIR}"

cp "${BUILD_DIR}/omninote.exe" "${DIST_DIR}/"
cp "src/stem/render_latex.py" "${DIST_DIR}/"

if [ -f "LICENSE" ]; then
    cp "LICENSE" "${DIST_DIR}/"
fi

if [ -f "README.md" ]; then
    cp "README.md" "${DIST_DIR}/"
fi

# Despliegue de plugins y bibliotecas de Qt6
if command -v windeployqt >/dev/null 2>&1; then
    echo "Ejecutando windeployqt..."
    windeployqt --release --no-translations --compiler-runtime "${DIST_DIR}/omninote.exe" || true
fi

# Copia recursiva de DLLs de terceros (Poppler, SQLite3, FreeType, etc.) via ntldd
if command -v ntldd >/dev/null 2>&1; then
    echo "Resolviendo dependencias dinámicas con ntldd..."
    for pass in 1 2 3; do
        for bin in "${DIST_DIR}"/*.exe "${DIST_DIR}"/*.dll; do
            if [ -f "${bin}" ]; then
                ntldd -R "${bin}" 2>/dev/null | grep "/mingw64/bin" | awk '{print $3}' | while read -r dll; do
                    if [ -f "${dll}" ]; then
                        dest="${DIST_DIR}/$(basename "${dll}")"
                        if [ ! -f "${dest}" ]; then
                            cp "${dll}" "${dest}"
                        fi
                    fi
                done
            fi
        done
    done
fi

echo "[4/4] Creando paquete ZIP portátil..."
(
    cd dist
    zip -r ../omninote-windows-x64.zip omninote
)

echo "Paquete generado exitosamente: omninote-windows-x64.zip"
ls -lh omninote-windows-x64.zip
