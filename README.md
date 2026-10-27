# OmniNote STEM Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Qt 6.4+](https://img.shields.io/badge/Qt-6.4%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black)](https://kernel.org)
[![Build Status](https://img.shields.io/badge/Tests-18%2F18%20Passed-brightgreen)](tests/)

OmniNote STEM Engine es un entorno de notas técnicas y cálculo vectorial de alto rendimiento, desarrollado en C++20 sobre Qt 6 (Widgets y OpenGLWidgets), Poppler-Qt6 y SQLite3. Está enfocado en dibujo técnico, matemáticas, programación y física sobre Linux.

---

## Características

### Entrada Digitalizadora y Tabletas Gráficas
- Soporte para tabletas digitalizadoras (Wacom Intuos y compatibles con el subsistema `evdev`/`libinput` de Linux).
- Muestreo de eventos `QTabletEvent` con resolución subpíxel (`QPointF`), presión de punto flotante ($0.0 \to 1.0$) y ángulo de inclinación (`xTilt`, `yTilt`).
- Reconocimiento automático del extremo físico de borrado del lápiz (`QPointingDevice::PointerType::Eraser`).
- Rechazo de palma mediante detección de proximidad (`TabletEnterProximity`), dejando el ratón reservado para panning y zoom.
- Filtro de estabilización de trazos en tiempo real con interpolación de splines de Catmull-Rom para escritura matemática de alta resolución.

### Lienzo Vectorial Acelerado por GPU
- Viewport acelerado por hardware sobre `QOpenGLWidget` con antialiasing multisináptico 4x (MSAA).
- Fondos procedurales ajustables:
  - Cuadrícula de cálculo: líneas secundarias a 25 px y maestras a 125 px con contraste técnico oscuro (`#14161C`).
  - Papel milimétrico: divisiones a 10 px, 50 px y 100 px.
  - Malla isométrica 3D: retícula a 30°/60° para dibujo de proyecciones y cálculo multivariable.
  - Punteado y modo liso.
- Lápiz con calibración de presión y paleta cromática técnica (LaTeX White, Euler Blue, Newton Orange, Asymptote Cyan, etc.).
- Resaltador translúcido ordenado bajo la tinta para no oscurecer fórmulas ni texto importado.
- Regla virtual interactiva con marcas métricas y ajuste angular para trazos rectos de precisión.
- Formas geométricas vectoriales: líneas, rectángulos, elipses y ejes de coordenadas cartesianas 2D y proyecciones 3D.
- Borrador con dos modos: eliminación por trazo completo o borrador parcial por corte de intersección.
- Herramienta Lazo para selección, traslación, escala, duplicación y cambio de color en lote.

### Subsistema STEM
- Bloques de código con resaltado de sintaxis nativo para C++20, Python y pseudocódigo.
- Tarjetas de fórmulas matemáticas LaTeX con previsualización tipográfica en vivo.
- Graficador de funciones en tiempo real mediante un parser descendente recursivo en C++ sin dependencias externas:
  - Operadores aritméticos, funciones trigonométricas, hiperbólicas, logarítmicas, constantes `pi` y `e`, y multiplicación implícita.
  - Detección de singularidades y asíntotas verticales.
- Inserción y escalado de imágenes locales y soporte para pegado directo desde el portapapeles (`Ctrl+V`).

### Sincronización y Respaldo
- Integración con Google Drive mediante OAuth 2.0 PKCE.
- Respaldo transaccional de la base de datos SQLite (`omninote_stem.omninote`).
- Exportación estructurada página por página (`PDFs/[Materia]/[Cuaderno]/Página X.pdf`).
- Exportación del cuaderno completo en PDF vectorial de 300 DPI vía `QPdfWriter`.
- Historial de copias de seguridad fechadas en almacenamiento local y remoto.

### Iconografía
- Iconos vectoriales de la librería Phosphor Icons (Duotone) embebidos directamente en el motor para evitar dependencias del sistema, con soporte completo para monitores de alta densidad (HiDPI).

### Persistencia y Motor Transaccional
- Base de datos relacional SQLite3 con modo WAL y transacciones inmediatas.
- Esquema relacional: `Subjects` -> `Notebooks` -> `Pages` -> `Strokes` y `StemBlocks`.
- Historial de Deshacer / Rehacer basado en `QUndoStack` con comandos atómicos.

---

## Atajos de Teclado

| Tecla | Acción |
| :--- | :--- |
| `P` | Lápiz |
| `H` | Resaltador |
| `E` | Borrador |
| `L` | Lazo de Selección |
| `S` | Herramienta de Formas / Ejes |
| `T` | Bloque de Texto |
| `C` | Bloque de Código |
| `F` | Graficador de Funciones |
| `M` | Fórmula Matemática LaTeX |
| `R` | Activar / Ocultar Regla Virtual |
| `I` | Insertar Imagen |
| `Ctrl + V` | Pegar imagen o texto |
| `Ctrl + S` | Guardar página actual |
| `Ctrl + Z` | Deshacer |
| `Ctrl + Y` | Rehacer |
| `Ctrl + D` | Duplicar elementos seleccionados |
| `Delete` | Eliminar elementos seleccionados |
| `F4` | Alternar panel lateral |
| `F1` | Guía de atajos de teclado |
| `Ctrl + Rueda` | Zoom centrado en el cursor |
| `Botón central / derecho + arrastrar` | Desplazamiento por el lienzo (Panning) |

---

## Requisitos y Compilación

### Dependencias

#### Arch Linux / Manjaro / CachyOS
```bash
sudo pacman -S base-devel cmake gcc qt6-base qt6-declarative qt6-svg poppler-qt6 sqlite
```

#### Ubuntu 24.04+ / Debian 12+
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev libqt6svg6-dev libpoppler-qt6-dev libsqlite3-dev
```

#### Fedora 39+
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel poppler-qt6-devel sqlite-devel
```

### Compilación

```bash
git clone https://github.com/FakeCxbas/OmniNote.git
cd OmniNote
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Pruebas

Para ejecutar la suite de pruebas unitarias automatizadas:

```bash
./build/test_omninote
```

### Ejecución

```bash
./build/OmniNoteSTEM
```

---

## Estructura del Proyecto

```text
OmniNote/
├── assets/
│   └── icons/
│       └── phosphor_duotone/   # Iconos vectoriales oficiales Phosphor Duotone
├── src/
│   ├── main.cpp                # Punto de entrada
│   ├── app/                    # Ventana principal, temas y gestión de iconos
│   ├── input/                  # Manejo de eventos de tableta y suavizado Catmull-Rom
│   ├── canvas/                 # Viewport OpenGL, trazos, lazo y regla virtual
│   ├── stem/                   # Bloques de código, parser matemático, gráficas y LaTeX
│   ├── pdf/                    # Importación y exportación vectorial (Poppler y QPdfWriter)
│   ├── storage/                # Base de datos SQLite3
│   └── sync/                   # Sincronización con Google Drive
├── tests/
│   └── test_omninote.cpp       # Suite de pruebas unitarias
├── CMakeLists.txt              # Configuración de compilación CMake
├── LICENSE                     # Licencia MIT
└── README.md                   # Documentación técnica
```

---

## Licencia

Este proyecto se distribuye bajo los términos de la Licencia MIT a nombre de **Sebastián Zambrano**. Eres libre de usar, modificar y distribuir este software siempre que se conserve el aviso de copyright original. Consulta el archivo [LICENSE](LICENSE) para más información.
