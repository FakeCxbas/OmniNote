# Compilación y Ejecución en Windows (x64)

OmniNote STEM cuenta con soporte de compilación nativa para Windows a través del entorno MSYS2 MinGW-w64 con Qt6, Poppler y SQLite3.

## 1. Descarga del Ejecutable Precompilado (.exe)

Los paquetes binarios portátiles para Windows se generan automáticamente en cada versión y cambio mediante GitHub Actions:
1. Accede a la pestaña **Actions** en el repositorio de GitHub.
2. Selecciona la ejecución del workflow **Build Windows Executable**.
3. Descarga el artefacto `omninote-windows-x64.zip`.
4. Descomprime el archivo y ejecuta `omninote.exe`.

El paquete incluye todas las bibliotecas de tiempo de ejecución necesarias (Qt6, Poppler, SQLite3, FreeType y MinGW runtime) de forma autónoma.

## 2. Compilación Manual con MSYS2 (MinGW64)

Si deseas compilar el ejecutable localmente en Windows:

1. Instala [MSYS2](https://www.msys2.org/).
2. Abre la terminal **MSYS2 MINGW64**.
3. Instala la cadena de herramientas y dependencias:
   ```bash
   pacman -Syu --needed \
       git zip \
       mingw-w64-x86_64-toolchain \
       mingw-w64-x86_64-cmake \
       mingw-w64-x86_64-ninja \
       mingw-w64-x86_64-pkgconf \
       mingw-w64-x86_64-qt6-base \
       mingw-w64-x86_64-qt6-svg \
       mingw-w64-x86_64-qt6-tools \
       mingw-w64-x86_64-poppler-qt6 \
       mingw-w64-x86_64-sqlite3 \
       mingw-w64-x86_64-ntldd
   ```
4. Clona el repositorio y ejecuta el script de empaquetado:
   ```bash
   git clone https://github.com/FakeCxbas/OmniNote.git
   cd OmniNote
   bash packaging/windows/build_windows_msys2.sh
   ```
5. El archivo portátil `omninote-windows-x64.zip` y la carpeta `dist/omninote/` contendrán `omninote.exe` con todas sus dependencias dinámicas resueltas.
