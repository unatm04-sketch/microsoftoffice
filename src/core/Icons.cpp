#include "core/Icons.h"

#include "core/ThemeManager.h"

#include <QGuiApplication>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace {

struct IconDef
{
    const char *name;
    const char *svg;
};

// 24x24 line icons. The wrapper supplies stroke="currentColor", round caps and no fill.
const IconDef kIcons[] = {
    // --- files & app ----------------------------------------------------------------
    {"file", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/>)svg"},
    {"file-text", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M9 13h6M9 17h6M9 9h2"/>)svg"},
    {"new", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M12 11v6M9 14h6"/>)svg"},
    {"open", R"svg(<path d="M3 19V6a1 1 0 0 1 1-1h5l2 2h8a1 1 0 0 1 1 1v3"/><path d="M3 19l3.2-7.4A1 1 0 0 1 7.1 11H21a.6.6 0 0 1 .55.83L18.6 18.4a1 1 0 0 1-.92.6H3z"/>)svg"},
    {"folder", R"svg(<path d="M3 6.5A1.5 1.5 0 0 1 4.5 5H9l2 2.5h8.5A1.5 1.5 0 0 1 21 9v9.5a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5z"/>)svg"},
    {"folder-plus", R"svg(<path d="M3 6.5A1.5 1.5 0 0 1 4.5 5H9l2 2.5h8.5A1.5 1.5 0 0 1 21 9v9.5a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5z"/><path d="M12 11v6M9 14h6"/>)svg"},
    {"save", R"svg(<path d="M5 4h11l3 3v12a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1z"/><path d="M8 4v4h7V4"/><path d="M7 20v-6h10v6"/>)svg"},
    {"save-as", R"svg(<path d="M12 20H5a1 1 0 0 1-1-1V5a1 1 0 0 1 1-1h11l3 3v4"/><path d="M8 4v4h7V4"/><path d="M7 20v-6h5"/><path d="M19.5 13.5l2 2L16 21h-2v-2z"/>)svg"},
    {"print", R"svg(<path d="M7 9V3h10v6"/><rect x="3" y="9" width="18" height="8" rx="2"/><path d="M7 14h10v7H7z"/>)svg"},
    {"pdf", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><text x="12" y="17.6" font-family="Segoe UI" font-size="6.4" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">PDF</text>)svg"},
    {"export", R"svg(<path d="M12 15V3"/><path d="M8 7l4-4 4 4"/><path d="M5 13v6a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-6"/>)svg"},
    {"import", R"svg(<path d="M12 3v12"/><path d="M8 11l4 4 4-4"/><path d="M5 17v2a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-2"/>)svg"},
    {"close", R"svg(<path d="M6 6l12 12M18 6L6 18"/>)svg"},
    {"home", R"svg(<path d="M3 11l9-7 9 7"/><path d="M5 10v10h5v-6h4v6h5V10"/>)svg"},
    {"settings", R"svg(<path d="M4 6h9M17 6h3M4 12h3M11 12h9M4 18h11M19 18h1"/><circle cx="15" cy="6" r="2"/><circle cx="9" cy="12" r="2"/><circle cx="17" cy="18" r="2"/>)svg"},
    {"sun", R"svg(<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M2 12h2M20 12h2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>)svg"},
    {"moon", R"svg(<path d="M20 14.5A8 8 0 0 1 9.5 4a8 8 0 1 0 10.5 10.5z"/>)svg"},
    {"search", R"svg(<circle cx="11" cy="11" r="6.5"/><path d="M20.5 20.5L16 16"/>)svg"},
    {"replace", R"svg(<path d="M17 3l3 3-3 3"/><path d="M4 11V9a3 3 0 0 1 3-3h13"/><path d="M7 21l-3-3 3-3"/><path d="M20 13v2a3 3 0 0 1-3 3H4"/>)svg"},
    {"zoom-in", R"svg(<circle cx="11" cy="11" r="6.5"/><path d="M20.5 20.5L16 16M11 8v6M8 11h6"/>)svg"},
    {"zoom-out", R"svg(<circle cx="11" cy="11" r="6.5"/><path d="M20.5 20.5L16 16M8 11h6"/>)svg"},
    {"plus", R"svg(<path d="M12 5v14M5 12h14"/>)svg"},
    {"minus", R"svg(<path d="M5 12h14"/>)svg"},
    {"trash", R"svg(<path d="M4 7h16"/><path d="M9 7V4h6v3"/><path d="M6 7l1 13a1 1 0 0 0 1 1h8a1 1 0 0 0 1-1l1-13"/><path d="M10 11v6M14 11v6"/>)svg"},
    {"refresh", R"svg(<path d="M20 11a8 8 0 0 0-14.5-4.5L4 8"/><path d="M4 3v5h5"/><path d="M4 13a8 8 0 0 0 14.5 4.5L20 16"/><path d="M20 21v-5h-5"/>)svg"},
    {"info", R"svg(<circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 7.5v.01"/>)svg"},
    {"help", R"svg(<circle cx="12" cy="12" r="9"/><path d="M9.5 9.5a2.5 2.5 0 1 1 3.5 2.3c-.6.3-1 .9-1 1.6v.6M12 17v.01"/>)svg"},
    {"share", R"svg(<circle cx="18" cy="5" r="2.5"/><circle cx="6" cy="12" r="2.5"/><circle cx="18" cy="19" r="2.5"/><path d="M8.2 10.8l7.6-4.4M8.2 13.2l7.6 4.4"/>)svg"},
    {"fullscreen", R"svg(<path d="M4 9V4h5M20 9V4h-5M4 15v5h5M20 15v5h-5"/>)svg"},
    {"chevron-up", R"svg(<path d="M6 15l6-6 6 6"/>)svg"},
    {"chevron-down", R"svg(<path d="M6 9l6 6 6-6"/>)svg"},
    {"chevron-left", R"svg(<path d="M15 6l-6 6 6 6"/>)svg"},
    {"chevron-right", R"svg(<path d="M9 6l6 6-6 6"/>)svg"},
    {"chevrons-left", R"svg(<path d="M11 6l-6 6 6 6M18 6l-6 6 6 6"/>)svg"},
    {"chevrons-right", R"svg(<path d="M6 6l6 6-6 6M13 6l6 6-6 6"/>)svg"},
    {"arrow-left", R"svg(<path d="M19 12H5M11 6l-6 6 6 6"/>)svg"},
    {"arrow-right", R"svg(<path d="M5 12h14M13 6l6 6-6 6"/>)svg"},
    {"arrow-up", R"svg(<path d="M12 19V5M6 11l6-6 6 6"/>)svg"},
    {"arrow-down", R"svg(<path d="M12 5v14M6 13l6 6 6-6"/>)svg"},
    {"check", R"svg(<path d="M5 12.5l4.5 4.5L19 7"/>)svg"},
    {"more", R"svg(<circle cx="6" cy="12" r="1.2" fill="currentColor"/><circle cx="12" cy="12" r="1.2" fill="currentColor"/><circle cx="18" cy="12" r="1.2" fill="currentColor"/>)svg"},
    {"pin", R"svg(<path d="M9 4h6l-1 5 3 3v2H7v-2l3-3z"/><path d="M12 14v6"/>)svg"},
    {"star", R"svg(<path d="M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z"/>)svg"},
    {"tag", R"svg(<path d="M3 12V4a1 1 0 0 1 1-1h8l9 9-9 9z"/><circle cx="7.5" cy="7.5" r="1.5"/>)svg"},
    {"calendar", R"svg(<rect x="3.5" y="5" width="17" height="15" rx="2"/><path d="M3.5 10h17M8 3v4M16 3v4"/>)svg"},
    {"clock", R"svg(<circle cx="12" cy="12" r="9"/><path d="M12 7v5l3 2"/>)svg"},
    {"link", R"svg(<path d="M10 14a4.5 4.5 0 0 0 6.4 0l3-3a4.5 4.5 0 0 0-6.4-6.4l-1 1"/><path d="M14 10a4.5 4.5 0 0 0-6.4 0l-3 3a4.5 4.5 0 0 0 6.4 6.4l1-1"/>)svg"},
    {"unlink", R"svg(<path d="M10 14a4.5 4.5 0 0 0 6.4 0l3-3a4.5 4.5 0 0 0-6.4-6.4l-1 1"/><path d="M14 10a4.5 4.5 0 0 0-6.4 0l-3 3a4.5 4.5 0 0 0 6.4 6.4l1-1"/><path d="M4 4l16 16"/>)svg"},
    {"image", R"svg(<rect x="3" y="4" width="18" height="16" rx="2"/><circle cx="8.5" cy="9.5" r="1.8"/><path d="M21 16l-5-5L5 20"/>)svg"},
    {"table", R"svg(<rect x="3" y="4" width="18" height="16" rx="1.5"/><path d="M3 9.5h18M3 14.8h18M9 4v16M15 4v16"/>)svg"},
    {"eye", R"svg(<path d="M2 12s3.5-7 10-7 10 7 10 7-3.5 7-10 7S2 12 2 12z"/><circle cx="12" cy="12" r="3"/>)svg"},
    {"sidebar", R"svg(<rect x="3" y="4" width="18" height="16" rx="2"/><path d="M9 4v16"/>)svg"},
    {"list-view", R"svg(<path d="M8 6h13M8 12h13M8 18h13"/><path d="M3.5 6h.01M3.5 12h.01M3.5 18h.01" stroke-width="2.6"/>)svg"},
    {"grid-view", R"svg(<rect x="3.5" y="3.5" width="7" height="7" rx="1"/><rect x="13.5" y="3.5" width="7" height="7" rx="1"/><rect x="3.5" y="13.5" width="7" height="7" rx="1"/><rect x="13.5" y="13.5" width="7" height="7" rx="1"/>)svg"},
    {"drive", R"svg(<rect x="3" y="13" width="18" height="7" rx="2"/><path d="M5 13l2.5-7h9L19 13"/><path d="M17 16.5h.01" stroke-width="2.6"/>)svg"},
    {"desktop", R"svg(<rect x="3" y="4" width="18" height="12" rx="1.5"/><path d="M9 20h6M12 16v4"/>)svg"},
    {"download", R"svg(<path d="M12 3v12"/><path d="M8 11l4 4 4-4"/><path d="M4 21h16"/>)svg"},
    {"music", R"svg(<path d="M9 18V5l11-2v13"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/>)svg"},
    {"video", R"svg(<rect x="3" y="6" width="13" height="12" rx="2"/><path d="M16 10l5-3v10l-5-3"/>)svg"},
    {"user", R"svg(<circle cx="12" cy="8" r="4"/><path d="M4 21a8 8 0 0 1 16 0"/>)svg"},
    {"lock", R"svg(<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V7a4 4 0 0 1 8 0v4"/>)svg"},
    {"palette", R"svg(<path d="M12 3a9 9 0 0 0 0 18c1.1 0 1.6-.9 1.2-1.8-.5-1-.1-2.2 1.2-2.2H17a4 4 0 0 0 4-4c0-5.5-4-10-9-10z"/><circle cx="7.5" cy="11" r="1.2" fill="currentColor"/><circle cx="10" cy="7" r="1.2" fill="currentColor"/><circle cx="15" cy="7.5" r="1.2" fill="currentColor"/>)svg"},
    {"open-external", R"svg(<path d="M14 4h6v6M20 4l-9 9"/><path d="M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5"/>)svg"},
    {"rename", R"svg(<rect x="3" y="7" width="13" height="10" rx="1.5"/><path d="M19 5v14M17 5h4M17 19h4"/>)svg"},
    {"launcher", R"svg(<path d="M5 5h8M5 5v8M19 19L8 8M19 19v-6M19 19h-6"/>)svg"},
    {"history", R"svg(<path d="M3 12a9 9 0 1 0 3-6.7L3 8"/><path d="M3 3v5h5"/><path d="M12 7v5l3 2"/>)svg"},
    {"keyboard", R"svg(<rect x="2.5" y="6" width="19" height="12" rx="2"/><path d="M6 10h.01M10 10h.01M14 10h.01M18 10h.01M7 14h10" stroke-width="2"/>)svg"},
    // --- editing --------------------------------------------------------------------
    {"undo", R"svg(<path d="M9 14L4 9l5-5"/><path d="M4 9h11a5 5 0 0 1 0 10h-3"/>)svg"},
    {"redo", R"svg(<path d="M15 14l5-5-5-5"/><path d="M20 9H9a5 5 0 0 0 0 10h3"/>)svg"},
    {"cut", R"svg(<circle cx="6" cy="18" r="3"/><circle cx="18" cy="18" r="3"/><path d="M8.1 16L18 4M15.9 16L6 4"/>)svg"},
    {"copy", R"svg(<rect x="8" y="8" width="13" height="13" rx="2"/><path d="M16 8V5a2 2 0 0 0-2-2H5a2 2 0 0 0-2 2v9a2 2 0 0 0 2 2h3"/>)svg"},
    {"paste", R"svg(<path d="M9 4H6a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V6a2 2 0 0 0-2-2h-3"/><rect x="9" y="2.5" width="6" height="3.5" rx="1"/><path d="M8 11h8M8 15h5"/>)svg"},
    {"format-painter", R"svg(<rect x="4" y="3" width="13" height="6" rx="1.5"/><path d="M17 6h2a1 1 0 0 1 1 1v3a1 1 0 0 1-1 1h-7v3"/><rect x="10.5" y="14" width="3" height="7" rx="1"/>)svg"},
    {"select-all", R"svg(<path d="M4 8V5a1 1 0 0 1 1-1h3M16 4h3a1 1 0 0 1 1 1v3M20 16v3a1 1 0 0 1-1 1h-3M8 20H5a1 1 0 0 1-1-1v-3"/><rect x="8" y="8" width="8" height="8" rx="1"/>)svg"},
    // --- text formatting ------------------------------------------------------------
    {"bold", R"svg(<text x="12" y="18" font-family="Segoe UI" font-size="16" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">B</text>)svg"},
    {"italic", R"svg(<path d="M19 4h-9M14 20H5M15 4L9 20"/>)svg"},
    {"underline", R"svg(<path d="M7 4v6a5 5 0 0 0 10 0V4"/><path d="M5 20h14"/>)svg"},
    {"strikethrough", R"svg(<path d="M16 6.5C15.3 5 13.8 4 12 4c-2.5 0-4.5 1.5-4.5 3.5 0 4 9 3 9 8 0 2-2 3.5-4.5 3.5-2 0-3.6-1-4.3-2.5"/><path d="M4 12h16"/>)svg"},
    {"superscript", R"svg(<path d="M4 8l8 10M12 8l-8 10"/><path d="M15.5 5.5a2 2 0 1 1 3.4 1.4L15.5 10H20"/>)svg"},
    {"subscript", R"svg(<path d="M4 6l8 10M12 6L4 16"/><path d="M15.5 15.5a2 2 0 1 1 3.4 1.4L15.5 20H20"/>)svg"},
    {"font-color", R"svg(<path d="M6.5 16L12 3.5 17.5 16M8.7 11.5h6.6"/>)svg"},
    {"highlight", R"svg(<path d="M14.5 3.5l5 5L11 17H6v-5z"/><path d="M12 6l5 5"/>)svg"},
    {"fill-color", R"svg(<path d="M5 11l6.5-6.5L18 11l-6.5 6.5z"/><path d="M5 11h13"/><path d="M20 13.5s1.5 1.8 1.5 2.8a1.5 1.5 0 0 1-3 0c0-1 1.5-2.8 1.5-2.8z"/>)svg"},
    {"clear-format", R"svg(<path d="M4 7V5h12v2M10 5l-3 14M6 19h4"/><path d="M15 14l5 5M20 14l-5 5"/>)svg"},
    {"font-grow", R"svg(<path d="M3 19L8.5 6 14 19M5 14.5h7"/><path d="M16.5 9.5L19 6.5l2.5 3"/>)svg"},
    {"font-shrink", R"svg(<path d="M3 19L8.5 6 14 19M5 14.5h7"/><path d="M16.5 6.5L19 9.5l2.5-3"/>)svg"},
    {"change-case", R"svg(<text x="12" y="17" font-family="Segoe UI" font-size="12" text-anchor="middle" fill="currentColor" stroke="none">Aa</text>)svg"},
    {"align-left", R"svg(<path d="M4 6h16M4 10h10M4 14h16M4 18h10"/>)svg"},
    {"align-center", R"svg(<path d="M4 6h16M7 10h10M4 14h16M7 18h10"/>)svg"},
    {"align-right", R"svg(<path d="M4 6h16M10 10h10M4 14h16M10 18h10"/>)svg"},
    {"align-justify", R"svg(<path d="M4 6h16M4 10h16M4 14h16M4 18h16"/>)svg"},
    {"list-bullet", R"svg(<path d="M9 6h11M9 12h11M9 18h11"/><circle cx="4.5" cy="6" r="1.3" fill="currentColor" stroke="none"/><circle cx="4.5" cy="12" r="1.3" fill="currentColor" stroke="none"/><circle cx="4.5" cy="18" r="1.3" fill="currentColor" stroke="none"/>)svg"},
    {"list-number", R"svg(<path d="M10 6h10M10 12h10M10 18h10"/><g font-family="Segoe UI" font-size="6.5" text-anchor="middle" fill="currentColor" stroke="none"><text x="4.8" y="8.3">1</text><text x="4.8" y="14.3">2</text><text x="4.8" y="20.3">3</text></g>)svg"},
    {"list-check", R"svg(<path d="M11 6h9M11 12h9M11 18h9"/><path d="M3.5 6l1.5 1.5L8 4.5"/><rect x="3.5" y="10" width="4" height="4" rx=".8"/><path d="M3.5 18l1.5 1.5L8 16.5"/>)svg"},
    {"indent-increase", R"svg(<path d="M3 5h18M11 10h10M11 14h10M3 19h18"/><path d="M3 9l3 3-3 3"/>)svg"},
    {"indent-decrease", R"svg(<path d="M3 5h18M11 10h10M11 14h10M3 19h18"/><path d="M7 9l-3 3 3 3"/>)svg"},
    {"line-spacing", R"svg(<path d="M11 6h10M11 12h10M11 18h10"/><path d="M5 4v16M3 6.5L5 4l2 2.5M3 17.5L5 20l2-2.5"/>)svg"},
    {"pilcrow", R"svg(<path d="M13 4v16M17 4v16M19 4H9.5a4.5 4.5 0 0 0 0 9H13"/>)svg"},
    {"text-tool", R"svg(<path d="M5 6V4h14v2M12 4v16M9 20h6"/>)svg"},
    {"textbox", R"svg(<rect x="3" y="4" width="18" height="16" rx="1.5" stroke-dasharray="2 2"/><path d="M8 9h8M12 9v7"/>)svg"},
    {"symbol", R"svg(<text x="12" y="18" font-family="Segoe UI" font-size="16" text-anchor="middle" fill="currentColor" stroke="none">Ω</text>)svg"},
    {"page-break", R"svg(<path d="M6 3v5h12V3M6 21v-5h12v5"/><path d="M3 12h3M9 12h2M13 12h2M18 12h3"/>)svg"},
    {"header-footer", R"svg(<rect x="4" y="3" width="16" height="18" rx="1.5"/><path d="M7 6.5h10M7 17.5h10"/><path d="M7 10h10M7 13.5h6" opacity=".45"/>)svg"},
    {"page-size", R"svg(<rect x="7" y="3" width="13" height="18" rx="1.5"/><path d="M3.5 6v12M2.3 7.4L3.5 6l1.2 1.4M2.3 16.6l1.2 1.4 1.2-1.4"/>)svg"},
    {"orientation", R"svg(<rect x="3" y="8" width="11" height="13" rx="1.5"/><rect x="9" y="3" width="12" height="9" rx="1.5"/>)svg"},
    {"margins", R"svg(<rect x="4" y="3" width="16" height="18" rx="1.5"/><path d="M8 3v18M16 3v18M4 7h16M4 17h16" stroke-dasharray="1.5 2"/>)svg"},
    {"toc", R"svg(<path d="M4 6h10M4 10h14M7 14h11M7 18h11"/><path d="M20 6h.01" stroke-width="2.5"/>)svg"},
    {"word-count", R"svg(<text x="12" y="11" font-family="Segoe UI" font-size="8.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">ABC</text><path d="M5 15h14M5 19h9"/>)svg"},
    {"hr", R"svg(<path d="M3 12h18"/><path d="M6 7h12M6 17h12" opacity=".45"/>)svg"},
    {"styles", R"svg(<text x="9" y="16" font-family="Segoe UI" font-size="13" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">A</text><path d="M15 9h6M15 13h6M15 17h4"/>)svg"},
    // --- spreadsheet ----------------------------------------------------------------
    {"sum", R"svg(<path d="M18 7V4H6l6 8-6 8h12v-3"/>)svg"},
    {"function", R"svg(<text x="12" y="17" font-family="Cambria" font-size="14" font-style="italic" text-anchor="middle" fill="currentColor" stroke="none">fx</text>)svg"},
    {"sort-asc", R"svg(<path d="M7 4v16M3.5 7.5L7 4l3.5 3.5"/><g font-family="Segoe UI" font-size="7.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none"><text x="16.5" y="10">A</text><text x="16.5" y="20">Z</text></g>)svg"},
    {"sort-desc", R"svg(<path d="M7 4v16M3.5 16.5L7 20l3.5-3.5"/><g font-family="Segoe UI" font-size="7.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none"><text x="16.5" y="10">Z</text><text x="16.5" y="20">A</text></g>)svg"},
    {"filter", R"svg(<path d="M3.5 5h17l-6.5 8v6l-4 2v-8z"/>)svg"},
    {"merge-cells", R"svg(<rect x="3" y="5" width="18" height="14" rx="1.5"/><path d="M7.5 12h9M7.5 12l2-2M7.5 12l2 2M16.5 12l-2-2M16.5 12l-2 2"/>)svg"},
    {"wrap-text", R"svg(<path d="M4 6h16M4 12h13a3 3 0 0 1 0 6h-4"/><path d="M15 16l-2 2 2 2"/><path d="M4 18h5"/>)svg"},
    {"border-all", R"svg(<rect x="4" y="4" width="16" height="16"/><path d="M12 4v16M4 12h16"/>)svg"},
    {"border-outer", R"svg(<rect x="4" y="4" width="16" height="16"/><path d="M12 4v16M4 12h16" stroke-dasharray="1 2.5" opacity=".55"/>)svg"},
    {"border-none", R"svg(<rect x="4" y="4" width="16" height="16" stroke-dasharray="1 2.5" opacity=".55"/><path d="M12 4v16M4 12h16" stroke-dasharray="1 2.5" opacity=".55"/>)svg"},
    {"border-bottom", R"svg(<path d="M4 20h16" stroke-width="2.4"/><path d="M4 4h16v16M4 4v16" stroke-dasharray="1 2.5" opacity=".55"/>)svg"},
    {"row-insert", R"svg(<rect x="3" y="3.5" width="18" height="7" rx="1"/><path d="M12 14v7M8.5 17.5h7"/>)svg"},
    {"col-insert", R"svg(<rect x="3.5" y="3" width="7" height="18" rx="1"/><path d="M17.5 8.5v7M14 12h7"/>)svg"},
    {"row-delete", R"svg(<rect x="3" y="3.5" width="18" height="7" rx="1"/><path d="M9 15l6 6M15 15l-6 6"/>)svg"},
    {"col-delete", R"svg(<rect x="3.5" y="3" width="7" height="18" rx="1"/><path d="M14.5 9l6 6M20.5 9l-6 6"/>)svg"},
    {"percent", R"svg(<path d="M19 5L5 19"/><circle cx="7" cy="7" r="2.5"/><circle cx="17" cy="17" r="2.5"/>)svg"},
    {"currency", R"svg(<text x="12" y="18" font-family="Segoe UI" font-size="16" text-anchor="middle" fill="currentColor" stroke="none">₺</text>)svg"},
    {"decimal-inc", R"svg(<text x="14" y="10.5" font-family="Segoe UI" font-size="8" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">,00</text><path d="M3 16h11M6.5 12.5L3 16l3.5 3.5"/>)svg"},
    {"decimal-dec", R"svg(<text x="10" y="10.5" font-family="Segoe UI" font-size="8" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">,0</text><path d="M8 16h12M16.5 12.5L20 16l-3.5 3.5"/>)svg"},
    {"number-format", R"svg(<text x="12" y="16.5" font-family="Segoe UI" font-size="10.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">123</text>)svg"},
    {"valign-top", R"svg(<path d="M4 4h16M12 20V8M8 12l4-4 4 4"/>)svg"},
    {"valign-middle", R"svg(<path d="M4 12h16M12 3v5M9.5 5.5L12 8l2.5-2.5M12 21v-5M9.5 18.5L12 16l2.5 2.5"/>)svg"},
    {"valign-bottom", R"svg(<path d="M4 20h16M12 4v12M8 12l4 4 4-4"/>)svg"},
    {"freeze", R"svg(<rect x="3" y="3" width="18" height="18" rx="1.5"/><path d="M3 9h18M9 3v18" stroke-width="2.4"/>)svg"},
    {"sheet-add", R"svg(<rect x="3" y="4" width="13" height="16" rx="1.5"/><path d="M3 9h13M3 14h13M8 4v16"/><path d="M20 9v6M17 12h6"/>)svg"},
    {"chart-bar", R"svg(<path d="M4 20h16"/><rect x="6" y="11" width="3" height="9"/><rect x="11" y="6" width="3" height="14"/><rect x="16" y="13" width="3" height="7"/>)svg"},
    {"chart-hbar", R"svg(<path d="M4 4v16"/><rect x="4" y="6" width="10" height="3"/><rect x="4" y="11" width="15" height="3"/><rect x="4" y="16" width="7" height="3"/>)svg"},
    {"chart-line", R"svg(<path d="M4 4v16h16"/><path d="M7 15l4-5 3 3 5-6"/>)svg"},
    {"chart-area", R"svg(<path d="M4 20h16"/><path d="M4 17l5-6 4 3 7-8v14H4z"/>)svg"},
    {"chart-pie", R"svg(<path d="M21 12A9 9 0 1 1 12 3v9z"/><path d="M15 3.5A9 9 0 0 1 20.5 9H15z"/>)svg"},
    {"chart-scatter", R"svg(<path d="M4 4v16h16"/><circle cx="9" cy="14" r="1.3" fill="currentColor"/><circle cx="12" cy="9" r="1.3" fill="currentColor"/><circle cx="16" cy="12" r="1.3" fill="currentColor"/><circle cx="18" cy="6" r="1.3" fill="currentColor"/>)svg"},
    // --- presentation ---------------------------------------------------------------
    {"slide", R"svg(<rect x="3" y="5" width="18" height="12" rx="1.5"/><path d="M12 17v3M8 20h8"/>)svg"},
    {"slide-new", R"svg(<rect x="2.5" y="4" width="15" height="10.5" rx="1.5"/><path d="M20 14v6M17 17h6"/>)svg"},
    {"slide-duplicate", R"svg(<rect x="7" y="7" width="14" height="10" rx="1.5"/><path d="M4 14V5.5A1.5 1.5 0 0 1 5.5 4H16"/>)svg"},
    {"layout", R"svg(<rect x="3" y="4" width="18" height="16" rx="1.5"/><path d="M3 9h18M12 9v11"/>)svg"},
    {"play", R"svg(<path d="M7 4.5v15l12-7.5z"/>)svg"},
    {"play-current", R"svg(<rect x="3" y="5" width="18" height="12" rx="1.5"/><path d="M10 8v6l5-3z"/><path d="M12 17v3M8 20h8"/>)svg"},
    {"transition", R"svg(<rect x="3" y="6" width="10" height="12" rx="1"/><path d="M16 6h4v12h-4" stroke-dasharray="2 2"/><path d="M13 12h5M16 10l2 2-2 2"/>)svg"},
    {"bring-forward", R"svg(<rect x="4" y="4" width="11" height="11" rx="1.5"/><rect x="9" y="9" width="11" height="11" rx="1.5" fill="currentColor" fill-opacity=".35"/>)svg"},
    {"send-backward", R"svg(<rect x="4" y="4" width="11" height="11" rx="1.5" fill="currentColor" fill-opacity=".35"/><rect x="9" y="9" width="11" height="11" rx="1.5"/>)svg"},
    {"shape-rect", R"svg(<rect x="4" y="6" width="16" height="12" rx=".5"/>)svg"},
    {"shape-rounded", R"svg(<rect x="4" y="6" width="16" height="12" rx="3.5"/>)svg"},
    {"shape-ellipse", R"svg(<ellipse cx="12" cy="12" rx="8.5" ry="6.5"/>)svg"},
    {"shape-triangle", R"svg(<path d="M12 5l8.5 14h-17z"/>)svg"},
    {"shape-diamond", R"svg(<path d="M12 4l8 8-8 8-8-8z"/>)svg"},
    {"shape-line", R"svg(<path d="M5 19L19 5"/>)svg"},
    {"shape-arrow", R"svg(<path d="M5 19L19 5M10 5h9v9"/>)svg"},
    {"shape-block-arrow", R"svg(<path d="M3 9h10V5l8 7-8 7v-4H3z"/>)svg"},
    {"shape-star", R"svg(<path d="M12 3.5l2.6 5.3 5.9.9-4.3 4.1 1 5.8L12 16.9l-5.2 2.7 1-5.8-4.3-4.1 5.9-.9z"/>)svg"},
    {"shape-hexagon", R"svg(<path d="M7.5 4.5h9L21 12l-4.5 7.5h-9L3 12z"/>)svg"},
    {"shape-callout", R"svg(<path d="M4 5h16v10H10l-4 4v-4H4z"/>)svg"},
    {"shapes", R"svg(<rect x="3" y="11" width="9" height="9" rx="1"/><circle cx="16.5" cy="7.5" r="4.5"/>)svg"},
    {"line-color", R"svg(<path d="M16.5 3.5l4 4L8 20H4v-4z"/>)svg"},
    // --- pdf ------------------------------------------------------------------------
    {"hand", R"svg(<path d="M8 13V5.5a1.5 1.5 0 0 1 3 0V11"/><path d="M11 10.5V4a1.5 1.5 0 0 1 3 0v6.5"/><path d="M14 10.5V5.5a1.5 1.5 0 0 1 3 0V12"/><path d="M17 9.5a1.5 1.5 0 0 1 3 0V14a7 7 0 0 1-7 7h-1a7 7 0 0 1-5.5-2.7L3.6 15a1.5 1.5 0 0 1 2.3-2l2.1 2"/>)svg"},
    {"text-select", R"svg(<path d="M9 4h2a1 1 0 0 1 1 1v14a1 1 0 0 1-1 1H9M15 4h-2a1 1 0 0 0-1 1v14a1 1 0 0 0 1 1h2M10 12h4"/>)svg"},
    {"highlighter", R"svg(<path d="M9 11l-5 5v3h4l5-5"/><path d="M8.5 11.5L15 5l4 4-6.5 6.5z"/><path d="M14 20h7"/>)svg"},
    {"pen", R"svg(<path d="M16.5 3.5l4 4L8 20H4v-4z"/><path d="M14 6l4 4"/>)svg"},
    {"eraser", R"svg(<path d="M8 20h12"/><path d="M5.5 16.5l9-9.5a2 2 0 0 1 2.8 0l2.2 2.2a2 2 0 0 1 0 2.8L12 19.5H8.5z"/><path d="M10 11l5 5"/>)svg"},
    {"rotate-left", R"svg(<path d="M4 4v5h5"/><path d="M4.6 15a8 8 0 1 0 1.9-8.3L4 9"/>)svg"},
    {"rotate-right", R"svg(<path d="M20 4v5h-5"/><path d="M19.4 15a8 8 0 1 1-1.9-8.3L20 9"/>)svg"},
    {"fit-width", R"svg(<path d="M3 12h18M7 8l-4 4 4 4M17 8l4 4-4 4"/>)svg"},
    {"fit-page", R"svg(<rect x="4" y="3" width="16" height="18" rx="1.5"/><path d="M8 7h3M8 7v3M16 17h-3M16 17v-3"/>)svg"},
    {"actual-size", R"svg(<rect x="3" y="5" width="18" height="14" rx="2"/><text x="12" y="15.3" font-family="Segoe UI" font-size="7.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">1:1</text>)svg"},
    {"bookmark", R"svg(<path d="M6 3h12v18l-6-4.5L6 21z"/>)svg"},
    {"thumbnails", R"svg(<rect x="4" y="3" width="7" height="8" rx="1"/><rect x="13" y="3" width="7" height="8" rx="1"/><rect x="4" y="13" width="7" height="8" rx="1"/><rect x="13" y="13" width="7" height="8" rx="1"/>)svg"},
    {"signature", R"svg(<path d="M3 17c3-1 4-9 6-9s1 8 3 8 2-4 4-4 1 3 5 3"/><path d="M3 21h18"/>)svg"},
    {"note", R"svg(<path d="M4 4h16v11l-5 5H4z"/><path d="M15 20v-5h5"/><path d="M8 9h8M8 13h5"/>)svg"},
    {"page-delete", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M9.5 12.5l5 5M14.5 12.5l-5 5"/>)svg"},
    {"page-extract", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M12 18v-7M9 14l3-3 3 3"/>)svg"},
    {"merge-pdf", R"svg(<rect x="3" y="3" width="8" height="11" rx="1"/><rect x="13" y="3" width="8" height="11" rx="1"/><path d="M7 14v3h10v-3M12 17v4M10 19l2 2 2-2"/>)svg"},
    {"invert", R"svg(<circle cx="12" cy="12" r="9"/><path d="M12 3a9 9 0 0 1 0 18z" fill="currentColor"/>)svg"},
    {"first-page", R"svg(<path d="M6 5v14M18 6l-6 6 6 6"/>)svg"},
    {"last-page", R"svg(<path d="M18 5v14M6 6l6 6-6 6"/>)svg"},
    // --- mail -----------------------------------------------------------------------
    {"mail", R"svg(<rect x="3" y="5" width="18" height="14" rx="2"/><path d="M3.5 6.5l8.5 6.5 8.5-6.5"/>)svg"},
    {"mail-open", R"svg(<path d="M3 10l9-6 9 6v9a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M3.5 10.5l8.5 6 8.5-6"/>)svg"},
    {"inbox", R"svg(<path d="M3 13l2.5-8h13L21 13v6a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M3 13h5l1.5 3h5L16 13h5"/>)svg"},
    {"send", R"svg(<path d="M21 3L10 14"/><path d="M21 3l-7 18-4-7-7-4z"/>)svg"},
    {"reply", R"svg(<path d="M9 7L4 12l5 5"/><path d="M4 12h10a6 6 0 0 1 6 6v1"/>)svg"},
    {"reply-all", R"svg(<path d="M12 7l-5 5 5 5"/><path d="M7 7l-5 5 5 5"/><path d="M7 12h7a6 6 0 0 1 6 6v1"/>)svg"},
    {"forward", R"svg(<path d="M15 7l5 5-5 5"/><path d="M20 12H10a6 6 0 0 0-6 6v1"/>)svg"},
    {"attach", R"svg(<path d="M20 11.5l-8.3 8.3a5.2 5.2 0 0 1-7.4-7.4l8.6-8.6a3.5 3.5 0 0 1 5 5l-8.6 8.6a1.8 1.8 0 0 1-2.5-2.5l7.9-7.9"/>)svg"},
    {"archive", R"svg(<rect x="3" y="4" width="18" height="5" rx="1"/><path d="M5 9v10a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1V9M10 13h4"/>)svg"},
    {"drafts", R"svg(<path d="M4 20h4L19 9l-4-4L4 16z"/><path d="M13.5 6.5l4 4"/>)svg"},
    {"notebook", R"svg(<path d="M6 3h12a1 1 0 0 1 1 1v16a1 1 0 0 1-1 1H6a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1z"/><path d="M9 3v18M3 7h4M3 11h4M3 15h4"/>)svg"},
    // --- module glyphs (drawn white inside tiles) --------------------------------------
    {"g-writer", R"svg(<path d="M5 5.5h14M5 9.5h14M5 13.5h14M5 17.5h9"/>)svg"},
    {"g-sheet", R"svg(<rect x="4" y="4" width="16" height="16" rx="1.5"/><path d="M4 9.5h16M4 15h16M10 4v16"/>)svg"},
    {"g-slides", R"svg(<rect x="3.5" y="4.5" width="17" height="11" rx="1.2"/><path d="M12 15.5v4M8.5 19.5h7"/><path d="M7.5 12l3-3 2 2 3.5-3.5"/>)svg"},
    {"g-pdf", R"svg(<text x="12" y="16" font-family="Segoe UI" font-size="9.5" font-weight="700" text-anchor="middle" fill="currentColor" stroke="none">PDF</text>)svg"},
    {"g-notes", R"svg(<path d="M6.5 3.5h11a1 1 0 0 1 1 1v15a1 1 0 0 1-1 1h-11a1 1 0 0 1-1-1v-15a1 1 0 0 1 1-1z"/><path d="M9 8h7M9 12h7M9 16h4"/>)svg"},
    {"g-files", R"svg(<path d="M3 6.5A1.5 1.5 0 0 1 4.5 5H9l2 2.5h8.5A1.5 1.5 0 0 1 21 9v9.5a1.5 1.5 0 0 1-1.5 1.5h-15A1.5 1.5 0 0 1 3 18.5z"/>)svg"},
    {"g-mail", R"svg(<rect x="3" y="5.5" width="18" height="13" rx="1.5"/><path d="M3.5 7l8.5 6.5L20.5 7"/>)svg"},
    {"g-home", R"svg(<path d="M3.5 11.5l8.5-7 8.5 7"/><path d="M6 10v9.5h4.5V15h3v4.5H18V10"/>)svg"},
    {"g-text", R"svg(<path d="M6 6h12M6 10h12M6 14h12M6 18h7"/>)svg"},
    {"g-image", R"svg(<rect x="3.5" y="5" width="17" height="14" rx="1.5"/><circle cx="9" cy="10" r="1.6"/><path d="M20 16l-4.5-4.5L6 19"/>)svg"},
    {"g-file", R"svg(<path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/>)svg"},
    {"g-archive", R"svg(<rect x="4" y="3" width="16" height="18" rx="1.5"/><path d="M12 3v2M12 7v2M12 11v2"/><rect x="10.5" y="14" width="3" height="4" rx=".8"/>)svg"},
};

