# PocketBook InkBook — Komga reader

> **This is a work of AI.** The code, UI, and build setup in this repository were generated and implemented with AI assistance. Treat it as experimental software and review it before relying on it.

A native Qt Quick / C++ Komga reader for PocketBook InkPad Color 3 (B288). It downloads whole books for offline reading, renders color pages through InkView, and syncs reading progress back to Komga.

**Tested on device:** PocketBook InkPad Color 3 (B288), including setup, downloads, offline reading, account switching, progress sync, and reader controls.

## Features

- Native PocketBook app, sideloaded as `komga-reader.app`.
- Komga HTTPS connection with runtime-entered API key.
- First-connection TLS certificate fingerprint pinning; no global SSL bypass.
- Multiple Komga accounts: add, edit, switch, and delete from the **Accounts** top menu; each account has its own URL, API key, TLS pin, and book cache. **Save** stores edited details, **Connect** saves and tests them, **Use** changes the active account, and **Startup** chooses the account opened at launch.
- Library, series, and books browsing, with In progress, Offline, Recently added, and On deck shortcuts in the top bar.
- Recently added series and on-deck books, using Komga's `/api/v1/series/new` and `/api/v1/books/ondeck` views.
- Search and A–Z title filtering on every list view, with a Done button to dismiss the on-screen keyboard.
- Finishing a book marks it complete in Komga and offers the next book in the series when available.
- Open series action on book rows for downloading more of a series.
- Whole-book offline downloads with native InkView color rendering.
- Offline books browser: open downloaded books without a server, delete local copies, and see cached size/count.
- Progress sync:
  - Automatic debounced sync after page turns.
  - Explicit page turns push the local page to Komga, so the server cannot pull the reader back to a newer page immediately.
  - Manual `Sync` button in the reader.
  - When opening a book, the app checks Komga's stored progress. If the server is further ahead, the local position moves to the server page; otherwise the local page is pushed.
- Native InkView reader: left/right thirds turn pages; a center tap toggles native bars with left/right arrows, a bold down-arrow download action, Sync, Exit, and Back.
- Top bar keeps Home, Back, Refresh, Accounts, Bar, and Exit as text; In progress, Offline, Recently added, and On deck appear in a second row on non-reader screens.
- `Bar:off` / `Bar:on` in the library top bar toggles the system status bar. The QML fallback reader's `Bar` button toggles reader chrome; the native InkView reader toggles chrome with a center tap, top-bar tap, or OK/Menu key and has no status-bar button. Default status bar state is hidden.

## Install

1. Download and unzip `komga-reader-vX.Y.Z.zip` from the GitHub Releases page.
2. Copy it to `/mnt/ext1/applications/` on the PocketBook.
3. Disconnect/Eject the device and launch **Komga Reader** from Applications.
4. Enter the Komga HTTPS URL and API key, then verify and trust the displayed SHA-256 certificate fingerprint.

The app stores settings and cached books only under `/mnt/ext1/.komga-reader/`. Accounts live in `settings.ini`; `startupAccount` is authoritative at launch when set, and `activeAccount` is used when startup is cleared. Book caches are isolated under `/mnt/ext1/.komga-reader/accounts/<account-id>/books/<hashed-book-id>/`. The older temporary `books/books/<hashed-book-id>/` layout is still recognized. Changing or clearing a configured server URL/API key clears that account's cache.
If cached books do not appear after upgrading, see [CACHE_RECOVERY.md](CACHE_RECOVERY.md). Books cached by older grayscale builds must be downloaded again for vivid color.

## Security notes

- API keys are stored in plaintext per account in `/mnt/ext1/.komga-reader/settings.ini` so the app can reconnect offline/after restart. Use dedicated Komga API keys and protect the device.
- The TLS certificate fingerprint is pinned on first trusted connection. If the server certificate changes, the app will refuse to connect until the stored pin is cleared or updated.
- No API key is sent during the initial untrusted certificate probe.
- Remote plaintext HTTP is rejected; API keys are sent only over HTTPS or loopback HTTP.

## Build

The target binary was cross-built for ARM softfp with Qt 6.8.2 using the `ghcr.io/fstanis/pocketbook-sdk-qt6-builder` image and PocketBook SDK headers.

```bash
./build.sh
sha256sum dist/komga-reader.app
```

The script configures CMake with `$POCKETBOOK_SDK_CMAKE` if set, otherwise `$SDK/cmake/pocketbook.toolchain.cmake` (default SDK: `/opt/pocketbook-build/pocketbook-sdk-qt6`), then copies the linked binary to `dist/komga-reader.app`.

Verified on device with:

- PocketBook InkPad Color 3 / B288.
- Device glibc 2.39; binary requires at most `GLIBC_2.34`.
- Qt 6 device libraries from `/ebrmain/lib` and QML modules from `/ebrmain/qml`.

QML validation:

```bash
/usr/lib/qt6/bin/qmllint --unqualified disable -W 0 \
  qml/main.qml qml/ActionButton.qml qml/IconButton.qml qml/ListRow.qml \
  qml/InputField.qml qml/AccountRow.qml qml/FilterBar.qml qml/ListScroller.qml
```

Host unit tests use desktop Qt and temporary settings directories:

```bash
cmake -S tests/host -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

## Project layout

- `qml/main.qml` — main UI, navigation, reader chrome.
- `qml/ListRow.qml` — reusable list row with optional offline delete button.
- `qml/AccountRow.qml` — account row with startup/use/edit/delete actions.
- `src/controller.cpp` — application state, navigation, downloads, sync logic.
- `src/komga_api.cpp` — Komga REST client, TLS pinning, downloads.
- `src/book_store.cpp` — offline cache, manifests, color cache tracking.
- `src/native_reader.cpp` — InkView bitmap rendering and reader input.
- `src/settings.cpp` — persisted settings.
- `src/device.cpp` — InkView screen/font/bootstrap and system status bar control.
- `tests/host/` — desktop Qt unit tests for portable settings/account logic.
- `dist/komga-reader.app` — compiled sideloadable binary.

## Limitations

- First offline scope is whole-book downloads only.
- Large libraries download sequentially and can feel slow.
- The `In progress` sort uses Komga's `readProgress.readDate`.
- Deleting an account removes its settings; its cached book directory is left on disk and can be removed manually if needed.
- System status bar behavior depends on the InkView firmware API; the toggle is available in the top menu on supported devices.

## Disclaimer

Provided as-is, without warranty. This is an AI-authored experimental project, not an official PocketBook or Komga application.
