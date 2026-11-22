#ifndef UIPAGE_H
#define UIPAGE_H

// The web interface files, embedded in the program as a fallback.
// minigit.exe prefers the files in the "ui" folder next to it (so they can be
// edited without rebuilding) and uses these copies if that folder is missing.
extern const char* const UI_HTML;   // ui/index.html
extern const char* const UI_CSS;    // ui/style.css
extern const char* const UI_JS;     // ui/app.js
extern const char* const UI_EDITOR_HTML;   // ui/editor.html
extern const char* const UI_EDITOR_JS;     // ui/editor.js

#endif
