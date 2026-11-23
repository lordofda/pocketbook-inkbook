# PocketBook InkBook — Komga reader

> **This is a work of AI.** The code, UI, and build setup in this repository were generated and implemented with AI assistance. Treat it as experimental software and review it before relying on it.

A native Qt Quick / C++ Komga reader for PocketBook InkPad Color 3 (B288). It downloads whole books for offline reading, renders pages locally in grayscale, and syncs reading progress back to Komga.

**Tested on device:** PocketBook InkPad Color 3 (B288), including setup, downloads, offline reading, account switching, progress sync, and reader controls.

## Features

- Native PocketBook app, sideloaded as `komga-reader.app`.
- Komga HTTPS connection with runtime-entered API key.
- First-connection TLS certificate fingerprint pinning; no global SSL bypass.
- Multiple Komga accounts: add, edit, switch, and delete from the **Accounts** top menu; each account has its own URL, API key, TLS pin, and book cache.
- Library, series, books, and in-progress browsing from the Home sections.
- Recently added series and on-deck books, using Komga's `/api/v1/series/new` and `/api/v1/books/ondeck` views.
- Search and A–Z title filtering on every list view.
- Whole-book offline downloads with local grayscale conversion.
- Offline books browser: open downloaded books without a server, delete local copies, and see cached size/count.
- Progress sync:
  - Automatic debounced sync after page turns.
  - Manual `Sync` button in the reader.
  - Before syncing, the app checks Komga's stored progress. If the server is further ahead, the local position moves to the server page; otherwise the local page is pushed.
- Immersive reader: header, footer, progress bar, and buttons are hidden until the middle third of the page is tapped. Left/right thirds turn pages.
- InkView system status bar can be hidden/shown from the top menu (`Bar:off` / `Bar:on`). Default is hidden.

## Install

1. Download `komga-reader.app` from the GitHub Releases page.
2. Copy it to `/mnt/ext1/applications/` on the PocketBook.
3. Disconnect/Eject the device and launch **Komga Reader** from Applications.
4. Enter the Komga HTTPS URL and API key, then verify and trust the displayed SHA-256 certificate fingerprint.

The app stores settings and cached books only under `/mnt/ext1/.komga-reader/`. Accounts live in `settings.ini`; book caches are isolated under `/mnt/ext1/.komga-reader/accounts/<account-id>/books/`. Existing single-account installs are migrated to a `default` account on first launch. If cached books do not appear after upgrading, see [CACHE_RECOVERY.md](CACHE_RECOVERY.md).

## Security notes

- API keys are stored in plaintext per account in `/mnt/ext1/.komga-reader/settings.ini` so the app can reconnect offline/after restart. Use dedicated Komga API keys and protect the device.
- The TLS certificate fingerprint is pinned on first trusted connection. If the server certificate changes, the app will refuse to connect until the stored pin is cleared or updated.
- No API key is sent during the initial untrusted certificate probe.

## Build

The target binary was cross-built for ARM softfp with Qt 6.8.2 using the `ghcr.io/fstanis/pocketbook-sdk-qt6-builder` image and PocketBook SDK headers.

```bash
./build.sh
sha256sum dist/komga-reader.app
```

The script configures CMake with `cmake/pocketbook.toolchain.cmake` and copies the linked binary to `dist/komga-reader.app`.

Verified on device with:

- PocketBook InkPad Color 3 / B288.
- Device glibc 2.39; binary requires at most `GLIBC_2.34`.
- Qt 6 device libraries from `/ebrmain/lib` and QML modules from `/ebrmain/qml`.

QML validation:

```bash
/usr/lib/qt6/bin/qmllint --unqualified disable -W 0 \
  qml/main.qml qml/ActionButton.qml qml/ListRow.qml qml/InputField.qml
```

## Project layout

- `qml/main.qml` — main UI, navigation, reader chrome.
- `qml/ListRow.qml` — reusable list row with optional offline delete button.
- `qml/AccountRow.qml` — account row with use/edit/delete actions.
- `src/controller.cpp` — application state, navigation, downloads, sync logic.
- `src/komga_api.cpp` — Komga REST client, TLS pinning, downloads.
- `src/book_store.cpp` — offline cache, manifests, grayscale conversion.
- `src/settings.cpp` — persisted settings.
- `src/device.cpp` — InkView screen/font/bootstrap and system status bar control.
- `dist/komga-reader.app` — compiled sideloadable binary.

## Limitations

- First offline scope is whole-book downloads only.
- Large libraries download sequentially and can feel slow.
- The `In progress` sort uses Komga's `readProgress.readDate`.
- Deleting an account removes its settings; its cached book directory is left on disk and can be removed manually if needed.
- System status bar behavior depends on the InkView firmware API; the toggle is available in the top menu on supported devices.

## Disclaimer

Provided as-is, without warranty. This is an AI-authored experimental project, not an official PocketBook or Komga application.
