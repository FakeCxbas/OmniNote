# Directrices de Flujo de Trabajo Git y Buenas Prácticas de Ingeniería

Estas reglas aplican para TODAS las operaciones de Git en este proyecto. Tienen máxima prioridad para elevar las métricas orgánicas y profesionales del perfil de GitHub del usuario.

## 1. REGLA ESTRICTA: PROHIBIDO PUSH DIRECTO A MAIN / MASTER
- NUNCA ejecutes `git push origin main` ni `git push origin master`.
- TODO cambio, por pequeño que sea, DEBE realizarse en una rama dedicada:
  1. Crear y cambiar a la rama: `git checkout -b feat/nombre-cambio` o `fix/nombre-bug`.
  2. Confirmar los cambios con Conventional Commits.
  3. Subir la rama y ABRIR UN PULL REQUEST INMEDIATAMENTE mediante GitHub CLI:
     `git push -u origin <nombre-rama>`
     `gh pr create --title "..." --body "..."`
  4. Una vez creado el PR, proceder a la fusión según se requiera (`gh pr merge --merge` o `--squash --delete-branch`).
- Esto es OBLIGATORIO para acumular historial real de Pull Requests y métricas profesionales en GitHub.

## 2. Convención de Commits Estructurados (Conventional Commits)
- Utilizar siempre el estándar de Conventional Commits:
  - `feat(...)`: Nuevas funcionalidades, componentes o endpoints.
  - `fix(...)`: Corrección de errores o bugs.
  - `refactor(...)`: Reestructuración de código sin alterar comportamiento.
  - `docs(...)`: Documentación y comentarios técnicos.
  - `test(...)`: Pruebas unitarias o de integración.
  - `perf(...)`: Mejoras de rendimiento y optimización.
- PROHIBIDOS los mensajes vagos como "Update code", "Fix bugs", "Cambios varios" o textos genéricos de IA.
- Redacción técnica, clara y en minúsculas en modo imperativo.

## 3. Trazabilidad con GitHub Issues
- Para cualquier tarea planificada, refactor o reporte de bug, registra o sugiere un Issue usando `gh issue create`.
- Enlazar siempre el issue en el commit y en el PR: `fix(engine): corregir escalado en viewport (closes #3)`.
- Esto demuestra organización y metodología ágil en el perfil.

## 4. Tono Humano, Sobrio y Cero Emojis de IA
- NUNCA incluir emojis genéricos de IA (🚀, 💻, ⚡, 🛠️, 📊, 📍, 💼, 💡, etc.) en READMEs, commits, PRs ni descripciones.
- Priorizar arquitectura limpia, pruebas reproducibles y explicaciones técnicas concretas.
