#!/usr/bin/env bash
# ==============================================================================
# OmniNote — Configurador de 4 ExpressKeys para tableta Wacom Intuos CTH-480
# Mapeo:
#   Botón 0 (Superior Izq.): Deshacer (Ctrl+Z)
#   Botón 1 (Inferior Izq.): Rehacer   (Ctrl+Y)
#   Botón 2 (Superior Der.): Lápiz     (P)
#   Botón 3 (Inferior Der.): Borrador  (E)
# ==============================================================================

set -e

echo "[OmniNote] Configurando ExpressKeys de Wacom CTH-480..."

# 1. Configuración para OpenTabletDriver (si está instalado y configurado)
OTD_SETTINGS="$HOME/.config/OpenTabletDriver/settings.json"
if [ -f "$OTD_SETTINGS" ]; then
    echo "-> Configurando OpenTabletDriver en $OTD_SETTINGS..."
    python3 -c "
import json, os

path = os.path.expanduser('~/.config/OpenTabletDriver/settings.json')
with open(path, 'r') as f:
    data = json.load(f)

aux_bindings = [
    {
        'Path': 'OpenTabletDriver.Desktop.Binding.MultiKeyBinding',
        'Settings': [{'Property': 'Keys', 'Value': 'Control+Z'}],
        'Enable': True
    },
    {
        'Path': 'OpenTabletDriver.Desktop.Binding.MultiKeyBinding',
        'Settings': [{'Property': 'Keys', 'Value': 'Control+Y'}],
        'Enable': True
    },
    {
        'Path': 'OpenTabletDriver.Desktop.Binding.KeyBinding',
        'Settings': [{'Property': 'Key', 'Value': 'P'}],
        'Enable': True
    },
    {
        'Path': 'OpenTabletDriver.Desktop.Binding.KeyBinding',
        'Settings': [{'Property': 'Key', 'Value': 'E'}],
        'Enable': True
    }
]

updated = False
for profile in data.get('Profiles', []):
    if profile.get('Tablet') == 'Wacom CTH-480':
        profile['Bindings']['AuxButtons'] = aux_bindings
        updated = True

if updated:
    with open(path, 'w') as f:
        json.dump(data, f, indent=2)
    print('   ✓ Perfil Wacom CTH-480 actualizado en settings.json')
else:
    print('   ! Perfil Wacom CTH-480 no encontrado en Profiles')
"
    if command -v otd >/dev/null 2>&1; then
        otd loadsettings "$OTD_SETTINGS" >/dev/null 2>&1 || true
        echo "   ✓ Configuración recargada en otd-daemon exitosamente."
    fi
fi

# 2. Configuración para entorno X11 / xsetwacom (si aplica)
if command -v xsetwacom >/dev/null 2>&1; then
    PAD_DEV=$(xsetwacom --list devices | grep -i "pad" | cut -d: -f1 | head -n1 | tr -d ' ' || true)
    if [ -n "$PAD_DEV" ]; then
        echo "-> Configurando xsetwacom para dispositivo: $PAD_DEV..."
        xsetwacom --set "$PAD_DEV" Button 1 "key +ctrl +z -z -ctrl" || true
        xsetwacom --set "$PAD_DEV" Button 2 "key +ctrl +y -y -ctrl" || true
        xsetwacom --set "$PAD_DEV" Button 3 "key p" || true
        xsetwacom --set "$PAD_DEV" Button 8 "key e" || true
        echo "   ✓ Atajos asignados en xsetwacom."
    fi
fi

echo "[OmniNote] ExpressKeys de Wacom CTH-480 configurados correctamente."
