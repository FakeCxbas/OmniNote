#include "AppIcons.hpp"
#include <QSvgRenderer>
#include <QPainter>
#include <QHash>
#include <QFile>
#include <QCoreApplication>

namespace OmniNote {

// Official Phosphor Duotone Library Icons (256x256 viewBox with 20% opacity depth layers)
QString AppIcons::getSvgXml(IconType type, const QString &colorHex) {
    QString pathData;

    switch (type) {
    case IconType::Panel:
        pathData = R"(<path d="M88,48V208H40a8,8,0,0,1-8-8V56a8,8,0,0,1,8-8Z" opacity="0.2"/><path d="M216,40H40A16,16,0,0,0,24,56V200a16,16,0,0,0,16,16H216a16,16,0,0,0,16-16V56A16,16,0,0,0,216,40ZM40,56H80V200H40ZM216,200H96V56H216V200Z"/>)";
        break;
    case IconType::Undo:
        pathData = R"(<path d="M80,40v96L32,88Z" opacity="0.2"/><path d="M168,80H88V40a8,8,0,0,0-13.66-5.66l-48,48a8,8,0,0,0,0,11.32l48,48A8,8,0,0,0,88,136V96h80a48,48,0,0,1,0,96H80a8,8,0,0,0,0,16h88a64,64,0,0,0,0-128ZM72,116.69,43.31,88,72,59.31Z"/>)";
        break;
    case IconType::Redo:
        pathData = R"(<path d="M224,88l-48,48V40Z" opacity="0.2"/><path d="M172.94,143.39a8,8,0,0,0,8.72-1.73l48-48a8,8,0,0,0,0-11.32l-48-48A8,8,0,0,0,168,40V80H88a64,64,0,0,0,0,128h88a8,8,0,0,0,0-16H88a48,48,0,0,1,0-96h80v40A8,8,0,0,0,172.94,143.39ZM184,59.31,212.69,88,184,116.69Z"/>)";
        break;
    case IconType::Pen:
        pathData = R"(<path d="M221.66,90.34,192,120,136,64l29.66-29.66a8,8,0,0,1,11.31,0L221.66,79A8,8,0,0,1,221.66,90.34Z" opacity="0.2"/><path d="M227.31,73.37,182.63,28.68a16,16,0,0,0-22.63,0L36.69,152A15.86,15.86,0,0,0,32,163.31V208a16,16,0,0,0,16,16H92.69A15.86,15.86,0,0,0,104,219.31L227.31,96a16,16,0,0,0,0-22.63ZM92.69,208H48V163.31l88-88L180.69,120ZM192,108.68,147.31,64l24-24L216,84.68Z"/>)";
        break;
    case IconType::Highlighter:
        pathData = R"(<path d="M195.88,195.88A95.7,95.7,0,0,1,168,215.29V152a8,8,0,0,0-8-8h-8V72L104,96v48H96a8,8,0,0,0-8,8v63.29a96,96,0,1,1,107.88-19.41Z" opacity="0.2"/><path d="M201.54,54.46A104,104,0,0,0,54.46,201.54,104,104,0,0,0,201.54,54.46ZM96,210V152h64v58a88.33,88.33,0,0,1-64,0Zm48-74H112V100.94l32-16Zm46.22,54.22A88.09,88.09,0,0,1,176,201.77V152a16,16,0,0,0-16-16V72a8,8,0,0,0-11.58-7.16l-48,24A8,8,0,0,0,96,96v40a16,16,0,0,0-16,16v49.77a88,88,0,1,1,110.22-11.55Z"/>)";
        break;
    case IconType::Eraser:
        pathData = R"(<path d="M160,168l-48,48H66.75L36.69,185.94a16,16,0,0,1,0-22.63L96,104Z" opacity="0.2"/><path d="M225,80.4,183.6,39a24,24,0,0,0-33.94,0L31,157.66a24,24,0,0,0,0,33.94l30.06,30.06A8,8,0,0,0,66.74,224H216a8,8,0,0,0,0-16h-84.7L225,114.34A24,24,0,0,0,225,80.4ZM108.68,208H70.05L42.33,180.28a8,8,0,0,1,0-11.31L96,115.31,148.69,168Zm105-105L160,156.69,107.31,104,161,50.34a8,8,0,0,1,11.32,0l41.38,41.38a8,8,0,0,1,0,11.31Z"/>)";
        break;
    case IconType::Lasso:
        pathData = R"(<path d="M232,112c0,35.35-46.56,64-104,64S24,147.35,24,112,70.56,48,128,48,232,76.65,232,112Z" opacity="0.2"/><path d="M205.73,59.93C184.85,47.08,157.24,40,128,40S71.15,47.08,50.27,59.93C28.17,73.52,16,92,16,112S28.17,150.44,50.27,164c19,11.67,43.49,18.56,69.73,19.73v0a37.35,37.35,0,0,1-18.58,33c-14.64,8.86-34.62,9.52-49.72,1.64a8,8,0,1,0-7.4,14.18A66.4,66.4,0,0,0,75,240a67.31,67.31,0,0,0,34.74-9.5c17-10.27,26.29-26.86,26.29-46.7v0c26.24-1.17,50.76-8.06,69.73-19.73C227.83,150.44,240,132,240,112S227.83,73.52,205.73,59.93ZM67.41,155.18c5.24-9.55,15.45-12,23.53-11,10.9,1.42,21.86,9.13,26.61,23.42C99.11,166.45,81.85,162.16,67.41,155.18Zm129.94-4.77c-16.95,10.43-39.17,16.53-63.13,17.43a54.37,54.37,0,0,0-11.39-23.07A47.17,47.17,0,0,0,93,128.35c-17-2.2-31.72,5.11-39.38,18.7C39.64,137,32,124.73,32,112c0-14.21,9.47-27.86,26.65-38.43C77.05,62.23,101.68,56,128,56S179,62.23,197.35,73.55C214.53,84.12,224,97.77,224,112S214.53,139.84,197.35,150.41Z"/>)";
        break;
    case IconType::Shapes:
        pathData = R"(<path d="M64,64l40,120H24ZM200,76a44,44,0,1,0-44,44A44,44,0,0,0,200,76Zm-64,76v56h88V152Z" opacity="0.2"/><path d="M224,144H136a8,8,0,0,0-8,8v56a8,8,0,0,0,8,8h88a8,8,0,0,0,8-8V152A8,8,0,0,0,224,144Zm-8,56H144V160h72ZM71.59,61.47a8,8,0,0,0-15.18,0l-40,120A8,8,0,0,0,24,192h80a8,8,0,0,0,7.59-10.53ZM35.1,176,64,89.3,92.9,176ZM208,76a52,52,0,1,0-52,52A52.06,52.06,0,0,0,208,76Zm-88,0a36,36,0,1,1,36,36A36,36,0,0,1,120,76Z"/>)";
        break;
    case IconType::StraightLine:
        pathData = R"(<path d="M81,175A24,24,0,1,1,47,175,24,24,0,0,1,81,175ZM209,47A24,24,0,1,0,209,81,24,24,0,0,0,209,47Z" opacity="0.2"/><path d="M214.64,41.36a32,32,0,0,0-50.2,38.89L80.25,164.44a32.06,32.06,0,0,0-38.89,4.94h0a32,32,0,1,0,50.2,6.37l84.19-84.19a32,32,0,0,0,38.89-50.2Zm-139.33,162a16,16,0,0,1-22.64-22.64h0a16,16,0,0,1,22.63,0h0A16,16,0,0,1,75.31,203.33Zm128-128a16,16,0,1,1,0-22.63A16,16,0,0,1,203.33,75.3Z"/>)";
        break;
    case IconType::Axes2D:
        pathData = R"(<path d="M224,64V208H32V48H208A16,16,0,0,1,224,64Z" opacity="0.2"/><path d="M232,208a8,8,0,0,1-8,8H32a8,8,0,0,1-8-8V48a8,8,0,0,1,16,0V156.69l50.34-50.35a8,8,0,0,1,11.32,0L128,132.69,180.69,80H160a8,8,0,0,1,0-16h40a8,8,0,0,1,8,8v40a8,8,0,0,1-16,0V91.31l-58.34,58.35a8,8,0,0,1-11.32,0L96,123.31l-56,56V200H224A8,8,0,0,1,232,208Z"/>)";
        break;
    case IconType::Axes3D:
        pathData = R"(<path d="M128,129.09V232a8,8,0,0,1-3.84-1l-88-48.16a8,8,0,0,1-4.16-7V80.2a8,8,0,0,1,.7-3.27Z" opacity="0.2"/><path d="M223.68,66.15,135.68,18h0a15.88,15.88,0,0,0-15.36,0l-88,48.17a16,16,0,0,0-8.32,14v95.64a16,16,0,0,0,8.32,14l88,48.17a15.88,15.88,0,0,0,15.36,0l88-48.17a16,16,0,0,0,8.32-14V80.18A16,16,0,0,0,223.68,66.15ZM128,32h0l80.34,44L128,120,47.66,76ZM40,90l80,43.78v85.79L40,175.82Zm96,129.57V133.82L216,90v85.78Z"/>)";
        break;
    case IconType::RectShape:
        pathData = R"(<path d="M216,48V208a8,8,0,0,1-8,8H48a8,8,0,0,1-8-8V48a8,8,0,0,1,8-8H208A8,8,0,0,1,216,48Z" opacity="0.2"/><path d="M208,32H48A16,16,0,0,0,32,48V208a16,16,0,0,0,16,16H208a16,16,0,0,0,16-16V48A16,16,0,0,0,208,32Zm0,176H48V48H208V208Z"/>)";
        break;
    case IconType::CircleShape:
        pathData = R"(<path d="M224,128a96,96,0,1,1-96-96A96,96,0,0,1,224,128Z" opacity="0.2"/><path d="M128,24A104,104,0,1,0,232,128,104.11,104.11,0,0,0,128,24Zm0,192a88,88,0,1,1,88-88A88.1,88.1,0,0,1,128,216Z"/>)";
        break;
    case IconType::Ruler:
        pathData = R"(<path d="M229.66,90.34,90.34,229.66a8,8,0,0,1-11.31,0L26.34,177a8,8,0,0,1,0-11.31L165.66,26.34a8,8,0,0,1,11.31,0L229.66,79A8,8,0,0,1,229.66,90.34Z" opacity="0.2"/><path d="M235.32,73.37,182.63,20.69a16,16,0,0,0-22.63,0L20.68,160a16,16,0,0,0,0,22.63l52.69,52.68a16,16,0,0,0,22.63,0L235.32,96A16,16,0,0,0,235.32,73.37ZM84.68,224,32,171.31l32-32,26.34,26.35a8,8,0,0,0,11.32-11.32L75.31,128,96,107.31l26.34,26.35a8,8,0,0,0,11.32-11.32L107.31,96,128,75.31l26.34,26.35a8,8,0,0,0,11.32-11.32L139.31,64l32-32L224,84.69Z"/>)";
        break;
    case IconType::Sparkles:
        pathData = R"(<path d="M194.82,151.43l-55.09,20.3-20.3,55.09a7.92,7.92,0,0,1-14.86,0l-20.3-55.09-55.09-20.3a7.92,7.92,0,0,1,0-14.86l55.09-20.3,20.3-55.09a7.92,7.92,0,0,1,14.86,0l20.3,55.09,55.09,20.3A7.92,7.92,0,0,1,194.82,151.43Z" opacity="0.2"/><path d="M197.58,129.06,146,110l-19-51.62a15.92,15.92,0,0,0-29.88,0L78,110l-51.62,19a15.92,15.92,0,0,0,0,29.88L78,178l19,51.62a15.92,15.92,0,0,0,29.88,0L146,178l51.62-19a15.92,15.92,0,0,0,0-29.88ZM137,164.22a8,8,0,0,0-4.74,4.74L112,223.85,91.78,169A8,8,0,0,0,87,164.22L32.15,144,87,123.78A8,8,0,0,0,91.78,119L112,64.15,132.22,119a8,8,0,0,0,4.74,4.74L191.85,144ZM144,40a8,8,0,0,1,8-8h16V16a8,8,0,0,1,16,0V32h16a8,8,0,0,1,0,16H184V64a8,8,0,0,1-16,0V48H152A8,8,0,0,1,144,40ZM248,88a8,8,0,0,1-8,8h-8v8a8,8,0,0,1-16,0V96h-8a8,8,0,0,1,0-16h8V72a8,8,0,0,1,16,0v8h8A8,8,0,0,1,248,88Z"/>)";
        break;
    case IconType::Text:
        pathData = R"(<path d="M200,56V184a16,16,0,0,1-16,16H72a16,16,0,0,1-16-16V56Z" opacity="0.2"/><path d="M208,56V88a8,8,0,0,1-16,0V64H136V192h24a8,8,0,0,1,0,16H96a8,8,0,0,1,0-16h24V64H64V88a8,8,0,0,1-16,0V56a8,8,0,0,1,8-8H200A8,8,0,0,1,208,56Z"/>)";
        break;
    case IconType::Code:
        pathData = R"(<path d="M240,128l-48,40H64L16,128,64,88H192Z" opacity="0.2"/><path d="M69.12,94.15,28.5,128l40.62,33.85a8,8,0,1,1-10.24,12.29l-48-40a8,8,0,0,1,0-12.29l48-40a8,8,0,0,1,10.24,12.3Zm176,27.7-48-40a8,8,0,1,0-10.24,12.3L227.5,128l-40.62,33.85a8,8,0,1,0,10.24,12.29l48-40a8,8,0,0,0,0-12.29ZM162.73,32.48a8,8,0,0,0-10.25,4.79l-64,176a8,8,0,0,0,4.79,10.26A8.14,8.14,0,0,0,96,224a8,8,0,0,0,7.52-5.27l64-176A8,8,0,0,0,162.73,32.48Z"/>)";
        break;
    case IconType::Plot:
        pathData = R"(<path d="M224,64V208H32V48H208A16,16,0,0,1,224,64Z" opacity="0.2"/><path d="M232,208a8,8,0,0,1-8,8H32a8,8,0,0,1-8-8V48a8,8,0,0,1,16,0V156.69l50.34-50.35a8,8,0,0,1,11.32,0L128,132.69,180.69,80H160a8,8,0,0,1,0-16h40a8,8,0,0,1,8,8v40a8,8,0,0,1-16,0V91.31l-58.34,58.35a8,8,0,0,1-11.32,0L96,123.31l-56,56V200H224A8,8,0,0,1,232,208Z"/>)";
        break;
    case IconType::Image:
        pathData = R"(<path d="M224,56V178.06l-39.72-39.72a8,8,0,0,0-11.31,0L147.31,164,97.66,114.34a8,8,0,0,0-11.32,0L32,168.69V56a8,8,0,0,1,8-8H216A8,8,0,0,1,224,56Z" opacity="0.2"/><path d="M216,40H40A16,16,0,0,0,24,56V200a16,16,0,0,0,16,16H216a16,16,0,0,0,16-16V56A16,16,0,0,0,216,40Zm0,16V158.75l-26.07-26.06a16,16,0,0,0-22.63,0l-20,20-44-44a16,16,0,0,0-22.62,0L40,149.37V56ZM40,172l52-52,80,80H40Zm176,28H194.63l-36-36,20-20L216,181.38V200ZM144,100a12,12,0,1,1,12,12A12,12,0,0,1,144,100Z"/>)";
        break;
    case IconType::Formula:
        pathData = R"(<path d="M200,40V200a16,16,0,0,1-16,16H56V56A16,16,0,0,1,72,40Z" opacity="0.2"/><path d="M208,40a8,8,0,0,1-8,8H170.71a24,24,0,0,0-23.62,19.71L137.59,120H184a8,8,0,0,1,0,16H134.68l-10,55.16A40,40,0,0,1,85.29,224H56a8,8,0,0,1,0-16H85.29a24,24,0,0,0,23.62-19.71l9.5-52.29H72a8,8,0,0,1,0-16h49.32l10-55.16A40,40,0,0,1,170.71,32H200A8,8,0,0,1,208,40Z"/>)";
        break;
    case IconType::Palette:
        pathData = R"(<path d="M224,127.17a96.48,96.48,0,0,1-2.39,22.18A24,24,0,0,1,198.21,168H152a24,24,0,0,0-24,24,24,24,0,0,1-32,22.61C58.73,201.44,32,169.81,32,128a96,96,0,0,1,95-96C179.84,31.47,223.55,74.35,224,127.17Z" opacity="0.2"/><path d="M200.77,53.89A103.27,103.27,0,0,0,128,24h-1.07A104,104,0,0,0,24,128c0,43,26.58,79.06,69.36,94.17A32,32,0,0,0,136,192a16,16,0,0,1,16-16h46.21a31.81,31.81,0,0,0,31.2-24.88,104.43,104.43,0,0,0,2.59-24A103.28,103.28,0,0,0,200.77,53.89Zm13,93.71A15.89,15.89,0,0,1,198.21,160H152a32,32,0,0,0-32,32,16,16,0,0,1-21.31,15.07C62.49,194.3,40,164,40,128a88,88,0,0,1,87.09-88h.9a88.35,88.35,0,0,1,88,87.25A88.86,88.86,0,0,1,213.81,147.6ZM140,76a12,12,0,1,1-12-12A12,12,0,0,1,140,76ZM96,100A12,12,0,1,1,84,88,12,12,0,0,1,96,100Zm0,56a12,12,0,1,1-12-12A12,12,0,0,1,96,156Zm88-56a12,12,0,1,1-12-12A12,12,0,0,1,184,100Z"/>)";
        break;
    case IconType::ImportPdf:
        pathData = R"(<path d="M208,88H152V32Z" opacity="0.2"/><path d="M213.66,82.34l-56-56A8,8,0,0,0,152,24H56A16,16,0,0,0,40,40V216a16,16,0,0,0,16,16H200a16,16,0,0,0,16-16V88A8,8,0,0,0,213.66,82.34ZM160,51.31,188.69,80H160ZM200,216H56V40h88V88a8,8,0,0,0,8,8h48V216Zm-42.34-77.66a8,8,0,0,1-11.32,11.32L136,139.31V184a8,8,0,0,1-16,0V139.31l-10.34,10.35a8,8,0,0,1-11.32-11.32l24-24a8,8,0,0,1,11.32,0Z"/>)";
        break;
    case IconType::ExportPdf:
        pathData = R"(<path d="M208,88H152V32Z" opacity="0.2"/><path d="M213.66,82.34l-56-56A8,8,0,0,0,152,24H56A16,16,0,0,0,40,40V216a16,16,0,0,0,16,16H200a16,16,0,0,0,16-16V88A8,8,0,0,0,213.66,82.34ZM160,51.31,188.69,80H160ZM200,216H56V40h88V88a8,8,0,0,0,8,8h48V216Zm-42.34-61.66a8,8,0,0,1,0,11.32l-24,24a8,8,0,0,1-11.32,0l-24-24a8,8,0,0,1,11.32-11.32L120,164.69V120a8,8,0,0,1,16,0v44.69l10.34-10.35A8,8,0,0,1,157.66,154.34Z"/>)";
        break;
    case IconType::Save:
        pathData = R"(<path d="M216,83.31V208a8,8,0,0,1-8,8H176V152a8,8,0,0,0-8-8H88a8,8,0,0,0-8,8v64H48a8,8,0,0,1-8-8V48a8,8,0,0,1,8-8H172.69a8,8,0,0,1,5.65,2.34l35.32,35.32A8,8,0,0,1,216,83.31Z" opacity="0.2"/><path d="M219.31,72,184,36.69A15.86,15.86,0,0,0,172.69,32H48A16,16,0,0,0,32,48V208a16,16,0,0,0,16,16H208a16,16,0,0,0,16-16V83.31A15.86,15.86,0,0,0,219.31,72ZM168,208H88V152h80Zm40,0H184V152a16,16,0,0,0-16-16H88a16,16,0,0,0-16,16v56H48V48H172.69L208,83.31ZM160,72a8,8,0,0,1-8,8H96a8,8,0,0,1,0-16h56A8,8,0,0,1,160,72Z"/>)";
        break;
    case IconType::Duplicate:
        pathData = R"(<path d="M216,40V168H168V88H88V40Z" opacity="0.2"/><path d="M216,32H88a8,8,0,0,0-8,8V80H40a8,8,0,0,0-8,8V216a8,8,0,0,0,8,8H168a8,8,0,0,0,8-8V176h40a8,8,0,0,0,8-8V40A8,8,0,0,0,216,32ZM160,208H48V96H160Zm48-48H176V88a8,8,0,0,0-8-8H96V48H208Z"/>)";
        break;
    case IconType::Copy:
        pathData = R"(<path d="M216,40V168H168V88H88V40Z" opacity="0.2"/><path d="M216,32H88a8,8,0,0,0-8,8V80H40a8,8,0,0,0-8,8V216a8,8,0,0,0,8,8H168a8,8,0,0,0,8-8V176h40a8,8,0,0,0,8-8V40A8,8,0,0,0,216,32ZM160,208H48V96H160Zm48-48H176V88a8,8,0,0,0-8-8H96V48H208Z"/>)";
        break;
    case IconType::Trash:
        pathData = R"(<path d="M200,56V208a8,8,0,0,1-8,8H64a8,8,0,0,1-8-8V56Z" opacity="0.2"/><path d="M216,48H176V40a24,24,0,0,0-24-24H104A24,24,0,0,0,80,40v8H40a8,8,0,0,0,0,16h8V208a16,16,0,0,0,16,16H192a16,16,0,0,0,16-16V64h8a8,8,0,0,0,0-16ZM96,40a8,8,0,0,1,8-8h48a8,8,0,0,1,8,8v8H96Zm96,168H64V64H192ZM112,104v64a8,8,0,0,1-16,0V104a8,8,0,0,1,16,0Zm48,0v64a8,8,0,0,1-16,0V104a8,8,0,0,1,16,0Z"/>)";
        break;
    case IconType::Close:
        pathData = R"(<path d="M216,56V200a16,16,0,0,1-16,16H56a16,16,0,0,1-16-16V56A16,16,0,0,1,56,40H200A16,16,0,0,1,216,56Z" opacity="0.2"/><path d="M205.66,194.34a8,8,0,0,1-11.32,11.32L128,139.31,61.66,205.66a8,8,0,0,1-11.32-11.32L116.69,128,50.34,61.66A8,8,0,0,1,61.66,50.34L128,116.69l66.34-66.35a8,8,0,0,1,11.32,11.32L139.31,128Z"/>)";
        break;
    case IconType::Check:
        pathData = R"(<path d="M232,56V200a16,16,0,0,1-16,16H40a16,16,0,0,1-16-16V56A16,16,0,0,1,40,40H216A16,16,0,0,1,232,56Z" opacity="0.2"/><path d="M205.66,85.66l-96,96a8,8,0,0,1-11.32,0l-40-40a8,8,0,0,1,11.32-11.32L104,164.69l90.34-90.35a8,8,0,0,1,11.32,11.32Z"/>)";
        break;
    case IconType::Folder:
        pathData = R"(<path d="M128,80H32V56a8,8,0,0,1,8-8H92.69a8,8,0,0,1,5.65,2.34Z" opacity="0.2"/><path d="M216,72H131.31L104,44.69A15.86,15.86,0,0,0,92.69,40H40A16,16,0,0,0,24,56V200.62A15.4,15.4,0,0,0,39.38,216H216.89A15.13,15.13,0,0,0,232,200.89V88A16,16,0,0,0,216,72ZM92.69,56l16,16H40V56ZM216,200H40V88H216Z"/>)";
        break;
    case IconType::Notebook:
        pathData = R"(<path d="M80,40V216H48a8,8,0,0,1-8-8V48a8,8,0,0,1,8-8Z" opacity="0.2"/><path d="M184,112a8,8,0,0,1-8,8H112a8,8,0,0,1,0-16h64A8,8,0,0,1,184,112Zm-8,24H112a8,8,0,0,0,0,16h64a8,8,0,0,0,0-16Zm48-88V208a16,16,0,0,1-16,16H48a16,16,0,0,1-16-16V48A16,16,0,0,1,48,32H208A16,16,0,0,1,224,48ZM48,208H72V48H48Zm160,0V48H88V208H208Z"/>)";
        break;
    case IconType::Page:
        pathData = R"(<path d="M208,88H152V32Z" opacity="0.2"/><path d="M213.66,82.34l-56-56A8,8,0,0,0,152,24H56A16,16,0,0,0,40,40V216a16,16,0,0,0,16,16H200a16,16,0,0,0,16-16V88A8,8,0,0,0,213.66,82.34ZM160,51.31,188.69,80H160ZM200,216H56V40h88V88a8,8,0,0,0,8,8h48V216Zm-32-80a8,8,0,0,1-8,8H96a8,8,0,0,1,0-16h64A8,8,0,0,1,168,136Zm0,32a8,8,0,0,1-8,8H96a8,8,0,0,1,0-16h64A8,8,0,0,1,168,168Z"/>)";
        break;
    case IconType::Plus:
        pathData = R"(<path d="M216,56V200a16,16,0,0,1-16,16H56a16,16,0,0,1-16-16V56A16,16,0,0,1,56,40H200A16,16,0,0,1,216,56Z" opacity="0.2"/><path d="M224,128a8,8,0,0,1-8,8H136v80a8,8,0,0,1-16,0V136H40a8,8,0,0,1,0-16h80V40a8,8,0,0,1,16,0v80h80A8,8,0,0,1,224,128Z"/>)";
        break;
    case IconType::Restore:
        pathData = R"(<path d="M216,128a88,88,0,1,1-88-88A88,88,0,0,1,216,128Z" opacity="0.2"/><path d="M224,128a96,96,0,0,1-94.71,96H128A95.38,95.38,0,0,1,62.1,197.8a8,8,0,0,1,11-11.63A80,80,0,1,0,71.43,71.39a3.07,3.07,0,0,1-.26.25L44.59,96H72a8,8,0,0,1,0,16H24a8,8,0,0,1-8-8V56a8,8,0,0,1,16,0V85.8L60.25,60A96,96,0,0,1,224,128Z"/>)";
        break;
    case IconType::Edit:
        pathData = R"(<path d="M221.66,90.34,192,120,136,64l29.66-29.66a8,8,0,0,1,11.31,0L221.66,79A8,8,0,0,1,221.66,90.34Z" opacity="0.2"/><path d="M227.31,73.37,182.63,28.68a16,16,0,0,0-22.63,0L36.69,152A15.86,15.86,0,0,0,32,163.31V208a16,16,0,0,0,16,16H92.69A15.86,15.86,0,0,0,104,219.31L227.31,96a16,16,0,0,0,0-22.63ZM92.69,208H48V163.31l88-88L180.69,120ZM192,108.68,147.31,64l24-24L216,84.68Z"/>)";
        break;
    case IconType::Cloud:
        pathData = R"(<path d="M240,128a80,80,0,0,1-80,80H72A56,56,0,1,1,85.92,97.74l0,.1A80,80,0,0,1,240,128Z" opacity="0.2"/><path d="M178.34,165.66,160,147.31V208a8,8,0,0,1-16,0V147.31l-18.34,18.35a8,8,0,0,1-11.32-11.32l32-32a8,8,0,0,1,11.32,0l32,32a8,8,0,0,1-11.32,11.32ZM160,40A88.08,88.08,0,0,0,81.29,88.68,64,64,0,1,0,72,216h40a8,8,0,0,0,0-16H72a48,48,0,0,1,0-96c1.1,0,2.2,0,3.29.12A88,88,0,0,0,72,128a8,8,0,0,0,16,0,72,72,0,1,1,100.8,66,8,8,0,0,0,3.2,15.34,7.9,7.9,0,0,0,3.2-.68A88,88,0,0,0,160,40Z"/>)";
        break;
    case IconType::Help:
        pathData = R"(<path d="M224,128a96,96,0,1,1-96-96A96,96,0,0,1,224,128Z" opacity="0.2"/><path d="M140,180a12,12,0,1,1-12-12A12,12,0,0,1,140,180ZM128,72c-22.06,0-40,16.15-40,36v4a8,8,0,0,0,16,0v-4c0-11,10.77-20,24-20s24,9,24,20-10.77,20-24,20a8,8,0,0,0-8,8v8a8,8,0,0,0,16,0v-.72c18.24-3.35,32-17.9,32-35.28C168,88.15,150.06,72,128,72Zm104,56A104,104,0,1,1,128,24,104.11,104.11,0,0,1,232,128Zm-16,0a88,88,0,1,0-88,88A88.1,88.1,0,0,0,216,128Z"/>)";
        break;
    }

    return QString(
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256" fill="%1">%2</svg>)"
    ).arg(colorHex, pathData);
}

QString AppIcons::getIconFilename(IconType type) {
    switch (type) {
    case IconType::Panel: return "panel.png";
    case IconType::Undo: return "undo.png";
    case IconType::Redo: return "redo.png";
    case IconType::Pen: return "pen.png";
    case IconType::Highlighter: return "highlighter.png";
    case IconType::Eraser: return "eraser.png";
    case IconType::Lasso: return "lasso.png";
    case IconType::Shapes: return "shapes.png";
    case IconType::StraightLine: return "straight_line.png";
    case IconType::Axes2D: return "axes_2d.png";
    case IconType::Axes3D: return "axes_3d.png";
    case IconType::RectShape: return "rect_shape.png";
    case IconType::CircleShape: return "circle_shape.png";
    case IconType::Ruler: return "ruler.png";
    case IconType::Sparkles: return "sparkles.png";
    case IconType::Text: return "text.png";
    case IconType::Code: return "code.png";
    case IconType::Plot: return "plot.png";
    case IconType::Image: return "image.png";
    case IconType::Formula: return "formula.png";
    case IconType::Palette: return "palette.png";
    case IconType::ImportPdf: return "import_pdf.png";
    case IconType::ExportPdf: return "export_pdf.png";
    case IconType::Save: return "save.png";
    case IconType::Duplicate: return "duplicate.png";
    case IconType::Trash: return "trash.png";
    case IconType::Close: return "close.png";
    case IconType::Check: return "check.png";
    case IconType::Folder: return "folder.png";
    case IconType::Notebook: return "notebook.png";
    case IconType::Page: return "page.png";
    case IconType::Plus: return "plus.png";
    case IconType::Restore: return "restore.png";
    case IconType::Edit: return "edit.png";
    case IconType::Copy: return "copy.png";
    case IconType::Cloud: return "cloud.png";
    case IconType::Help: return "help.png";
    }
    return QString();
}

QPixmap AppIcons::getPixmap(IconType type, const QColor &color, int size) {
    static QHash<quint64, QPixmap> pixmapCache;
    quint64 cacheKey = (static_cast<quint64>(type) << 32) | static_cast<quint32>(size);
    if (pixmapCache.contains(cacheKey)) {
        return pixmapCache.value(cacheKey);
    }

    QString filename = getIconFilename(type);
    QPixmap basePixmap;

    // 1. Try Qt Resource System
    if (!filename.isEmpty()) {
        QString qrcPath = QString(":/icons/3d/%1").arg(filename);
        if (QFile::exists(qrcPath)) {
            basePixmap.load(qrcPath);
        }
    }

    // 2. Try filesystem paths
    if (basePixmap.isNull() && !filename.isEmpty()) {
        const QStringList candidatePaths = {
            QString("assets/icons/3d/%1").arg(filename),
            QString("../assets/icons/3d/%1").arg(filename),
            QCoreApplication::applicationDirPath() + QString("/../assets/icons/3d/%1").arg(filename),
            QCoreApplication::applicationDirPath() + QString("/assets/icons/3d/%1").arg(filename)
        };
        for (const auto &p : candidatePaths) {
            if (QFile::exists(p)) {
                basePixmap.load(p);
                if (!basePixmap.isNull()) break;
            }
        }
    }

    // 3. Fallback to vector SVG if PNG not found
    if (basePixmap.isNull()) {
        QString svg = getSvgXml(type, color.name());
        QSvgRenderer renderer(svg.toUtf8());

        const int scale = 2; // High-DPI support
        QPixmap pixmap(size * scale, size * scale);
        pixmap.setDevicePixelRatio(scale);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        renderer.render(&painter, QRectF(0.0, 0.0, size, size));
        painter.end();
        pixmapCache.insert(cacheKey, pixmap);
        return pixmap;
    }

    const int scale = 2;
    int targetPx = size * scale;
    QPixmap scaled = basePixmap.scaled(targetPx, targetPx, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    scaled.setDevicePixelRatio(scale);
    pixmapCache.insert(cacheKey, scaled);
    return scaled;
}

QIcon AppIcons::get(IconType type, const QColor &color, int size) {
    QIcon icon;
    QPixmap normalPx = getPixmap(type, color, size);
    icon.addPixmap(normalPx, QIcon::Normal, QIcon::Off);
    icon.addPixmap(normalPx, QIcon::Active, QIcon::Off);
    icon.addPixmap(normalPx, QIcon::Normal, QIcon::On);
    icon.addPixmap(normalPx, QIcon::Active, QIcon::On);

    QPixmap dim(normalPx.size());
    dim.fill(Qt::transparent);
    QPainter p(&dim);
    p.setOpacity(0.35);
    p.drawPixmap(0, 0, normalPx);
    p.end();
    dim.setDevicePixelRatio(normalPx.devicePixelRatio());
    icon.addPixmap(dim, QIcon::Disabled, QIcon::Off);

    return icon;
}

} // namespace OmniNote
