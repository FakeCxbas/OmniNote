# OmniNote STEM Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![Qt 6.4+](https://img.shields.io/badge/Qt-6.4%2B-41CD52?logo=qt&logoColor=white)](https://www.qt.io/)
[![Platform: Linux](https://img.shields.io/badge/Platform-Linux-FCC624?logo=linux&logoColor=black)](https://kernel.org)
[![Build Status](https://img.shields.io/badge/Tests-18%2F18%20Passed-brightgreen)](tests/)

**OmniNote STEM Engine** es un entorno de notas técnicas y cálculo vectorial de alto rendimiento, 100% nativo en **C++20** utilizando **Qt 6 (Widgets + OpenGLWidgets)**, **Poppler-Qt6** y **SQLite3**, diseñado específicamente para estudiantes, profesores e investigadores en Ciencias de la Computación, Matemáticas y Física.

---

## 🌟 Características Principales

### 1. Entrada Digitalizadora Wacom de Baja Latencia
- **Soporte Wacom de Primera Clase (Intuos CTH-480 y tabletas gráficas Linux):**
  - Intercepción directa de `QTabletEvent` con precisión sub-píxel (`QPointF`).
  - Muestreo continuo de presión real ($0.0 \to 1.0$) y ángulo de inclinación (`xTilt`, `yTilt`).
  - Detección automática del borrador físico del lápiz (`QPointingDevice::PointerType::Eraser` vs `PointerType::Pen`).
  - **Rechazo de Palma (Palm Rejection):** Exclusión de eventos de ratón sintéticos en proximidad (`TabletEnterProximity`), reservando el ratón exclusivamente para desplazamiento (*Panning* con botón central o derecho) y zoom suave (`Ctrl + Rueda`).
- **Motor Matemático de Trazos (`StrokeSmoother`):**
  - Filtro adaptativo de baja latencia para eliminar el micro-temblor al trazar símbolos matemáticos pequeños ($\lim_{x\to 0}$, $\int$, $\sum$, subíndices).
  - Interpolación mediante **Splines de Catmull-Rom** con cálculo de vectores normales ortogonales para renderizar listones vectoriales de ancho dinámico según la presión.
  - Estabilización y embellecimiento automático estilo Canva con ajuste geométrico de líneas rectas.

### 2. Lienzo Vectorial Acelerado por GPU
- **Renderizado por Hardware:** `QGraphicsView` con viewport en `QOpenGLWidget` con antialiasing multisináptico 4x (MSAA).
- **Fondos de Ingeniería Procedurales:**
  - **Cuadrícula de Cálculo:** Líneas secundarias cada 25 px y líneas maestras cada 125 px en tema oscuro descansado (`#14161C`).
  - **Papel Milimétrico:** Divisiones a 10 px, 50 px y 100 px.
  - **Malla Isométrica 3D:** Retícula triangular a 30°/60° para dibujo técnico y cálculo multivariable.
  - **Punteado (Dotted):** Matriz de puntos a 25 px.
  - **Blanco / Liso:** Modo despejado.
- **Herramientas de Geometría y Trazado:**
  - Lápiz sensible a la presión con paleta rápida de colores matemáticos (Blanco LaTeX, Azul Euler, Verde, Naranja Newton, Magenta Cálculo, Cian Asíntotas).
  - Resaltador translúcido ordenado bajo la tinta (`Z-Ordering` inferior) para no ocultar fórmulas ni texto PDF.
  - **Regla Virtual STEM:** Herramienta interactiva con marcas milimétricas, proyección de eje láser y ajuste angular para líneas de precisión matemática exacta.
  - Generador de formas: Línea recta, ejes cartesianos 2D con flechas y marcas de escala, ejes isométricos 3D, rectángulos y elipses.
  - **Borrador Dual:**
    - *Borrador de Trazo:* Elimina el trazo vectorial completo por colisión.
    - *Borrador de Corte/Parcial:* Divide el trazo en sub-trazos independientes en el punto de intersección.
  - **Herramienta Lazo:** Selección a mano alzada para mover, rotar, escalar, cambiar color, duplicar o borrar grupos de ecuaciones o bloques de texto.

### 3. Subsistema Especializado STEM
- **Fórmulas Matemáticas LaTeX (`FormulaBlockItem`):**
  - Tarjetas flotantes con fórmulas matemáticas LaTeX renderizadas tipográficamente.
  - Edición en vivo con doble clic y previsualización inmediata.
- **Bloques de Código Embebidos (`CodeBlockItem`):**
  - Cajas de código flotantes con numeración de líneas y redimensionamiento dinámico.
  - `QSyntaxHighlighter` nativo con soporte para **C++20, Python y Pseudocódigo algorítmico**.
  - Botón de copia al portapapeles y cabecera arrastrable.
- **Graficador Matemático Rápido (`FunctionPlotItem`):**
  - Parser descendente recursivo en C++ puro sin dependencias externas.
  - Soporta operadores (`+`, `-`, `*`, `/`, `^`), funciones trigonométricas, exponenciales, logarítmicas, hiperbólicas, constantes (`pi`, `e`) y multiplicación implícita (`2x`, `3sin(x)`).
  - Detección inteligente de discontinuidades y asíntotas verticales (ej. $\tan(x)$, $1/x$).
  - Inserción vectorial en el lienzo lista para trazar encima derivadas, límites, tangentes o áreas bajo la curva.
- **Bloques de Imágenes (`ImageBlockItem`):**
  - Inserción de diagramas e imágenes desde archivo o pegado directo desde el portapapeles (`Ctrl+V`).

### 4. Sincronización en la Nube con Google Drive
- **Integración OAuth 2.0 PKCE:** Sincronización nativa y segura con Google Drive.
- **Estructura Organizada en la Nube:**
  - Respaldo continuo de la base de datos completa SQLite (`omninote_stem.omninote`).
  - Exportación automática **página por página** en carpetas estructuradas `PDFs/[Materia]/[Cuaderno]/Página X.pdf`.
  - Exportación completa en PDF vectorial de 300 DPI (`OmniNote_Apuntes.pdf`).
  - Historial de copias de seguridad fechadas (`historial_backups/`).

### 5. Iconografía Oficial Phosphor Duotone
- Set de iconos de la librería oficial **Phosphor Icons (Duotone)** con capas translúcidas de opacidad al 20% y trazos nítidos con paleta semántica Tokyo Night.
- Renderizado vectorial escalable sin pérdida de nitidez en pantallas de cualquier densidad.

### 6. Persistencia Transaccional SQLite (.omninote)
- Base de datos relacional SQLite3 con modo WAL y transacciones ACID inmediatas.
- Tablas jerárquicas: `Subjects` (Materias) $\to$ `Notebooks` (Cuadernos) $\to$ `Pages` (Páginas) $\to$ `Strokes` (Trazos comprimidos en BLOB) y `StemBlocks` (Bloques de código, fórmulas y gráficas en JSON).
- Historial completo de Deshacer / Rehacer basado en el patrón **Command** (`QUndoStack`).

---

## ⌨️ Atajos de Teclado

| Tecla | Acción |
| :--- | :--- |
| `P` | Activar herramienta Lápiz |
| `H` | Activar Resaltador |
| `E` | Activar Borrador |
| `L` | Activar Lazo de Selección |
| `S` | Activar Herramienta de Formas / Ejes |
| `T` | Insertar Bloque de Texto |
| `C` | Insertar Bloque de Código interactivo |
| `F` | Abrir diálogo de Graficar Función Matemática |
| `M` | Insertar Fórmula Matemática LaTeX |
| `R` | Mostrar / Ocultar Regla Virtual STEM |
| `I` | Insertar Imagen desde archivo |
| `Ctrl + V` | Pegar imagen o contenido del portapapeles |
| `Ctrl + S` | Guardar página actual en la base de datos |
| `Ctrl + Z` | Deshacer trazo o acción |
| `Ctrl + Y` | Rehacer trazo o acción |
| `Ctrl + D` | Duplicar elementos seleccionados con el Lazo |
| `Delete` | Eliminar elementos seleccionados con el Lazo |
| `F4` | Ocultar / Mostrar panel lateral izquierdo |
| `F1` | Guía de atajos de teclado y herramientas STEM |
| `Ctrl + Rueda` | Zoom suave centrado en el cursor |
| `Clic Central / Derecho + Arrastrar` | Desplazamiento por el lienzo (*Panning*) |
| `Espacio + Clic Izquierdo + Arrastrar` | Desplazamiento alternativo por el lienzo |

---

## 🛠️ Compilación e Instalación

### Requisitos del Sistema

#### Arch Linux / Manjaro / CachyOS:
```bash
sudo pacman -S base-devel cmake gcc qt6-base qt6-declarative qt6-svg poppler-qt6 sqlite
```

#### Ubuntu 24.04+ / Debian 12+:
```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev libqt6svg6-dev libpoppler-qt6-dev libsqlite3-dev
```

#### Fedora 39+:
```bash
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel poppler-qt6-devel sqlite-devel
```

### Proceso de Compilación
```bash
git clone https://github.com/FakeCxbas/OmniNote.git
cd OmniNote
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Ejecutar Pruebas Automatizadas
```bash
./build/test_omninote
```

### Ejecutar la Aplicación
```bash
./build/OmniNoteSTEM
```

---

## 📐 Estructura del Proyecto

```text
OmniNote/
├── assets/
│   └── icons/
│       └── phosphor_duotone/   # Iconos vectoriales oficiales Phosphor Duotone
├── src/
│   ├── main.cpp                # Punto de entrada de la aplicación
│   ├── app/                    # Ventana principal, temas y gestor de iconos
│   ├── input/                  # Soporte Wacom QTabletEvent y Catmull-Rom smoother
│   ├── canvas/                 # Lienzo QOpenGLWidget, trazos, lazo y regla virtual
│   ├── stem/                   # Bloques de código, parser matemático, gráficas y LaTeX
│   ├── pdf/                    # Importación/exportación vectorial Poppler y QPdfWriter
│   ├── storage/                # Persistencia transaccional SQLite3
│   └── sync/                   # Sincronización Google Drive OAuth 2.0 PKCE
├── tests/
│   └── test_omninote.cpp       # Suite completa de 18 pruebas unitarias
├── CMakeLists.txt              # Configuración CMake del proyecto
├── LICENSE                     # Licencia MIT
└── README.md                   # Documentación principal
```

---

## 📄 Licencia

Este proyecto está bajo la Licencia **MIT**. Consulta el archivo [LICENSE](LICENSE) para más detalles.
