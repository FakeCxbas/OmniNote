#!/usr/bin/env bash
# ==============================================================================
# OmniNote — Instalador de Escritorio para Linux (Espacio de Usuario)
# Instala el binario en ~/.local/bin y la integración de escritorio en ~/.local/share
# No requiere permisos de administrador (sudo).
# ==============================================================================

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${PROJECT_DIR}/build"
PREFIX="${HOME}/.local"

echo "=========================================================="
echo " OmniNote STEM Engine — Instalador de Aplicación Nativa"
echo "=========================================================="
echo "-> Directorio del proyecto: ${PROJECT_DIR}"
echo "-> Prefijo de instalación:   ${PREFIX}"
echo ""

# 1. Configuración y compilación en modo Release
echo "[1/4] Configurando y compilando OmniNote..."
cmake -B "${BUILD_DIR}" -S "${PROJECT_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="${PREFIX}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"

# 2. Ejecución de pruebas automatizadas previas a la instalación
echo "[2/4] Verificando subsistemas mediante suite de pruebas..."
"${BUILD_DIR}/test_omninote"

# 3. Instalación de binarios, entrada de escritorio e iconos
echo "[3/4] Instalando archivos en ${PREFIX}..."
cmake --install "${BUILD_DIR}" --prefix "${PREFIX}"

# 4. Actualización de bases de datos de escritorio e iconos
echo "[4/4] Actualizando cachés de sistema de escritorio..."
if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true
fi

echo ""
echo "=========================================================="
echo " OmniNote se ha instalado exitosamente como app de escritorio."
echo "=========================================================="
echo ""
echo "-> Ya puedes abrir OmniNote directamente desde tu lanzador de"
echo "   aplicaciones (Wofi, Rofi, menú de inicio de Hyprland/GNOME/KDE)"
echo "   o escribiendo 'omninote' en tu terminal."
echo ""
if [[ ":$PATH:" != *":${HOME}/.local/bin:"* ]]; then
    echo "NOTA: Asegúrate de que ~/.local/bin se encuentre en tu variable PATH."
fi
