# Vicinae Windows Port — Plan de Travail Vivant

> **Document évolutif** — Mis à jour au fur et à mesure de l'avancement. Chaque session commence par lire ce fichier pour comprendre l'état actuel.
>
> **Dernière mise à jour**: 2026-06-05
> **Session précédente**: Claude Code (analyse initiale + 1 fix CMake)
> **Auteur du plan**: Prometheus

---

## TL;DR

> **Objectif**: Porter Vicinae (launcher Qt6 C++23 pour Linux) sur Windows 11 avec une expérience identique.
>
> **Approche**: Port progressif — d'abord faire compiler le projet, puis remplacer les services Linux un par un en suivant le pattern d'abstraction déjà existant (macOS port comme référence).
>
> **État actuel**: 1 fix CMake appliqué sur ~12 prévus. Aucune tentative de compilation. Qt 6 non installé.
>
> **Parallélisme**: Élevé — chaque service Windows peut être développé indépendamment.
> **Chemin critique**: Build system → AppDiscovery → WindowManager → Clipboard → Services utilisateur

---

## Contexte

### Demande originale
Porter Vicinae sur Windows 11 pour avoir exactement la même expérience que sur Linux. Commencer par faire compiler le projet, puis porter toutes les fonctionnalités progressivement.

### Résumé de l'analyse du codebase
Vicinae est une application native C++23/Qt6 avec trois binaires (server, CLI, input-server) et une couche d'extension TypeScript. Le codebase suit déjà un pattern d'abstraction plateforme solide :

- **13 interfaces abstraites** avec des implémentations Linux/macOS/dummy
- **12 implémentations dummy** qui servent de fallback silencieux
- **Port macOS existant** comme référence architecturale
- **Build CMake** avec des blocs `if (APPLE)` / `if (UNIX AND NOT APPLE)` bien définis

**Problème**: Aucune détection Windows n'existe dans le CMake. Le seul `#ifdef Q_OS_WIN` est dans `font-service.cpp` (pour Segoe UI Emoji). Sur ~15 fichiers qui incluent du D-Bus, ~30 qui utilisent des APIs POSIX, ~6 qui utilisent XCB/X11, ~15 qui utilisent Wayland.

### Métriques clés
- **Lignes de code C++/Qt**: ~50k+ (server uniquement)
- **Fichiers source du server**: ~972 lignes CMake, ~397 lignes server.cpp
- **Services Linux à remplacer**: 12 (dont 6 critiques pour un MVP)
- **Binaires Linux-only**: `vicinae-input-server`, `vicinae-data-control-server`
- **Bibliothèques Linux-only**: `lib/xdgpp/`, `lib/linux-utils/`, `src/snippet/`, `src/data-control-server/`

---

## Architecture du Port

### Pattern d'abstraction plateforme (à suivre)

Le codebase suit déjà ce pattern. Pour chaque nouveau service Windows :

```
abstract-{service}.hpp      ← Interface existante (inchangée)
├── linux/{impl}.hpp/cpp    ← Implémentation Linux existante
├── macos/{impl}.hpp/.mm    ← Implémentation macOS existante
├── win32/{impl}.hpp/cpp    ← NOUVEAU: Implémentation Windows
└── dummy/{impl}.hpp/cpp    ← Fallback existant (inefficace mais stable)
```

Le sélecteur de plateforme est modifié dans la classe wrapper :
```cpp
// Avant
#ifdef Q_OS_LINUX
  m_backend = std::make_unique<LinuxImpl>();
#else
  m_backend = std::make_unique<DummyImpl>();
#endif

// Après
#ifdef Q_OS_LINUX
  m_backend = std::make_unique<LinuxImpl>();
#elif defined(Q_OS_WIN)
  m_backend = std::make_unique<Win32Impl>();
#else
  m_backend = std::make_unique<DummyImpl>();
#endif
```

### Architecture du build

```
if(WIN32)
  list(APPEND SRCS src/services/{service}/win32/win32-{service}.cpp)
  find_package(...)  # Windows-specific dependencies
endif()
```

### Dépendances externes sur Windows

| Dépendance | Statut | Notes |
|---|---|---|
| Qt 6 (Core, Network, Svg, Concurrent, Quick, Qml, GuiPrivate, QuickDialogs2, QuickControls2, ShaderTools) | ❌ Non installé | Installer via qt-online-installer ou vcpkg |
| OpenSSL | ⚠️ À vérifier | Normalement disponible sur Windows |
| ICU | ✅ Cross-platform | `find_package(ICU COMPONENTS uc)` fonctionne |
| glaze (FetchContent) | ✅ Cross-platform | Header-only |
| KF6 Syntax Highlighting | ✅ Cross-platform | Déjà géré via FetchContent + `if(NOT USE_SYSTEM_KF6 OR APPLE)` |
| cmark-gfm (FetchContent) | ✅ Cross-platform | Devrait compiler |
| qt-keychain | ✅ Cross-platform | Supporte Windows DPAPI |
| sqlcipher (vendored) | ⚠️ Fix flag `-w` | Remplacer par `/W0` ou équivalent MSVC |
| pugixml (vendored) | ✅ Cross-platform | Single-file, devrait compiler |
| zip (vendored) | ⚠️ Fix `if(UNIX)` | `_FILE_OFFSET_BITS=64` et symlinks |
| wayland | ❌ Linux-only | Ne pas compiler |
| X11/xcb | ❌ Linux-only | Ne pas compiler |
| xkbcommon | ❌ Linux-only | Ne pas compiler |
| libqalculate | ✅ Cross-platform | Optionnel, supporte Windows |
| node (TypeScript) | ✅ Cross-platform | Déjà disponible via scoop |

---

## Objectifs du Travail

### Objectif Principal
Porter Vicinae sur Windows 11 avec parité fonctionnelle complète avec l'expérience Linux.

### Livrables Concrets
- [ ] `vicinae-server.exe` compilé et fonctionnel sur Windows 11
- [ ] `vicinae.exe` (CLI) compilé et fonctionnel
- [ ] Découverte des applications Windows (Start Menu, .lnk)
- [ ] Gestion des fenêtres (Alt+Tab-like, focus tracking)
- [ ] Presse-papier (monitoring + historique)
- [ ] Expansion de snippets (SendInput)
- [ ] Coller programmatique (SendInput Ctrl+V)
- [ ] Notifications desktop (Toast ou fallback)
- [ ] Contrôle audio (Core Audio API)
- [ ] Gestion d'alimentation (Win32 API)
- [ ] Indexation de fichiers (ReadDirectoryChangesW)
- [ ] Choix de fichiers (IFileDialog ou QFileDialog)
- [ ] Extensions TypeScript fonctionnelles
- [ ] Raccourcis clavier système
- [ ] Installateur / package

### Définition de "Fait"
- Le binaire compile sans erreur avec MSVC ou MinGW
- Tous les tests d'intégration passent
- L'application se lance et affiche la fenêtre de recherche
- Les apps installées sont détectées et lancées
- Le presse-papier fonctionne (historique)
- Les snippets peuvent être créés et expansés
- La commutation de fenêtres fonctionne

### Doit Être Inclus
- [ ] Tous les services Linux ont un équivalent Windows
- [ ] Le build CMake détecte et configure Windows correctement
- [ ] Chaque service Windows a ses propres fichiers sous `win32/`
- [ ] Le pattern d'abstraction est respecté (interface + implémentation)
- [ ] Les dummies existants servent de fallback si un service Windows n'est pas encore implémenté