const QHash<QString, QByteArray> &iconTable()
{
    static const QHash<QString, QByteArray> table = [] {
        QHash<QString, QByteArray> t;
        for (const IconDef &def : kIcons)
            t.insert(QString::fromLatin1(def.name), QByteArray(def.svg));
        return t;
    }();
    return table;
}

QByteArray lineSvg(const QString &name, const QColor &color)
{
    QByteArray body = iconTable().value(name);
    body.replace("currentColor", color.name().toLatin1());
    return QByteArray("<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"")
           + color.name().toLatin1()
           + "\" stroke-width=\"1.7\" stroke-linecap=\"round\" stroke-linejoin=\"round\">" + body + "</svg>";
}

QByteArray tileSvg(const QString &glyph, const QColor &background)
{
    QByteArray body = iconTable().value(glyph);
    body.replace("currentColor", "#FFFFFF");
    const QByteArray bg = background.name().toLatin1();
    return QByteArray(
               "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\">"
               "<defs><linearGradient id=\"g\" x1=\"0\" y1=\"0\" x2=\"0\" y2=\"1\">"
               "<stop offset=\"0\" stop-color=\"#FFFFFF\" stop-opacity=\"0.20\"/>"
               "<stop offset=\"1\" stop-color=\"#000000\" stop-opacity=\"0.08\"/></linearGradient></defs>"
               "<rect x=\"1\" y=\"1\" width=\"22\" height=\"22\" rx=\"5.5\" fill=\"")
           + bg
           + "\"/><rect x=\"1\" y=\"1\" width=\"22\" height=\"22\" rx=\"5.5\" fill=\"url(#g)\"/>"
             "<g transform=\"translate(4.4 4.4) scale(0.633)\" fill=\"none\" stroke=\"#FFFFFF\" "
             "stroke-width=\"2.3\" stroke-linecap=\"round\" stroke-linejoin=\"round\">"
           + body + "</g></svg>";
}

QByteArray logoSvg()
{
    return QByteArrayLiteral(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\">"
        "<defs><linearGradient id=\"bg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"1\">"
        "<stop offset=\"0\" stop-color=\"#26365A\"/><stop offset=\"1\" stop-color=\"#141C30\"/></linearGradient></defs>"
        "<rect x=\"1\" y=\"1\" width=\"22\" height=\"22\" rx=\"6\" fill=\"url(#bg)\"/>"
        "<g fill=\"none\" stroke-width=\"3.3\" transform=\"rotate(-90 12 12)\">"
        "<circle cx=\"12\" cy=\"12\" r=\"6.2\" stroke=\"#4C8DF0\" stroke-dasharray=\"8.9 30.06\" stroke-dashoffset=\"0\"/>"
        "<circle cx=\"12\" cy=\"12\" r=\"6.2\" stroke=\"#2BB673\" stroke-dasharray=\"8.9 30.06\" stroke-dashoffset=\"-9.74\"/>"
        "<circle cx=\"12\" cy=\"12\" r=\"6.2\" stroke=\"#F2784B\" stroke-dasharray=\"8.9 30.06\" stroke-dashoffset=\"-19.48\"/>"
        "<circle cx=\"12\" cy=\"12\" r=\"6.2\" stroke=\"#A970F0\" stroke-dasharray=\"8.9 30.06\" stroke-dashoffset=\"-29.22\"/>"
        "</g></svg>");
}

class SvgIconEngine final : public QIconEngine
{
public:
    enum class Kind { Line, Tile, Logo };

    SvgIconEngine(Kind kind, const QString &name, const QColor &color)
        : m_kind(kind), m_name(name), m_color(color)
    {
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : qApp->devicePixelRatio();
        painter->drawPixmap(rect, scaledPixmap(rect.size(), mode, state, dpr));
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        return scaledPixmap(size, mode, state, 1.0);
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        Q_UNUSED(state);
        const QSize px = (QSizeF(size) * scale).toSize();
        if (px.isEmpty())
            return {};
        const QColor color = colorFor(mode);
        const QString key = QStringLiteral("omni-icon|%1|%2|%3|%4|%5x%6")
                                .arg(int(m_kind))
                                .arg(m_name, color.name(QColor::HexArgb))
                                .arg(int(mode))
                                .arg(px.width())
                                .arg(px.height());
        QPixmap pm;
        if (!QPixmapCache::find(key, &pm)) {
            pm = render(px, color, mode);
            QPixmapCache::insert(key, pm);
        }
        pm.setDevicePixelRatio(scale);
        return pm;
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }
    QIconEngine *clone() const override { return new SvgIconEngine(*this); }
    QString key() const override { return QStringLiteral("OmniSvgIconEngine"); }
    QString iconName() override { return m_name; }
    bool isNull() override { return false; }
    QList<QSize> availableSizes(QIcon::Mode, QIcon::State) override
    {
        return {QSize(16, 16), QSize(20, 20), QSize(24, 24), QSize(32, 32), QSize(48, 48)};
    }

private:
    QColor colorFor(QIcon::Mode mode) const
    {
        if (m_kind != Kind::Line)
            return m_color;
        const ThemePalette &p = ThemeManager::instance().palette();
        if (mode == QIcon::Disabled)
            return p.iconDisabled;
        return m_color.isValid() ? m_color : p.icon;
    }

    QPixmap render(const QSize &px, const QColor &color, QIcon::Mode mode) const
    {
        QByteArray svg;
        switch (m_kind) {
        case Kind::Line:
            svg = lineSvg(m_name, color);
            break;
        case Kind::Tile:
            svg = tileSvg(m_name, m_color);
            break;
        case Kind::Logo:
            svg = logoSvg();
            break;
        }
        QPixmap pm(px);
        pm.fill(Qt::transparent);
        QSvgRenderer renderer(svg);
        {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            renderer.render(&p, QRectF(QPointF(0, 0), QSizeF(px)));
        }
        if (m_kind != Kind::Line && mode == QIcon::Disabled) {
            QPixmap faded(px);
            faded.fill(Qt::transparent);
            QPainter p(&faded);
            p.setOpacity(0.35);
            p.drawPixmap(0, 0, pm);
            return faded;
        }
        return pm;
    }

    Kind m_kind;
    QString m_name;
    QColor m_color;
};

} // namespace

namespace Icons {

QIcon icon(const QString &name, const QColor &color)
{
    return QIcon(new SvgIconEngine(SvgIconEngine::Kind::Line, name, color));
}

QIcon tile(const QString &glyph, const QColor &background)
{
    return QIcon(new SvgIconEngine(SvgIconEngine::Kind::Tile, glyph, background));
}

QIcon logo()
{
    return QIcon(new SvgIconEngine(SvgIconEngine::Kind::Logo, QStringLiteral("logo"), QColor()));
}

QPixmap pixmap(const QString &name, int size, const QColor &color, qreal dpr)
{
    return icon(name, color).pixmap(QSize(size, size), dpr);
}

bool exists(const QString &name)
{
    return iconTable().contains(name);
}

} // namespace Icons
