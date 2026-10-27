#!/usr/bin/env bash
# ==============================================================================
# OmniNote — Desinstalador de Escritorio (Espacio de Usuario)
# Elimina los archivos instalados en ~/.local
# ==============================================================================

set -e

PREFIX="${HOME}/.local"

echo "[OmniNote] Desinstalando OmniNote de ${PREFIX}..."

rm -f "${PREFIX}/bin/omninote"
rm -f "${PREFIX}/share/applications/org.omninote.OmniNote.desktop"
rm -f "${PREFIX}/share/icons/hicolor/scalable/apps/org.omninote.OmniNote.svg"

for size in 16 24 32 48 64 128 256 512; do
    rm -f "${PREFIX}/share/icons/hicolor/${size}x${size}/apps/org.omninote.OmniNote.png"
done

if command -v update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" 2>/dev/null || true
fi

if command -v gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${PREFIX}/share/icons/hicolor" 2>/dev/null || true
fi

echo "[OmniNote] Desinstalación completada con éxito."