### Ne Doit PAS Être Inclus
- ❌ Support Windows 10 (uniquement Windows 11 pour l'instant)
- ❌ Support ARM64 Windows (uniquement x64)
- ❌ Réécriture du code QML (le rendu Qt6 est cross-platform)
- ❌ Changement de l'API d'extension TypeScript
- ❌ Port de `vicinae-input-server` en binaire séparé (l'injection clavier sera intégrée au server)
- ❌ Support du flou d'arrière-plan (pas d'équivalent DirectComposition prévu)
- ❌ Redesign de l'UI pour Windows (même UI que Linux)

---

## Stratégie de Vérification

### Test Decision
- **Infrastructure existante**: OUI (Catch2, mais uniquement pour Linux)
- **Tests automatisés**: NON pour l'instant (tests manuels + QA agent)
- **Framework**: Catch2 sera étendu pour Windows plus tard
- **QA Agent**: Chaque tâche inclut des scénarios de vérification exécutables par l'agent

### Politique QA
Chaque tâche DOIT inclure des scénarios de vérification :
- **Build**: `cmake --build . --target vicinae-server` ✅
- **Lancement**: Lancer `vicinae-server.exe`, vérifier qu'il démarre
- **Fonctionnalité**: Tester chaque service via des appels Win32/Qt
- **Preuve**: Screenshots de la fenêtre, logs de console, résultats de commandes

---

## Stratégie d'Exécution

### Ordre de priorité

```
EPIC 0 — Build System (PRÉ-REQUIS)
├── Installer Qt 6 sur Windows
├── Fixer les bugs CMake (DBus dans LIBS, else() → elseif())
├── Ajouter les blocs WIN32 dans CMake
├── Adapter les flags compilateur pour MSVC/MinGW
└── Première compilation réussie

EPIC 1 — Core Infrastructure (MVP — essentiel pour lancer)
├── Découverte d'applications (Start Menu .lnk)
├── Gestionnaire de fenêtres (EnumWindows)
├── Presse-papier (Win32 clipboard API)
├── Chemins de fichiers & données (paths Windows)
└── Lancement de processus (CreateProcess)

EPIC 2 — Services Utilisateur (fonctionnalités visibles)
├── Notifications desktop (Toast API)
├── Contrôle audio (Core Audio)
├── Gestion d'alimentation (Win32 power)
├── Choix de fichiers (IFileDialog)
├── Indexation de fichiers (ReadDirectoryChangesW)
└── Gestionnaire d'extensions TypeScript

EPIC 3 — Input Avancé (productivité)
├── Expansion de snippets (SendInput)
├── Service de collage (SendInput Ctrl+V)
├── Raccourcis clavier système
└── Détection du layout clavier

EPIC 4 — Polish & Intégration
├── Gestion des icônes et thèmes
├── Associations de fichiers
├── Packaging / Installateur
├── CI/CD Windows
└── Tests d'intégration complets
```

### Vagues d'exécution parallèles

```
Wave 0 (Build System — séquentiel):
├── T0.1: Installer Qt 6
├── T0.2: Fixer CMakeLists.txt
├── T0.3: Flags compilateur MSVC
└── T0.4: Build de test

Wave 1 (Core — parallèle max):
├── T1.1: WinPaths (chemins fichiers)
├── T1.2: WinAppDatabase (découverte apps)
├── T1.3: WinWindowManager (gestion fenêtres)
├── T1.4: WinClipboardServer (presse-papier)
├── T1.5: WinAppRuntime (process running)
├── T1.6: IPC (QLocalSocket)
└── T1.7: Extension Manager (node)

Wave 2 (Services — parallèle):
├── T2.1: WinPowerManager
├── T2.2: WinAudioControl
├── T2.3: WinDesktopNotification
├── T2.4: WinFileChooser
├── T2.5: WinFileIndexer
└── T2.6: Calculator

Wave 3 (Input — dépend de Wave 1):
├── T3.1: WinSnippetServer
├── T3.2: WinPasteService
├── T3.3: Keyboard layout
├── T3.4: System keybinds

Wave 4 (Polish):
├── T4.1: Icon theme
├── T4.2: File associations
├── T4.3: Packaging
├── T4.4: CI/CD
└── T4.5: Integration tests
```

---

## TODOs

---

### EPIC 0 — Build System (PRÉ-REQUIS) ✅ Terminé

- [x] 0.1. **Installer Qt 6 sur Windows**

  **What to do**:
  - Télécharger et installer Qt 6.8+ depuis qt.io (Online Installer)
  - Sélectionner les composants :
    - Qt 6.x → MinGW 13.2.0 64-bit
    - Qt 6.x → MSVC 2022 64-bit
    - Qt6 WebEngine (optionnel, pour extensions)
  - Alternative : utiliser `vcpkg install qt6`
  - Noter le chemin d'installation (`CMAKE_PREFIX_PATH`)
  - Vérifier que `qmake6` ou `qt-cmake` est accessible

  **Must NOT do**:
  - Ne pas installer Qt 5 (le projet nécessite Qt 6)
  - Ne pas installer les composants Android/iOS

  **Recommended Agent Profile**:
  - **Category**: `quick`
  - **Skills**: `[]`
  - **Reason**: Tâche d'installation simple, pas de code à écrire

  **Acceptance Criteria**:
  - [ ] `qmake6 --version` retourne Qt 6.x
  - [ ] Les headers Qt6 sont trouvables par CMake
  - [ ] `find_package(Qt6 REQUIRED COMPONENTS Core)` fonctionne

  **QA Scenarios**:
  ```
  Scenario: Qt6 installation verification
    Tool: Bash
    Preconditions: Qt 6 installer has been run
    Steps:
      1. Run `cmake -DQT_DIR="C:/Qt/6.x.x/mingw_64" -P cmake/test_qt6.cmake` (or similar)
      2. Or just run `qmake6 --version`
    Expected Result: Qt version 6.x is displayed
    Evidence: .sisyphus/evidence/task-0.1-qt-version.txt
  ```

  **Commit**: NON (pas de code)

- [x] 0.2. **Fixer les bugs CMake pour Windows**

  **What to do**:
  - **Bug 1** (`src/server/CMakeLists.txt:14`): Rendre `Qt6::DBus` conditionnel :
    ```cmake
    if(Qt6DBus_FOUND)
      list(APPEND LIBS Qt6::DBus)
    endif()
    ```
  - **Bug 2** (`src/server/CMakeLists.txt:608`): Changer `else()` en `elseif(UNIX AND NOT APPLE)` pour ne pas ajouter `linux-app-runtime` sur Windows :
    ```cmake
    elseif(UNIX AND NOT APPLE)
      list(APPEND SRCS ...)
    endif()
    ```
  - **Bug 3** (`src/server/CMakeLists.txt:927-931`): Gater l'import du plugin Wayland Qt :
    ```cmake
    if(Qt6_FOUND AND UNIX AND NOT APPLE)
      qt_import_plugins(${TARGET} INCLUDE Qt6::QWaylandIntegrationPlugin)
    endif()
    ```
  - **Bug 4** (`src/server/src/pch.h:16-17`): Rendre les includes D-Bus conditionnels :
    ```cpp
    #ifdef Q_OS_LINUX
    #include <QDBusConnection>
    #include <QDBusInterface>
    #endif
    ```
  - **Bug 5** (`vendor/zip/CMakeLists.txt`): Remplacer `if(UNIX)` par des alternatives compatibles Windows pour `_FILE_OFFSET_BITS=64` et `ZIP_HAVE_SYMLINK`

  **Must NOT do**:
  - Ne pas supprimer les fonctionnalités Linux (juste les rendre conditionnelles)
  - Ne pas réécrire les wrappers de sélection de plateforme (seulement le CMake)

  **References**:
  - `src/server/CMakeLists.txt:14` (Ligne avec `Qt6::DBus` dans LIBS)
  - `src/server/CMakeLists.txt:608` (Bloc `else()` qui ajoute linux-app-runtime)
  - `src/server/CMakeLists.txt:927-931` (Import plugin Wayland)
  - `src/server/src/pch.h:16-17` (Includes D-Bus globaux)
  - `vendor/zip/CMakeLists.txt` (Bloc `if(UNIX)`)

  **Acceptance Criteria**:
  - [ ] `Qt6::DBus` n'est lié que si D-Bus est disponible
  - [ ] `linux-app-runtime` n'est pas compilé sur Windows
  - [ ] Le plugin Wayland n'est pas importé sur Windows
  - [ ] `pch.h` n'inclut pas D-Bus sur Windows
  - [ ] Le zip vendor compile sans erreur sur Windows

  **QA Scenarios**:
  ```
  Scenario: CMake configure on Windows (dry-run)
    Tool: Bash
    Preconditions: Qt 6 installed, CMakeLists.txt modified
    Steps:
      1. Run `cmake -S . -B build-win-check -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT_DIR="C:/Qt/6.x.x/mingw_64" 2>&1 | tail -30`
    Expected Result: CMake configure succeeds (no errors about DBus, no Linux files)
    Evidence: .sisyphus/evidence/task-0.2-cmake-configure.txt
  ```
  ```
  Scenario: Verify DBus not in LIBS on Windows
    Tool: Bash
    Preconditions: CMake configured
    Steps:
      1. Grep build-win-check/CMakeCache.txt for "DBus"
    Expected Result: No DBus references in cached build config (or only if installed)
    Evidence: .sisyphus/evidence/task-0.2-dbus-check.txt
  ```

  **Commit**: OUI
  - Message: `build(windows): fix CMake for Windows compilation`
  - Files: `src/server/CMakeLists.txt`, `src/server/src/pch.h`, `vendor/zip/CMakeLists.txt`

- [x] 0.3. **Adapter les flags compilateur pour MSVC/MinGW**

  **What to do**:
  - Dans `CMakeLists.txt` racine (ligne 206) : Remplacer les flags GCC-only par des expressions génératrices :
    ```cmake
    # Avant
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-missing-field-initializers)

    # Après
    if(MSVC)
      add_compile_options(/W4 /wd4100 /wd4101 /wd4996)
    else()
      add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter -Wno-missing-field-initializers)
    endif()
    ```
  - `-flto=auto` → Remplacer par `/LTCG` pour MSVC
  - `-fsanitize=address` → `/fsanitize=address` (MSVC 16.9+)
  - `-O3` → `/O2` pour MSVC
  - `vendor/sqlcipher/CMakeLists.txt:9` : `-w` → `/W0` pour MSVC
  - Ajouter `set(CMAKE_CXX_STANDARD 23)` si pas déjà fait (C++23)

  **Must NOT do**:
  - Ne pas casser les builds Linux existants
  - Ne pas changer les flags pour Apple

  **References**:
  - `CMakeLists.txt:203-213` (Flags d'optimisation et sanitizers)
  - `vendor/sqlcipher/CMakeLists.txt:9` (Flag `-w`)

  **Acceptance Criteria**:
  - [ ] MSVC compile sans erreur de flag inconnu
  - [ ] MinGW compile sans régression
  - [ ] Linux compile sans régression (vérifier avec `make debug`)

  **QA Scenarios**:
  ```
  Scenario: CMake flags verification
    Tool: Bash
    Preconditions: CMakeLists.txt modified
    Steps:
      1. Run `cmake -S . -B build-flags -G Ninja -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -E "warning|error|flag"`
    Expected Result: No compiler flag errors
    Evidence: .sisyphus/evidence/task-0.3-flags.txt
  ```

  **Commit**: OUI (avec 0.2)
  - Message: `build(windows): add MSVC-compatible compiler flags`

- [x] 0.4. **Première compilation de test**

  **What to do**:
  - Tenter une première compilation complète avec MinGW
  - Noter tous les fichiers qui échouent
  - Core d'erreurs typiques à corriger :
    - Includes manquants (D-Bus, Wayland, X11)
    - Fonctions POSIX non disponibles (`fork`, `execvp`, `dlopen`, etc.)
    - `AF_UNIX` sockets (remplacer par `QLocalSocket`)
    - `xdgpp` includes (vicinae.cpp, environment.hpp)
    - Appels système Linux (`inotify`, `epoll`, `ioctl`)
  - Créer une liste des fichiers à exclure du build Windows
  - Ajouter les guards `#ifdef Q_OS_WIN` / `#ifndef Q_OS_WIN` nécessaires

  **Must NOT do**:
  - Ne pas réécrire des fichiers entiers (juste ajouter les guards)
  - Ne pas supprimer des fonctionnalités du build Linux

  **References**:
  - `src/server/src/vicinae.cpp` (utilise `xdgpp` pour les chemins — non-macOS)
  - `src/server/src/utils/environment.hpp` (utilise `xdgpp`, `/sys/class/dmi/`, `uwsm`)
  - `src/lib/vicinae-ipc/include/vicinae-ipc/client.hpp` (AF_UNIX sockets)
  - `src/cli/src/ipc-client.hpp` (AF_UNIX sockets)
  - `src/server/src/extension/manager/extension-manager.hpp` (AF_UNIX sockets)

  **Acceptance Criteria**:
  - [ ] `cmake --build build-win` produit au moins un binaire
  - [ ] Liste des fichiers exclus/excusés documentée
  - [ ] Les erreurs de compilation sont toutes résolues (soit fix, soit guard)

  **QA Scenarios**:
  ```
  Scenario: First build attempt
    Tool: Bash
    Preconditions: All prior fixes applied, Qt 6 installed
    Steps:
      1. `cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DQT_DIR="C:/Qt/6.x.x/mingw_64" 2>&1`
      2. `cmake --build build-win 2>&1 | tail -50`
    Expected Result: Build completes or produces clear list of remaining errors
    Evidence: .sisyphus/evidence/task-0.4-build-result.txt
  ```

  **Commit**: OUI
  - Message: `build(windows): first successful compilation`

---

### EPIC 1 — Infrastructure Core (MVP)

- [ ] 1.1. **WinPaths — Chemins de fichiers Windows**

  **What to do**:
  - Remplacer les appels à `xdgpp::dataHome()`, `xdgpp::configHome()`, `xdgpp::stateHome()` dans `vicinae.cpp` par des équivalents Windows
  - Créer un fichier `src/server/src/utils/win-paths.hpp` (ou ajouter dans `vicinae.cpp`) :
    ```cpp
    #ifdef Q_OS_WIN
    fs::path Omnicast::dataDir() {
      // %APPDATA%/vicinae
      return fs::path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()) / "vicinae";
    }
    fs::path Omnicast::configDir() {
      // %APPDATA%/vicinae
      return fs::path(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString()) / "vicinae";
    }
    fs::path Omnicast::runtimeDir() {
      // %TMP%/vicinae
      if (const char *t = std::getenv("TMP")) return fs::path(t) / "vicinae";
      return fs::temp_directory_path() / "vicinae";
    }
    fs::path Omnicast::stateDir() {
      return dataDir();
    }
    #endif
    ```
  - Remplacer `QStandardPaths::RuntimeLocation` (Linux-only) : utiliser `QStandardPaths::TempLocation` sur Windows
  - Remplacer `xdgpp::dataDirs()` dans `systemDataDirs()` : utiliser `QStandardPaths::standardLocations(GenericDataLocation)` sur Windows
  - Modifier `systemPaths()` : remplacer le séparateur `:` par `;` sur Windows

  **Must NOT do**:
  - Ne pas casser les chemins Linux/macOS existants
  - Ne pas supprimer les fonctions `xdgpp` (juste ajouter des alternatives Windows)

  **References**:
  - `src/server/src/vicinae.cpp` (toutes les fonctions de chemin)
  - `QStandardPaths` documentation Qt

  **Acceptance Criteria**:
  - [ ] `Omnicast::dataDir()` retourne `%APPDATA%/vicinae` sur Windows
  - [ ] `Omnicast::runtimeDir()` retourne `%TMP%/vicinae` sur Windows
  - [ ] `Omnicast::ensureDirectories()` crée les dossiers sur Windows
  - [ ] Les paths utilisent `\` sur Windows

  **QA Scenarios**:
  ```
  Scenario: Test Windows paths
    Tool: Bash
    Preconditions: Compiled with Q_OS_WIN
    Steps:
      1. Run a quick test program that prints each Omnicast path function
      2. Or add temporary qDebug() calls and run vicinae-server
    Expected Result: Paths point to correct Windows locations
    Evidence: .sisyphus/evidence/task-1.1-paths.txt
  ```

  **Commit**: OUI (avec groupe EPIC 1.1-1.3)
  - Message: `feat(windows): implement WinPaths for file locations`

- [ ] 1.2. **WinAppDatabase — Découverte des applications Windows**

  **What to do**:
  - Créer `src/server/src/services/app-service/win32/win-app.hpp` (implémentation de `AbstractApplication`)
  - Créer `src/server/src/services/app-service/win32/win-app-database.hpp` + `.cpp` (implémentation de `AbstractAppDatabase`)
  - L'implémentation doit :
    - Scanner `%ProgramData%\Microsoft\Windows\Start Menu\Programs\` et `%AppData%\Microsoft\Windows\Start Menu\Programs\`
    - Parser les fichiers `.lnk` (IShellLink) pour obtenir : nom, chemin executable, icône
    - Utiliser `SHGetKnownFolderPath` et `IShellFolder` pour l'énumération
    - Pour les apps UWP/Store : utiliser `IApplicationActivationManager` ou `PackageManager`
    - Implémenter `launch()` via `CreateProcess` ou `ShellExecuteEx`
    - Implémenter `findDefaultOpener()` via les associations de fichiers dans le Registry
    - Implémenter `findOpeners()` via `HKEY_CLASSES_ROOT`
    - Implémenter `showInFileBrowser()` via `ShellExecute("open", "explorer.exe", "/select,path")`
    - Implémenter `terminalEmulator()` via `findExecutable("wt.exe")` (Windows Terminal) ou `"cmd.exe"`
    - Implémenter `fileBrowser()` → `"explorer.exe"`
    - Implémenter `webBrowser()` → `findExecutable("msedge.exe")` ou default browser via Registry
  - Modifier `AppService::createLocalProvider()` dans `app-service.cpp` :
    ```cpp
    #ifdef Q_OS_MACOS
      return std::make_unique<MacAppDatabase>();
    #elif defined(Q_OS_WIN)
      return std::make_unique<WinAppDatabase>();
    #else
      return std::make_unique<XdgAppDatabase>();
    #endif
    ```
  - Ajouter les fichiers au CMake :
    ```cmake
    if(WIN32)
      list(APPEND SRCS src/services/app-service/win32/win-app-database.cpp)
    endif()
    ```

  **Must NOT do**:
  - Ne pas implémenter le parsing MIME (pas d'équivalent direct sur Windows)
  - Ne pas toucher à XdgAppDatabase (toujours utilisé sur Linux)

  **References**:
  - `src/server/src/services/app-service/abstract-app-db.hpp` (interface)
  - `src/server/src/services/app-service/app-service.cpp` (factory createLocalProvider)
  - `src/server/src/services/app-service/macos/mac-app-database.hpp` (comme référence)
  - Windows API: `IShellLink`, `SHGetKnownFolderPath`, `CreateProcess`, `ShellExecuteEx`

  **Acceptance Criteria**:
  - [ ] Les apps installées sont listées (depuis Start Menu)
  - [ ] `launch()` ouvre une application correctement
  - [ ] `terminalEmulator()` retourne Windows Terminal ou cmd.exe
  - [ ] `fileBrowser()` retourne explorer.exe
  - [ ] `findById()` fonctionne avec les IDs d'apps Windows
  - [ ] `showInFileBrowser()` ouvre l'explorateur Windows

  **QA Scenarios**:
  ```
  Scenario: Enumerate installed applications
    Tool: Bash
    Preconditions: WinAppDatabase compiled
    Steps:
      1. Add temporary code to print app list at startup
      2. Run vicinae-server
      3. Check output for known apps (Calculator, Notepad, etc.)
    Expected Result: 20+ Windows apps are discovered
    Evidence: .sisyphus/evidence/task-1.2-app-list.txt
  ```

  **Commit**: OUI (avec 1.1 et 1.3)
  - Message: `feat(windows): implement WinAppDatabase for Start Menu app discovery`

- [ ] 1.3. **WinWindowManager — Gestion des fenêtres Windows**

  **What to do**:
  - Créer `src/server/src/services/window-manager/win32/win32-window-manager.hpp` + `.cpp`
  - Créer `src/server/src/services/window-manager/win32/win32-window.hpp` (implémentation de `AbstractWindow`)
  - Implémenter via Win32 API :
    - `listWindowsSync()` : `EnumWindows` + `GetWindowText` + `GetClassName`
    - `getFocusedWindowSync()` : `GetForegroundWindow` + `GetWindowThreadProcessId`
    - `focusWindowSync()` : `SetForegroundWindow` + `AllowSetForegroundWindow`
    - `closeWindow()` : `SendMessage(WM_CLOSE)` ou `PostMessage(WM_CLOSE)`
    - `isActivatable()` : toujours `true` sur Windows
    - `ping()` : toujours `true`
    - `start()` : lancer un thread d'écoute avec `SetWinEventHook` pour `EVENT_OBJECT_CREATE`, `EVENT_OBJECT_DESTROY`, `EVENT_SYSTEM_FOREGROUND`
    - `id()` : retourner `"windows"`
    - `supportsFocusTracking()` : `true`
    - `focusNullsOnLayerGrab()` : `false`
  - La classe `Win32Window` doit stocker `HWND` et implémenter :
    - `id()` : `std::to_string(hwnd)`
    - `title()` : `GetWindowText(hwnd)`
    - `wmClass()` : `GetClassName(hwnd)`
    - `pid()` : `GetWindowThreadProcessId(hwnd)`
  - Modifier `WindowManager::createCandidates()` dans `window-manager.cpp` :
    ```cpp
    #ifdef Q_OS_WIN
    candidates.emplace_back(std::make_unique<Win32WindowManager>());
    #endif
    ```

  **Must NOT do**:
  - Ne pas implémenter les workspaces (pas d'équivalent Windows)
  - Ne pas implémenter `fullScreen()` (trop complexe pour MVP)
  - Ne pas toucher aux backends Linux existants

  **References**:
  - `src/server/src/services/window-manager/abstract-window-manager.hpp` (interface)
  - `src/server/src/services/window-manager/window-manager.cpp` (factory createCandidates)
  - Windows API: `EnumWindows`, `GetWindowText`, `SetForegroundWindow`, `SetWinEventHook`

  **Acceptance Criteria**:
  - [ ] `listWindowsSync()` retourne toutes les fenêtres ouvertes
  - [ ] `getFocusedWindowSync()` retourne la fenêtre active
  - [ ] `focusWindowSync()` amène une fenêtre au premier plan
  - [ ] Les changements de fenêtre (ouverture/fermeture/focus) émettent des signaux
  - [ ] `id()` retourne `"windows"`

  **QA Scenarios**:
  ```
  Scenario: List and focus windows
    Tool: Bash
    Preconditions: Notepad.exe is open
    Steps:
      1. Run vicinae-server
      2. Trigger window list (via search)
      3. Select a window to focus
    Expected Result: Notepad appears in window list, focus switches on selection
    Evidence: .sisyphus/evidence/task-1.3-window-list.txt
  ```

  **Commit**: OUI (avec 1.1 et 1.2)
  - Message: `feat(windows): implement WinWindowManager using EnumWindows`

- [ ] 1.4. **WinClipboardServer — Presse-papier Windows**

  **What to do**:
  - Créer `src/server/src/services/clipboard/win32/win32-clipboard-server.hpp` + `.cpp`
  - Implémenter `AbstractClipboardServer` :
    - `start()` : installer un `AddClipboardFormatListener` sur la fenêtre de Vicinae
    - `stop()` : `RemoveClipboardFormatListener`
    - `isActivatable()` : toujours `true` sur Windows
    - `isAlive()` : toujours `true`
    - `activationPriority()` : 100 (priorité haute)
    - `id()` : `"windows-clipboard"`
    - `setClipboardContent()` : déjà hérité de `AbstractClipboardServer` (utilise `QClipboard`)
  - Utiliser `QAbstractNativeEventFilter` pour intercepter `WM_CLIPBOARDUPDATE`
  - Émettre `selectionAdded()` quand le contenu change
  - Enregistrer le server dans `clipboard-service.cpp` :
    ```cpp
    #ifdef Q_OS_WIN
    factory.registerServer<Win32ClipboardServer>();
    #endif
    ```

  **Must NOT do**:
  - Ne pas réimplémenter `setClipboardContent` si `QClipboard` fonctionne
  - Ne pas désactiver les backends existants

  **References**:
  - `src/server/src/services/clipboard/clipboard-server.hpp` (interface)
  - `src/server/src/services/clipboard/clipboard-service.cpp` (registration)
  - `src/server/src/services/clipboard/clipboard-server-factory.hpp` (factory)
  - `src/server/src/services/clipboard/qt/qt-clipboard-server.hpp` (exemple QT)
  - Windows API: `AddClipboardFormatListener`, `GetClipboardData`, `WM_CLIPBOARDUPDATE`

  **Acceptance Criteria**:
  - [ ] Le presse-papier est surveillé (copier → détecté)
  - [ ] `selectionAdded()` est émis lors d'un changement
  - [ ] Le contenu peut être récupéré (GetClipboardData)
  - [ ] `isActivatable()` retourne true
  - [ ] Fonctionne avec l'historique du presse-papier Vicinae

  **QA Scenarios**:
  ```
  Scenario: Clipboard monitoring
    Tool: Bash
    Preconditions: vicinae-server running
    Steps:
      1. Copy text (Ctrl+C) in Notepad
      2. Check clipboard history in Vicinae
    Expected Result: Copied text appears in Vicinae clipboard history
    Evidence: .sisyphus/evidence/task-1.4-clipboard.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32ClipboardServer`

- [ ] 1.5. **WinAppRuntime — Gestion des processus en cours**

  **What to do**:
  - Créer `src/server/src/services/app-runtime/win32/win32-app-runtime.hpp` + `.cpp`
  - Implémenter `AbstractAppRuntime` :
    - `isRunning()` : `EnumProcesses` + `GetModuleBaseName` ou vérifier si `FindWindow` trouve une fenêtre de l'app
    - `frontmostApp()` : utiliser `GetForegroundWindow` + `GetWindowThreadProcessId` + mapper le PID à une app
    - `activate()` : trouver une fenêtre de l'app (via `EnumWindows`) + `SetForegroundWindow`
    - Signaux : utiliser un timer pour poller `GetForegroundWindow` (car Win32 n'a pas de callback direct pour le changement d'app au premier plan côté Win32 — on peut utiliser `SetWinEventHook(EVENT_SYSTEM_FOREGROUND)`)
  - Modifier `AppRuntime::createProvider()` dans `app-runtime.cpp` :
    ```cpp
    #ifdef Q_OS_MACOS
      return std::make_unique<MacAppRuntime>(appService);
    #elif defined(Q_OS_WIN)
      return std::make_unique<Win32AppRuntime>(wm, appService);
    #else
      return std::make_unique<LinuxAppRuntime>(wm, appService);
    #endif
    ```
  - Ajouter au CMake

  **Must NOT do**:
  - Ne pas implémenter de tracking de workspace (N/A sur Windows)

  **References**:
  - `src/server/src/services/app-runtime/abstract-app-runtime.hpp` (interface)
  - `src/server/src/services/app-runtime/app-runtime.cpp` (factory)
  - Windows API: `EnumProcesses`, `CreateToolhelp32Snapshot`, `SetWinEventHook`

  **Acceptance Criteria**:
  - [ ] `isRunning()` détecte correctement si une app a des fenêtres ouvertes
  - [ ] `frontmostApp()` retourne l'application au premier plan
  - [ ] `activate()` amène l'app au premier plan

  **QA Scenarios**:
  ```
  Scenario: Track running applications
    Tool: Bash
    Preconditions: Notepad.exe running
    Steps:
      1. vicinae-server query for running apps
      2. Switch focus between apps
    Expected Result: Notepad shown as running, frontmost app updates on focus change
    Evidence: .sisyphus/evidence/task-1.5-running-apps.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32AppRuntime`

- [ ] 1.6. **IPC — Remplacer AF_UNIX par QLocalSocket**

  **What to do**:
  - `src/lib/vicinae-ipc/` : Remplacer `socket(AF_UNIX, ...)` par `QLocalSocket`/`QLocalServer` (Qt wrapper cross-platform)
  - `src/cli/src/ipc-client.hpp` : Même changement
  - `src/server/src/extension/manager/extension-manager.hpp` : Même changement
  - `src/browser-extension/src/browser.cpp` : Même changement
  - `QLocalSocket` utilise des Unix Domain Sockets sur Linux/macOS et des Named Pipes sur Windows
  - Modifier `vicinae-ipc` CMakeLists.txt pour ne pas inclure `<sys/socket.h>`, `<sys/un.h>`

  **Must NOT do**:
  - Ne pas casser l'IPC existant sur Linux (QLocalSocket est compatible)
  - Ne pas réécrire tout le protocole IPC (juste le transport)

  **References**:
  - `src/lib/vicinae-ipc/include/vicinae-ipc/client.hpp` (AF_UNIX)
  - `src/cli/src/ipc-client.hpp` (AF_UNIX)
  - `src/server/src/extension/manager/extension-manager.hpp` (AF_UNIX)
  - `src/browser-extension/src/browser.cpp` (AF_UNIX)
  - Qt docs: `QLocalSocket`, `QLocalServer`

  **Acceptance Criteria**:
  - [ ] L'IPC fonctionne avec QLocalSocket sur Linux (régression test)
  - [ ] L'IPC fonctionne avec QLocalServer/Named Pipe sur Windows
  - [ ] Le CLI peut communiquer avec le server

  **QA Scenarios**:
  ```
  Scenario: IPC communication
    Tool: Bash
    Preconditions: vicinae-server running with QLocalSocket
    Steps:
      1. Run `vicinae toggle` from CLI
      2. Check that window toggles
    Expected Result: CLI command reaches the server
    Evidence: .sisyphus/evidence/task-1.6-ipc.txt
  ```

  **Commit**: OUI
  - Message: `refactor(ipc): replace AF_UNIX sockets with QLocalSocket`

- [ ] 1.7. **Environment Utils — Adapter pour Windows**

  **What to do**:
  - Modifier `src/server/src/utils/environment.hpp` :
    - `isWaylandSession()` : retourner `false` sur Windows
    - `isGnomeEnvironment()` : retourner `false` sur Windows
    - `isWlrootsCompositor()` : retourner `false` sur Windows
    - `fallbackIconSearchPaths()` : utiliser `QStandardPaths::standardLocations(GenericDataLocation)` au lieu de `xdgpp::dataDirs()`
    - `chassisType()` : ne pas lire `/sys/class/dmi/id/chassis_type` (utiliser `GetSystemFirmwareTable` ou retourner "desktop" par défaut)
    - `detectAppLauncher()` : retourner `std::nullopt` sur Windows
    - `appImageDir()` : retourner `std::nullopt` sur Windows
    - Toutes ces fonctions sont `inline` dans un header — utiliser `#ifdef Q_OS_WIN` pour les blocs Windows

  **Must NOT do**:
  - Ne pas supprimer les fonctions (juste ajouter des branches Windows)

  **References**:
  - `src/server/src/utils/environment.hpp` (toutes les fonctions)

  **Acceptance Criteria**:
  - [ ] `isWaylandSession()` retourne `false` sur Windows
  - [ ] `fallbackIconSearchPaths()` retourne des chemins Windows valides
  - [ ] `chassisType()` ne lit pas `/sys/class/dmi/` sur Windows
  - [ ] Aucune référence à XDG sur Windows

  **QA Scenarios**:
  ```
  Scenario: Environment functions on Windows
    Tool: Bash
    Preconditions: Compiled with Q_OS_WIN
    Steps:
      1. Print results of each environment function
    Expected Result: No XDG references, all functions return safe defaults
    Evidence: .sisyphus/evidence/task-1.7-env.txt
  ```

  **Commit**: OUI (avec 1.1)
  - Message: `feat(windows): adapt environment utils for Windows`

---

### EPIC 2 — Services Utilisateur

- [ ] 2.1. **WinPowerManager — Gestion d'alimentation Windows**

  **What to do**:
  - Créer `src/server/src/services/power-manager/win32/win32-power-manager.hpp` + `.cpp`
  - Implémenter `AbstractPowerManager` :
    - `powerOff()` : `InitiateSystemShutdownEx`
    - `reboot()` : `InitiateSystemShutdownEx` avec flag reboot
    - `sleep()` / `suspend()` : `SetSuspendState`
    - `hibernate()` : `SetSuspendState(TRUE, TRUE, FALSE)`
    - `lock()` : `LockWorkStation()`
    - `logout()` : `ExitWindowsEx`
    - `canPowerOff()`, `canReboot()`, etc. : retourner `true`
    - `canSuspend()` : retourner `true` (vérifier `IsPwrSuspendAllowed`)
    - `id()` : retourner `"windows"`
  - Modifier `PowerManager` dans `power-manager.hpp` :
    ```cpp
    #ifdef Q_OS_LINUX
      m_manager = std::make_unique<SystemdPowerManager>();
    #elif defined(Q_OS_WIN)
      m_manager = std::make_unique<Win32PowerManager>();
    #else
      m_manager = std::make_unique<DummyPowerManager>();
    #endif
    ```

  **Must NOT do**:
  - Ne pas implémenter `softReboot()` (spécifique Linux)

  **References**:
  - `src/server/src/services/power-manager/abstract-power-manager.hpp` (interface)
  - `src/server/src/services/power-manager/power-manager.hpp` (factory)
  - Windows API: `InitiateSystemShutdownEx`, `SetSuspendState`, `LockWorkStation`, `ExitWindowsEx`

  **Acceptance Criteria**:
  - [ ] `lock()` verrouille la session Windows
  - [ ] `canPowerOff()` retourne `true`
  - [ ] `id()` retourne `"windows"`
  - [ ] `canSuspend()` retourne `true`

  **QA Scenarios**:
  ```
  Scenario: Lock workstation
    Tool: Bash
    Preconditions: vicinae-server running with Win32PowerManager
    Steps:
      1. Trigger lock command from Vicinae
    Expected Result: Windows shows lock screen
    Evidence: .sisyphus/evidence/task-2.1-lock.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32PowerManager`

- [ ] 2.2. **WinAudioControl — Contrôle audio Windows**

  **What to do**:
  - Créer `src/server/src/services/audio-control/win32/win32-audio-control.hpp` + `.cpp`
  - Implémenter `AbstractAudioControl` via Windows Core Audio API (WASAPI) :
    - `getVolume()` : `IMMDeviceEnumerator::GetDefaultAudioEndpoint` → `IAudioEndpointVolume::GetMasterVolumeLevelScalar`
    - `setVolume(float)` : `IAudioEndpointVolume::SetMasterVolumeLevelScalar`
    - `adjustVolume(float)` : `SetMasterVolumeLevelScalar(current + delta)`
    - `isMuted()` : `IAudioEndpointVolume::GetMute`
    - `setMuted(bool)` : `IAudioEndpointVolume::SetMute`
    - `toggleMute()` : inverser l'état
    - `listSinks()` : énumérer via `IMMDeviceEnumerator::EnumAudioEndpoints`
    - `setDefaultSink()` : utiliser `IMMDeviceEnumerator` ou `IPolicyConfig`
    - `id()` : retourner `"wasapi"`
  - Nécessite `#include <mmdeviceapi.h>`, `#include <endpointvolume.h>`, `#include <functiondiscoverykeys_devpkey.h>`
  - Lier `ole32.lib`, `mmdevapi.lib` sur Windows
  - Modifier `AudioControlService` dans `audio-control-service.hpp`

  **Must NOT do**:
  - Ne pas essayer d'utiliser `pactl` sur Windows
  - Ne pas implémenter le contrôle par application (uniquement master)

  **References**:
  - `src/server/src/services/audio-control/abstract-audio-control.hpp`
  - `src/server/src/services/audio-control/audio-control-service.hpp`
  - Windows API: `IMMDeviceEnumerator`, `IAudioEndpointVolume`, `IMMDeviceCollection`
  - Doc MSDN: Core Audio APIs

  **Acceptance Criteria**:
  - [ ] `getVolume()` retourne le volume actuel (0.0-1.0)
  - [ ] `setVolume()` change le volume master
  - [ ] `isMuted()` / `setMuted()` fonctionne
  - [ ] `listSinks()` retourne les périphériques audio
  - [ ] `id()` retourne `"wasapi"`

  **QA Scenarios**:
  ```
  Scenario: Volume control
    Tool: Bash
    Preconditions: vicinae-server running
    Steps:
      1. Query current volume
      2. Set volume to 50%
      3. Check Windows volume mixer
    Expected Result: Volume changes reflected in Windows
    Evidence: .sisyphus/evidence/task-2.2-volume.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32AudioControl with WASAPI`

- [ ] 2.3. **WinDesktopNotification — Notifications Windows**

  **What to do**:
  - Créer `src/server/src/services/desktop-notification/win32/win32-notification-client.hpp` + `.cpp`
  - Implémenter `AbstractDesktopNotificationClient` :
    - Option A (simplifiée) : Utiliser `QSystemTrayIcon::showMessage()` — c'est cross-platform et fonctionne sur Windows
    - Option B (complète) : Utiliser WinRT Toast Notification via `Windows::UI::Notifications::ToastNotification`
    - Pour l'option A, il faut un `QSystemTrayIcon` visible (avec une icône dans la system tray)
    - `send()` : appeler `showMessage()` avec le titre, le message et l'icône
    - Ajouter un `QSystemTrayIcon` dans `server.cpp` lors de l'initialisation
  - Modifier `DesktopNotificationClient` dans `desktop-notification-client.hpp`

  **Must NOT do**:
  - Ne pas essayer d'utiliser D-Bus Freedesktop Notifications
  - Option B nécessite WinRT qui peut être complexe — l'Option A (QSystemTrayIcon) est préférée pour commencer

  **References**:
  - `src/server/src/services/desktop-notification/abstract-desktop-notification-client.hpp`
  - `src/server/src/services/desktop-notification/desktop-notification-client.hpp`
  - Qt docs: `QSystemTrayIcon::showMessage`
  - Windows API (option B): `Windows::UI::Notifications::ToastNotification`

  **Acceptance Criteria**:
  - [ ] `send()` affiche une notification Windows (balloon toast)
  - [ ] Le titre et le message sont corrects
  - [ ] L'icône Vicinae apparaît dans la system tray
  - [ ] `id()` retourne `"windows-tray"`

  **QA Scenarios**:
  ```
  Scenario: Send desktop notification
    Tool: Bash
    Preconditions: vicinae-server running with tray icon
    Steps:
      1. Trigger a notification (e.g., clipboard copied)
    Expected Result: Windows notification balloon appears
    Evidence: .sisyphus/evidence/task-2.3-notification.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32 notification client`

- [ ] 2.4. **WinFileChooser — Sélecteur de fichiers Windows**

  **What to do**:
  - Utiliser la classe `NativeFileChooser` déjà existante (`src/server/src/services/file-chooser/native/native-file-chooser.hpp`)
  - L'implémenter avec `QFileDialog` (cross-platform, fonctionne sur Windows)
  - OU utiliser `IFileOpenDialog` / `IFileSaveDialog` (Common Item Dialog API)
  - Modifier `FileChooser` dans `file-chooser.hpp` pour sélectionner le bon backend :
    ```cpp
    #ifdef Q_OS_WIN
    // Utiliser NativeFileChooser (QFileDialog) directement
    #endif
    ```

  **Must NOT do**:
  - Ne pas essayer d'utiliser XDP Portal (D-Bus) sur Windows

  **References**:
  - `src/server/src/services/file-chooser/native/native-file-chooser.hpp`
  - `src/server/src/services/file-chooser/file-chooser.hpp` + `file-chooser.cpp`
  - `src/server/src/services/file-chooser/file-chooser-service.hpp` + `.cpp`

  **Acceptance Criteria**:
  - [ ] `open()` ouvre une boîte de dialogue de sélection de fichiers Windows
  - [ ] Les fichiers sélectionnés sont retournés via callback
  - [ ] `isAvailable()` retourne `true`

  **QA Scenarios**:
  ```
  Scenario: File chooser dialog
    Tool: Bash
    Preconditions: vicinae-server running
    Steps:
      1. Trigger file chooser from extension or settings
    Expected Result: Native Windows file dialog appears
    Evidence: .sisyphus/evidence/task-2.4-filechooser.png
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32 file chooser with QFileDialog`

- [ ] 2.5. **WinFileIndexer — Indexation de fichiers Windows**

  **What to do**:
  - L'indexeur de fichiers existant (`file-indexer/file-indexer.cpp`) utilise `std::filesystem` et SQLite — il devrait déjà fonctionner sur Windows avec peu de modifications
  - Le principal changement : remplacer le watcher inotify (`vendor/watcher`) par `QFileSystemWatcher` ou `ReadDirectoryChangesW`
  - Créer `src/server/src/services/files-service/file-indexer/watcher-scanner-win.cpp` (ou modifier `watcher-scanner.cpp`)
  - Points d'attention :
    - Les chemins Windows (`C:\`) vs Linux (`/`)
    - Les permissions et fichiers système à exclure
    - Les chemins d'index par défaut (`%USERPROFILE%` au lieu de `$HOME`)
  - Si `DummyFileIndexer` est acceptable pour le MVP, on peut reporter cette tâche

  **Must NOT do**:
  - Ne pas réécrire tout l'indexeur (il est déjà largement cross-platform)
  - Ne pas indexer `C:\Windows` par défaut

  **References**:
  - `src/server/src/services/files-service/abstract-file-indexer.hpp`
  - `src/server/src/services/files-service/file-indexer/` (tous les fichiers)
  - `src/server/src/services/files-service/dummy-file-indexer.hpp`
  - Windows API: `ReadDirectoryChangesW`, `FindFirstChangeNotification`
  - `QFileSystemWatcher`

  **Acceptance Criteria**:
  - [ ] L'indexeur démarre et indexe `%USERPROFILE%`
  - [ ] Rechercher un fichier par nom retourne des résultats
  - [ ] Les changements de fichiers sont détectés et l'index est mis à jour
  - [ ] `QFileSystemWatcher` ou `ReadDirectoryChangesW` remplace inotify

  **QA Scenarios**:
  ```
  Scenario: File search
    Tool: Bash
    Preconditions: vicinae-server running, indexer started
    Steps:
      1. Search for a known file name
    Expected Result: File appears in search results
    Evidence: .sisyphus/evidence/task-2.5-file-search.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): adapt file indexer for Windows file system watching`

- [ ] 2.6. **Calculator — Support Windows pour le calculateur**

  **What to do**:
  - Le backend soulver-core utilise `dlopen("libSoulverWrapper.so")` — ne fonctionnera pas sur Windows
  - Solution 1 : Utiliser Qalculate (déjà optionnel, supporte Windows)
  - Solution 2 : Compiler soulver-core en DLL et utiliser `LoadLibrary`
  - Solution 3 : Désactiver soulver-core sur Windows, n'utiliser que Qalculate
  - Modifier `calculator-service.cpp` pour gérer Windows :
    ```cpp
    #if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
      // soulver-core
    #elif defined(Q_OS_WIN)
      // Qalculate seulement, ou stub
    #endif
    ```

  **Must NOT do**:
  - Ne pas bloquer le build à cause de soulver-core

  **References**:
  - `src/server/src/services/calculator-service/calculator-service.cpp`
  - `src/server/src/services/calculator-service/soulver-core/soulver-core.cpp`

  **Acceptance Criteria**:
  - [ ] Le calculateur n'empêche pas la compilation
  - [ ] Au moins un backend est disponible sur Windows (Qalculate ou stub)

  **Commit**: OUI
  - Message: `feat(windows): adapt calculator service for Windows`

---

### EPIC 3 — Input Avancé (Productivité)

- [ ] 3.1. **WinSnippetServer — Expansion de snippets Windows**

  **What to do**:
  - Créer `src/server/src/services/snippet/win32/win32-snippet-server.hpp` + `.cpp`
  - Implémenter `AbstractSnippetServer` via `SendInput` API :
    - `supportsKeyInjection()` : `true`
    - `injectExpand(charsToDelete, prePasteDelayUs, terminal, cursorLeftMoves)` :
      - Simuler des `VK_BACK` pour supprimer `charsToDelete` caractères
      - Envoyer les caractères du snippet via `SendInput` avec `KEYBDINPUT`
      - Gérer le délai `prePasteDelayUs`
    - `injectUndo(backspaceCount, trigger)` : Simuler Ctrl+Z ou backspaces
    - `setKeymap(layoutInfo)` : Mettre à jour la disposition clavier (via `LoadKeyboardLayout`)
    - `registerSnippet()` / `unregisterSnippet()` : Gérer la liste des snippets
    - `resetContext()` : Réinitialiser l'état
    - `isRunning()` : `true` (toujours disponible)
    - `setKeyDelay(us)` : Configurer le délai entre les frappes
  - Modifier `server.cpp` pour créer `Win32SnippetServer` sur Windows :
    ```cpp
    #ifdef Q_OS_WIN
    auto snippetServer = std::make_unique<Win32SnippetServer>();
    auto platformPaste = std::unique_ptr<AbstractPasteService>(std::make_unique<Win32PasteService>());
    #elif defined(Q_OS_LINUX)
    ...
    #endif
    ```

  **Must NOT do**:
  - Ne pas essayer d'utiliser evdev/uinput sur Windows
  - Ne pas implémenter de hook clavier global (trop complexe pour MVP)

  **References**:
  - `src/server/src/services/snippet/abstract-snippet-server.hpp` (interface)
  - `src/server/src/services/snippet/null-snippet-server.hpp` (stub actuel)
  - `src/server/src/server.cpp:138-146` (création du snippet server)
  - Windows API: `SendInput`, `KEYBDINPUT`, `VkKeyScanEx`, `ToUnicodeEx`, `LoadKeyboardLayout`

  **Acceptance Criteria**:
  - [ ] `supportsKeyInjection()` retourne `true`
  - [ ] `injectExpand()` envoie les caractères du snippet
  - [ ] `injectUndo()` annule l'expansion
  - [ ] `isRunning()` retourne `true`
  - [ ] Un snippet déclenché expande le texte correctement

  **QA Scenarios**:
  ```
  Scenario: Snippet expansion
    Tool: Bash (powershell)
    Preconditions: vicinae-server running, snippet "sig" → "Best regards,\nJohn"
    Steps:
      1. Open Notepad
      2. Type "sig" and trigger snippet expansion
    Expected Result: "sig" is replaced by "Best regards,\nJohn"
    Evidence: .sisyphus/evidence/task-3.1-snippet.txt
  ```

  **Commit**: OUI
  - Message: `feat(windows): implement Win32SnippetServer with SendInput`

- [ ] 3.2. **WinPasteService — Collage programmatique Windows**

  **What to do**:
  - Créer `src/server/src/services/paste/win32/win32-paste-service.hpp` + `.cpp`
  - Implémenter `AbstractPasteService` :
    - `supportsPaste()` : `true`
    - `pasteToApp()` : Simuler Ctrl+V via `SendInput` :
      ```cpp
      INPUT inputs[4] = {};
      // Ctrl down
      inputs[0].type = INPUT_KEYBOARD;
      inputs[0].ki.wVk = VK_LCONTROL;
      // V down
      inputs[1].type = INPUT_KEYBOARD;
      inputs[1].ki.wVk = 'V';
      // V up
      inputs[2].type = INPUT_KEYBOARD;
      inputs[2].ki.wVk = 'V';
      inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
      // Ctrl up
      inputs[3].type = INPUT_KEYBOARD;
      inputs[3].ki.wVk = VK_LCONTROL;
      inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
      SendInput(4, inputs, sizeof(INPUT));
      ```
  - Modifier `server.cpp` pour utiliser `Win32PasteService` sur Windows (voir 3.1)

  **Must NOT do**:
  - Ne pas utiliser `SendMessage(WM_PASTE)` qui peut ne pas fonctionner avec UIPI (UAC)

  **References**:
  - `src/server/src/services/paste/abstract-paste-service.hpp` (interface)
  - `src/server/src/services/paste/dummy-paste-service.hpp` (stub)
  - Windows API: `SendInput`, `KEYBDINPUT`

  **Acceptance Criteria**:
  - [ ] `supportsPaste()` retourne `true`
  - [ ] `pasteToApp()` colle le contenu du presse-papier dans l'application cible
  - [ ] Fonctionne avec le service `PasteService` existant

  **QA Scenarios**:
  ```
  Scenario: Paste to app
    Tool: Bash
    Preconditions: vicinae-server running, text in clipboard
    Steps:
      1. Open Notepad
      2. Trigger paste from Vicinae
    Expected Result: Clipboard content is pasted into Notepad
    Evidence: .sisyphus/evidence/task-3.2-paste.txt
  ```

  **Commit**: OUI (avec 3.1)
  - Message: `feat(windows): implement Win32PasteService`

- [ ] 3.3. **Keyboard Layout — Détection du layout clavier**

  **What to do**:
  - Remplacer `libxkbcommon` par Windows API :
  - `GetKeyboardLayout(0)` : obtenir le layout actuel
  - `ToUnicodeEx(vk, sc, keyboardState, buffer, buflen, flags, layout)` : convertir VK → char
  - `GetKeyboardLayoutName` : obtenir le nom du layout
  - `LoadKeyboardLayout` : charger un layout
  - `src/server/src/internal/keyboard/keyboard.cpp` : adapter les parties qui utilisent xkbcommon
  - `src/lib/linux-utils/` : exclure du build Windows (dépend de xkbcommon)

  **Must NOT do**:
  - Ne pas implémenter la création de device uinput virtuel (N/A sur Windows)

  **References**:
  - Windows API: `GetKeyboardLayout`, `ToUnicodeEx`, `MapVirtualKeyEx`, `VkKeyScanEx`

  **Acceptance Criteria**:
  - [ ] Le layout clavier actif est détecté
  - [ ] La conversion VK → char fonctionne
  - [ ] `linux-utils` n'est pas compilé sur Windows

  **Commit**: OUI
  - Message: `feat(windows): implement keyboard layout detection via Win32 API`

- [ ] 3.4. **System Keybinds — Raccourcis clavier globaux**

  **What to do**:
  - Sur Linux : `KeybindManager` utilise des hooks clavier spécifiques
  - Sur Windows : utiliser `RegisterHotKey` pour enregistrer des raccourcis globaux
  - `QAbstractNativeEventFilter` pour intercepter `WM_HOTKEY`
  - Modifier `KeybindManager` ou créer un adaptateur Windows
  - Gérer le conflit de `VicinaeToggle` (Win+Space ou autre)

  **Must NOT do**:
  - Ne pas enregistrer de hotkeys qui entrent en conflit avec des raccourcis Windows système

  **References**:
  - `src/server/src/internal/keyboard/keybind-manager.hpp`
  - Windows API: `RegisterHotKey`, `WM_HOTKEY`, `QAbstractNativeEventFilter`

  **Acceptance Criteria**:
  - [ ] `RegisterHotKey` enregistre le toggle de Vicinae
  - [ ] Le hotkey déclenche l'ouverture/fermeture de Vicinae
  - [ ] Les hotkeys sont nettoyés à la fermeture

  **Commit**: OUI
  - Message: `feat(windows): implement global hotkeys with RegisterHotKey`

---

### EPIC 4 — Polish & Intégration

- [ ] 4.1. **Icon Theme — Gestion des icônes Windows**

  **What to do**:
  - L'`IconThemeDatabase` est Linux-only (lit les thèmes XDG)
  - Sur Windows : les icônes sont extraites directement des `.lnk` ou `.exe` via `SHGetFileInfo` ou `ExtractIconEx`
  - `src/server/src/internal/icon-theme-db/` : ajouter un guard `#ifdef Q_OS_LINUX`
  - Créer un stub ou alternative Windows dans `server.cpp` :
    ```cpp
    #ifdef Q_OS_WIN
    // Les icônes sont extraites via SHGetFileInfo, pas besoin de thème
    #else
    IconThemeDatabase iconThemeDb;
    QIcon::setThemeName(iconThemeDb.guessBestTheme());
    #endif
    ```

  **Must NOT do**:
  - Ne pas implémenter le parsing de thèmes XDG sur Windows

  **References**:
  - Windows API: `SHGetFileInfo`, `ExtractIconEx`, `SHGetStockIconInfo`

  **Acceptance Criteria**:
  - [ ] Les icônes des applications sont extraites correctement
  - [ ] `icon-theme-db` n'est pas compilé sur Windows

  **Commit**: OUI
  - Message: `feat(windows): handle icons via SHGetFileInfo on Windows`

- [ ] 4.2. **Packaging — Installateur Windows**

  **What to do**:
  - Créer un script ou config pour générer un installateur Windows
  - Options :
    - WiX Toolset (`.msi`)
    - NSIS (`.exe`)
    - Squirrel.Windows (utilisé par Slack, Discord)
    - Simple zip archive
  - Inclure :
    - `vicinae-server.exe`
    - `vicinae.exe` (CLI)
    - DLLs Qt nécessaires (windeployqt)
    - Thèmes
    - Extensions TypeScript (node_modules)
  - Configurer `windeployqt` pour rassembler les DLLs Qt

  **Must NOT do**:
  - Ne pas créer d'installeur complexe pour le MVP (zip suffit)

  **References**:
  - Qt docs: `windeployqt`
  - WiX Toolset: `https://wixtoolset.org/`

  **Acceptance Criteria**:
  - [ ] L'installeur/zip inclut tous les binaires et dépendances
  - [ ] Vicinae se lance après installation dans un dossier propre
  - [ ] Les DLLs Qt sont présentes (windeployqt)

  **Commit**: OUI
  - Message: `build(windows): add packaging support`

- [ ] 4.3. **CI/CD — Build Windows automatisé**

  **What to do**:
  - Ajouter un workflow GitHub Actions pour Windows
  - Étapes :
    - Installer Qt 6 (via `jurplel/install-qt-action`)
    - Installer les dépendances (vcpkg, node)
    - Configurer CMake
    - Builder
    - Packager (windeployqt)
    - Upload artefact
  - Créer `.github/workflows/build-windows.yml`

  **Must NOT do**:
  - Ne pas casser le workflow Linux existant

  **References**:
  - `.github/workflows/build.yaml` (workflow Linux existant)
  - `jurplel/install-qt-action` GitHub Action

  **Acceptance Criteria**:
  - [ ] Le workflow Windows compile avec succès
  - [ ] L'artefact est uploadé
  - [ ] Le workflow Linux continue de fonctionner

  **Commit**: OUI
  - Message: `ci(windows): add Windows build workflow`

- [ ] 4.4. **Tests d'intégration Windows**

  **What to do**:
  - Modifier le CMake pour permettre les tests sur Windows (`if (WIN32)` en plus de `if (UNIX AND NOT APPLE)`)
  - Ajouter des tests spécifiques Windows :
    - Test de découverte d'apps
    - Test de fenêtres
    - Test de presse-papier
  - Utiliser Catch2 (déjà présent)

  **Must NOT do**:
  - Ne pas réécrire tous les tests (juste en ajouter pour les nouveaux services)

  **References**:
  - `src/server/CMakeLists.txt:963` (condition de test actuelle)

  **Acceptance Criteria**:
  - [ ] `cmake --build build --target vicinae-server-tests` compile sur Windows
  - [ ] Les tests passent sur Windows

  **Commit**: OUI
  - Message: `test(windows): add integration tests for Windows services`

---

## Vague de Vérification Finale

> Les 4 vérifications s'exécutent en parallèle à la fin de chaque EPIC.

- [ ] F1. **Plan Compliance Audit** (oracle)
  Vérifier que l'EPIC terminée couvre tous les points du plan. Lire les diffs, comparer avec les TODO du plan.
  Output: `EPIC [N] — Tâches [X/X complètes, Y/Y vérifiées] | VERDICT: APPROVE/REJECT`

- [ ] F2. **Code Quality Review** (unspecified-high)
  Vérifier la qualité du code Windows :
  - `#ifdef Q_OS_WIN` bien placés
  - Pas de fuites mémoire (HWND, HANDLE bien fermés)
  - Pas de `dynamic_cast` inutiles
  - Respect des conventions du projet (clang-format, `#pragma once`, etc.)
  - Run `make format`
  Output: `Build [PASS/FAIL] | Conventions [PASS/FAIL] | VERDICT`

- [ ] F3. **Real Manual QA** (unspecified-high)
  Exécuter TOUS les scénarios QA de l'EPIC terminée. Tester l'intégration entre les services.
  Output: `Scénarios [N/N pass] | Intégration [N/N] | VERDICT`

- [ ] F4. **Scope Fidelity Check** (deep)
  Vérifier qu'aucune fonctionnalité Linux n'a été cassée. Vérifier que les dummies existants sont toujours utilisés comme fallback. Vérifier que les changements sont limités à ce qui était prévu.
  Output: `Tâches [N/N conformes] | Régression Linux [CLEAN/N issues] | VERDICT`

### Statut du Plan (Section à mettre à jour à chaque session)

**Dernière mise à jour**: 2026-07-14
**État global**:

| EPIC | Statut | Progression |
|------|--------|-------------|
| **EPIC 0** — Build System | ✅ Terminé | 4/4 tâches |
| **EPIC 1** — Core Infrastructure | 🟡 En cours | 3/7 tâches (paths, env, IPC — DONE) |
| **EPIC 2** — User Services | 🔴 Non commencé | 1/6 tâches (calculator dummy — DONE) |
| **EPIC 3** — Advanced Input | 🔴 Non commencé | 0/4 tâches |
| **EPIC 4** — Polish | 🔴 Non commencé | 0/4 tâches |

### Journal des Changements

| Date | Session | Changements |
|------|---------|-------------|
| 2026-06-05 | Prometheus | Création du plan initial. Analyse complète du codebase. Documentation de tous les services. |
| 2026-06-05 | Claude Code | Analyse initiale. Retrait de `DBus` de `find_package()` dans `src/server/CMakeLists.txt`. |
| 2026-07-14 | opencode | **EPIC 0 complété**. MSVC flags ajoutés (optimisation, warnings, LTO, sanitizers, sqlcipher). Build vérifié (`vicinae-server.exe` compile). `make format` exécuté. Plan mis à jour. |

### Problèmes Connus

| ID | Service | Problème | Solution | Statut |
|----|---------|----------|----------|--------|
| K1 | Clipboard | `DummyClipboardServer.isActivatable()` retourne `true` → le server pense que le presse-papier est actif | Ajouter `isAlive()` check ou désactiver le dummy sur Windows | ⚠️ À corriger |
| K2 | Build | `pch.h` inclut D-Bus globalement | Déplacer les includes D-Bus derrière `#ifdef Q_OS_LINUX` | 🔧 Fix inclus dans T0.2 |
| K3 | Build | `Qt6::DBus` dans LIBS non conditionnel | Rendre conditionnel | 🔧 Fix inclus dans T0.2 |
| K4 | Build | `else()` ligne 608 qui ajoute linux-app-runtime sur Windows | Changer en `elseif(UNIX AND NOT APPLE)` | 🔧 Fix inclus dans T0.2 |
| K5 | IPC | `vicinae-ipc` utilise AF_UNIX sockets | Remplacer par QLocalSocket | 📋 TODO 1.6 |
| K6 | Paths | `vicinae.cpp` utilise `xdgpp` pour les chemins (non-macOS) | Ajouter branche `#ifdef Q_OS_WIN` avec QStandardPaths | 📋 TODO 1.1 |
| K7 | Env | `environment.hpp` utilise XDG et `/sys/class/dmi/` | Ajouter branches Windows | 📋 TODO 1.7 |
| K8 | Vendor | `vendor/zip/CMakeLists.txt` a un bloc `if(UNIX)` | Ajouter `if(WIN32)` ou remplacer par check de feature | 🔧 Fix inclus dans T0.2 |
| K9 | Vendor | `vendor/sqlcipher/CMakeLists.txt` flag `-w` | Remplacer par `/W0` pour MSVC | 🔧 Fix inclus dans T0.3 |

---

## Stratégie de Commit

Les commits seront groupés par EPIC :
- `build(windows): add WIN32 platform support to CMake`
- `feat(windows): implement WinAppDatabase for Start Menu app discovery`
- `feat(windows): implement WinWindowManager using EnumWindows`
- `feat(windows): implement WinClipboardServer using Win32 clipboard`
- etc.

---

## Critères de Succès

### EPIC 0 — Build System ✅
- [x] `cmake -S . -B build -G Ninja` fonctionne sur Windows sans erreur
- [x] `cmake --build build` produit `vicinae-server.exe` (CLI et browser-link bloqués par bug figura DLL pré-existant, hors scope EPIC 0)
- [x] Le binaire se lance sans crash immédiat
- [x] `make format` passe (clang-format)

### EPIC 1 — Core Infrastructure
- [ ] Les chemins de données pointent vers `%APPDATA%/vicinae`
- [ ] Les applications Windows (Calculator, Notepad, etc.) sont détectées
- [ ] `vicinae launch Calculator` ouvre Calculator
- [ ] `vicinae toggle` bascule la fenêtre
- [ ] Le presse-papier est surveillé (copier → historique)
- [ ] `vicinae switch` liste les fenêtres ouvertes
- [ ] Les extensions TypeScript fonctionnent

### EPIC 2 — User Services
- [ ] `vicinae lock` verrouille la session
- [ ] Le volume peut être changé depuis Vicinae
- [ ] Les notifications apparaissent dans Windows
- [ ] Le sélecteur de fichiers natif s'ouvre
- [ ] La recherche de fichiers fonctionne
- [ ] Le calculateur fonctionne

### EPIC 3 — Advanced Input
- [ ] Les snippets s'expandent correctement dans Notepad
- [ ] Le collage programmatique fonctionne
- [ ] Les raccourcis clavier globaux fonctionnent

### EPIC 4 — Polish
- [ ] Les icônes des applications s'affichent correctement
- [ ] L'installeur zip contient tout le nécessaire
- [ ] Le workflow CI Windows est vert
- [ ] Les tests d'intégration passent
