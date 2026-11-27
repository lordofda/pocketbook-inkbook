# PocketBook InkBook — Komga Reader Design and Specification

## 1. Purpose and scope

This document records the design decisions, architecture, UI behavior, and bug fixes that shaped the native Komga reader for PocketBook InkPad Color 3 / B288.

The app is an experimental, AI-authored Qt Quick / C++ client. It is sideloaded as `komga-reader.app`, connects to a user-supplied Komga server, downloads whole books for offline reading, renders color pages through InkView, and syncs reading progress back to Komga.

The released line is `v1.3.0`, which includes the native InkView renderer, reader-chrome refinements, startup account persistence fix, whole-codebase review hardening, offline progress controls, queued downloads, and Sync all described below.

## 2. Product goals

- Native reading experience on PocketBook InkPad Color 3 / B288.
- Works with self-signed HTTPS Komga servers without global SSL bypass.
- Whole-book offline download and offline reading.
- Per-account settings, TLS pins, and book caches.
- Reliable progress handling when device and server disagree.
- E-ink-friendly UI: large tap targets, high contrast, minimal keyboard friction.
- Sideloadable release artifact with no device-specific secret material in source control.

## 3. Platform constraints

- Target runtime: PocketBook InkPad Color 3 / B288, ARM softfp.
- QPA platform plugin: `pocketbook2`.
- Rendering: `QSGRendererInterface::Software`.
- Qt version used for cross-build: Qt 6.8.2.
- QML imported from `/ebrmain/qml`; plugins from `/ebrmain/plugins`.
- App data is confined to `/mnt/ext1/.komga-reader/`.
- Install location is `/mnt/ext1/applications/komga-reader.app`.
- InkView APIs provide screen/font information and system status bar control.
- Device glibc is 2.39; the built binary targets no symbol newer than `GLIBC_2.34`.

## 4. Architecture

### 4.1 Composition root

`src/main.cpp` is the composition root. It:

1. Selects the PocketBook platform plugin and software renderer.
2. Opens the InkView screen and font.
3. Instantiates the core services:
   - `DeviceInfo`
   - `Settings`
   - `KomgaApi`
   - `BookStore`
   - `AppController`
4. Exposes them to QML as context properties:
   - `deviceInfo`
   - `appSettings`
   - `controller`
5. Loads `qrc:/main.qml`.

QML does not create business objects. It binds to context properties and calls `Q_INVOKABLE` methods. C++ emits `stateChanged` / `configurationChanged` and QML reacts through bindings and `Connections`.

### 4.2 Module responsibilities

| Module | Responsibility |
|---|---|
| `src/settings.cpp/.h` | Persisted INI settings, account records, active/startup account, TLS pin storage, cache paths. |
| `src/komga_api.cpp/.h` | Komga REST client, TLS fingerprint pinning, page downloads, progress updates. |
| `src/book_store.cpp/.h` | Per-account offline book cache, manifests, page files, local progress, color cache tracking. |
| `src/models.cpp/.h` | `ListModel` and typed `ListItem` roles used by all list screens. |
| `src/controller.cpp/.h` | Application state machine, navigation, request routing, downloads, sync orchestration. |
| `src/device.cpp/.h` | InkView screen/font bootstrap, status bar toggle, keyboard hiding. |
| `src/native_reader.cpp/.h` | InkView bitmap color adjustment, full/partial refreshes, native reader input and chrome. |
| `qml/main.qml` | Screen layout, navigation, dialogs, reader chrome, global input handling. |
| `qml/ActionButton.qml` | Shared large e-ink text button. |
| `qml/IconButton.qml` | Shared canvas-drawn e-ink icon button. |
| `qml/ListRow.qml` | Reusable list row with optional Open series, Download, and Delete actions. |
| `qml/AccountRow.qml` | Account row with Startup, Use, Edit, and Delete actions. |
| `qml/FilterBar.qml` | Search and A–Z filter strip with Done/keyboard dismissal. |
| `qml/ListScroller.qml` | Custom Up/Down buttons and draggable scrollbar for list views. |
| `qml/InputField.qml` | Shared labeled text input. |

### 4.3 Controller as state machine

`AppController` owns a string-valued `view`:

`setup`, `libraries`, `series`, `books`, `inProgress`, `recent`, `onDeck`, `offline`, `accounts`, `reader`.

Navigation methods normalize state before setting the view: clear transient models, clear reader-only state, reset filters when appropriate, and set status text. `setView()` emits `stateChanged`; QML shows the corresponding screen.

A `pending_` map tracks in-flight REST requests by request id. Each request carries an action name, id, title, page, manual/automatic flag, and request epoch. Responses are routed back into controller slots that validate the epoch and current view before populating models, prompting for TLS trust, handling downloads, or reconciling progress.

### 4.4 Data flow

```text
Komga REST
   |
   v
KomgaApi --> AppController --> ListModel --> QML ListView
                |    ^                         |
                |    |                         | taps
                v    |                         v
            BookStore <---- page cache <---- reader/download
                |
                +--> local progress --> debounced sync --> Komga
```

Typical browsing flow:

1. Libraries are fetched and stored in `libraryModel_`.
2. Opening a library fetches series into `seriesModel_`.
3. Opening a series fetches books into `bookModel_`.
4. Opening a book sets reader state and page source.
5. Downloads fetch the page list, store page images, and mark rows downloaded.
6. Page turns write local progress and schedule a debounced sync.

## 5. Configuration and account design

### 5.1 Storage layout

```text
/mnt/ext1/.komga-reader/
  settings.ini
  accounts/
    <account-id>/
      books/
        <hashed-book-id>/
          manifest.json
          page files
```

Qt writes general keys under `[%General]` and account keys under `[accounts/<id>]`; logical keys are:

- `general/activeAccount`
- `general/startupAccount`
- `accounts/<id>/name`
- `accounts/<id>/url`
- `accounts/<id>/apiKey`
- `accounts/<id>/fingerprint`
- `accounts/<id>/pem`
- `reader/hideStatusBar`

Old top-level `books/` data is no longer moved automatically; copy it into the target account's `books/` directory as described in CACHE_RECOVERY.md. Caches written by interim builds under `accounts/<id>/books/books/<hashed-book-id>/` remain readable, but new downloads use the single-`books` layout shown above.

### 5.2 Active vs startup account

The app distinguishes:

- **Startup account**: stored as `general/startupAccount`; when set to an existing account, it becomes the active account at launch.
- **Active account**: stored as `general/activeAccount`; the account currently loaded in the session, used at launch only when no startup account is set.

At startup, `Settings::initializeAccounts()` creates a first account only on a fresh install, then applies a valid startup account over the stored active account. If startup has been cleared, the last active account is preserved. Legacy keys are ignored and left untouched; there is no migration or fallback cascade.

`Settings::switchAccount()` and the **Use** action change only the active account; **Edit** also opens the selected account without changing startup. `AppController::setStartupAccount()` from the **Startup** action writes only `general/startupAccount`; `AppController::clearStartupAccount()` from **Clear startup** removes it. Deleting the startup account clears startup; deleting the active account selects the startup account when available, otherwise the first remaining account.

The account editor shows active and startup account names. The setup form offers **Set as startup** when the active account is not already the startup account. The Accounts screen also offers **Clear startup** near **New account**.

### 5.3 Security decisions

- API keys are entered at runtime and stored per account in plaintext so the app can reconnect after restart. The device must be protected; dedicated Komga API keys are recommended.
- TLS uses first-connection fingerprint pinning. The fingerprint is shown for verification before trust and must match the trusted PEM certificate.
- Remote plaintext HTTP is rejected; the API key is only sent over HTTPS or loopback HTTP.
- No global SSL verification bypass is used.
- The initial untrusted certificate probe does not send the API key.
- No secrets are committed to the repository.

## 6. UI specification

### 6.1 Global header and footer

The top header contains large buttons: **Home**, **Back**, **Refresh**, **Accounts**, status bar toggle, and **Exit**. The reader hides most chrome until the middle third of the page is tapped.

The status line shows `Working · <status>` while busy and `<status>` otherwise.

### 6.2 Screens

| Screen | Content |
|---|---|
| Setup | Account name, startup-account control, HTTPS URL, API key, certificate trust flow. |
| Accounts | List of accounts with Startup, Use, Edit, Delete; startup and active indicators; scroll controls. |
| Libraries | Komga libraries; top-bar shortcuts for In progress, Offline, Recently added, and On deck. |
| Series | Series in the selected library, filtered by `FilterBar`, with scroll controls. |
| Books | Books in a series, sorted numerically; row actions Open series and Download. |
| In progress | Books ordered by `readProgress.readDate`, descending. |
| Recent | Recently added series from `/api/v1/series/new`. |
| On deck | On-deck books from `/api/v1/books/ondeck`. |
| Offline | Downloaded books with Sync, Open series (when cached), Delete, cached-size summary, and a Sync all action above the filter row. |
| Reader | InkView-rendered page, native tap zones/bars, mirrored QML fallback controls, completion banner. |

### 6.2.1 Top bar and reader chrome

The global top bar keeps Home, Back, Refresh, Accounts, Bar, and Exit as text. Library shortcuts for In progress, Offline, Recently added, and On deck live in a second row that is visible on every non-reader screen; the reader fallback uses the compact single-row header. The Offline page adds a Sync all action between the header and the filter row.

In `v1.2.3`, top-bar `Bar:off` / `Bar:on` toggled the InkView system status bar; reader chrome was toggled by tapping the middle third. The native reader bottom bar has no Bar control; status-bar changes are made from the QML/library top bar before entering the native reader. Reader chrome remains toggled by the center third, top-bar tap, or OK/Menu key. The native Download slot and the QML fallback down-arrow become Cancel while a download queue is active. The QML fallback bottom bar keeps left/right arrows, bold down-arrow Download, Sync, Bar, Exit, and Back; its `Bar` toggles reader chrome because the fallback has no center-tap chrome toggle. Series and Delete were removed from reading mode but remain available outside it.

### 6.3 List rows

`ListRow.qml` exposes optional actions:

- **Open series** on book rows, including in-progress, on-deck, and offline rows with a cached series ID.
- **Download** on non-downloaded book rows; a new press queues the book if another download is active.
- **Delete** on offline rows.
- **SYNC** on offline rows with pending progress; tapping the badge syncs that book without opening it.

The row's tap area is limited to the left/text portion. Action buttons live in a right-hand row with higher z-order. This prevents row taps from accidentally triggering navigation when the user taps an action button.

### 6.4 Account row

`AccountRow.qml` shows name, server URL, and status:

- `ACTIVE`
- `STARTUP` if not active but configured as the startup account
- `READY`
- `SETUP NEEDED`

Actions are **Startup**, **Use**, **Edit**, and **Delete**. Startup persists the launch account; Use and Edit change only the active account. Editing always opens the setup form, even for a fully configured account.

### 6.5 Delete confirmation

Deleting an account opens a modal confirmation dialog:

- Title: `Delete account?`
- Message includes the account name and explains that connection settings will be removed.
- Buttons: **Delete** and **Cancel**.
- Tapping the dimmed background cancels.
- The cached book directory is not deleted automatically.

### 6.6 Scrolling

`ListScroller.qml` provides both:

- **Up** / **Down** buttons that move relative to the first visible row.
- A draggable scrollbar on the right side of account and browsing lists.

A custom QML implementation was chosen over `QtQuick.Controls.ScrollBar` to avoid adding a Controls dependency on the device and to keep tap targets explicit.

### 6.7 Filter and keyboard handling

`FilterBar.qml` has a text field, A–Z buttons, and a **Done** button. Keyboard dismissal is centralized in `root.dismissKeyboard()`, called on:

- Filter Done.
- A–Z selection.
- Enter/Escape/back.
- Navigation button taps.
- View changes.
- Row taps and row action taps.

`DeviceInfo::hideKeyboard()` calls `QGuiApplication::inputMethod()->hide()`.

### 6.8 Reader behavior

- Left third: previous page.
- Middle third: toggle native InkView bars.
- Right third: next page.
- Native bars show title, page position, completion state, a sync status line, left/right arrows, bold down-arrow Download, Sync, Exit, and Back controls.
- Finishing a book marks it complete in Komga and appends `Finished` to the native header.
- The QML next-book banner remains only as a fallback when native rendering fails.

## 7. API and sync behavior

### 7.1 Endpoints

Used Komga endpoints include:

- `/api/v1/libraries`
- `/api/v1/series`
- `/api/v1/series/new`
- `/api/v1/books/ondeck`
- `/api/v1/books/list?unpaged=true&sort=metadata.numberSort%2Casc`
- `/api/v1/books/{id}`
- `/api/v1/books/{id}/pages`
- `PATCH /api/v1/books/{id}/read-progress`
- `/api/v1/books/{id}/pages/{page}`

Authenticated requests use `X-API-Key` over HTTPS or loopback HTTP only.

### 7.2 Series sorting

Book ordering failed with titles such as `Chapter 14`, `Chapter 17`, `Chapter 2` when relying on string order. The fix has two layers:

1. Server-side sort uses `metadata.numberSort,asc`.
2. Client-side stable sort uses `metadata.numberSort`, then digits parsed from `metadata.number`, then top-level `number`.

In-progress books remain sorted by read date rather than book number.

### 7.3 Progress reconciliation

Before pushing local progress, the app fetches the book from Komga and re-reads the current local page:

- If the server page is ahead, local state jumps to the server page.
- Otherwise local progress is pushed.
- Completion pushes `page` and `completed:true`.
- Automatic sync runs for online books from the reader; leaving the reader starts an immediate sync for pending progress and does not cancel an in-flight check or write.
- Manual sync works from the reader, including offline-opened copies, and from the offline `SYNC` badge.
- Sync all on the Offline page snapshots books with pending progress and reconciles them one at a time; failures keep their badge and are reported in the final summary.
- Failed writes retry with bounded backoff even after leaving the reader; overlapping sync checks are ignored.

Progress writes are local, then debounced to the server. This protects against overwriting newer server progress after offline reading.

### 7.4 Download/progress boundary

Downloads and reading progress are separate operations. A row **Download** must not mark a book as in progress.

The cleanup branch separates download state from reader state. Row downloads no longer change `currentBookId_`, `currentPage_`, or progress. Page updates run for the active reader book; progress sync may continue in the background after the reader closes and may be started explicitly from offline rows.

## 8. Offline storage design

- Downloads are whole-book only and queued per book; the active book's pages download sequentially.
- `createManifest()` writes page numbers, media types, and the cached series ID.
- Pages are downloaded sequentially into a `.raw` temporary file, then stored unmodified in the per-account book folder.
- Starting another download appends a job; Cancel clears the active page request, pending page-list requests, and queued jobs.
- Manifest records include a color-cache flag; grayscale caches from older builds prompt for a fresh download.
- `BookStore` tracks local page, pending progress, downloaded count, cached size, and color-cache mode.
- Offline books can be opened, synced manually, and deleted without a server connection; Open series appears when the cached manifest has a series ID.
- Deleting a downloaded book removes local page files; deleting an account removes only its settings, leaving the cache directory for manual recovery.

## 9. Bug solutions and design decisions

| Issue | Decision / fix |
|---|---|
| Blank first row in early QML build | Removed problematic resource pruning/pragma behavior; kept QML resources explicit in `komga.qrc`. |
| Self-signed HTTPS | First-connection SHA-256 fingerprint pinning with explicit user confirmation; no global bypass. |
| Offline cache migration | Added account-scoped cache paths and legacy migration to `accounts/default/books`. |
| Server-ahead progress overwrite | Fetch server book before pushing local progress; adopt server page when ahead. |
| Keyboard covered UI | Done button, Enter/Escape, navigation/row/page taps, and centralized `dismissKeyboard()`. |
| Missing next-book recommendation | Added completion detection, numeric next-book lookup, and completion banner. |
| No series shortcut | Added `Open series` on rows and in reader using `seriesId` model role. |
| `Chapter 14` / `Chapter 2` ordering | Server sort plus local numeric `bookSortNumber()`. |
| Edit button appeared to switch account | Two causes: row MouseArea overlapped buttons, and configured accounts reset to Libraries; narrowed row tap area and forced `resetForAccount(true)` into setup. |
| No account delete safeguard | Added modal delete confirmation. |
| No list scrolling affordance | Added custom `ListScroller` with Up/Down and a draggable bar. |
| Up/Down buttons jumped incorrectly | Buttons now use the first visible row via `indexAt()` and `positionViewAtIndex(..., ListView.Beginning)`. |
| Download reset scroll position | Replaced full model rebuilds with in-place `markDownloaded()` data changes during and after download. |
| Download marked book in progress | Row downloads keep separate target/title state; reader updates only for the active online book. |
| Avoid force-pushing released tags | Used new incremental tags `v1.2.1` and `v1.2.2` instead of moving `v1.2.0`. |
| Washed-out color on Kaleido 3 | Native reader loads RGB24 bitmaps, calls `adjustBitmapDefault()`, and refreshes through InkView; new caches store color bytes and record a color-cache flag. |
| Startup account could not be changed reliably | Startup and active account state are now separate keys; only **Startup** writes `general/startupAccount`, while **Use** and **Edit** update only the active account. |
| Startup account lost priority to last active | A valid `startupAccount` now becomes active at launch; **Clear startup** removes it and preserves the last active account. Legacy keys and repair cascades were removed. |
| Incomplete cache reset progress to page 1 on download | Opening a book now restores stored manifest progress regardless of full-download state, falls back to manifest page count when the list omits it, and keeps a server-reported page from collapsing to 1. |
| Reader navigation icons | Previous/Next are left/right arrows, Download is a bold down arrow, Series and Delete were removed from reading mode, and Exit was added. The Bar control was removed from the native reader bar. |
| Top bar text and shortcuts | Home, Refresh, and Accounts reverted to text because the icons were not well liked; In progress/Offline/Recently added/On deck moved into a second row. |
| Fallback reader controls | The QML bottom controls use the native actions plus a Bar action for reader chrome; the top fallback keeps its existing text controls and shows library shortcuts on every non-reader screen. |
| Reader icon proportions | Previous/Next arrows are larger centered chevrons, and Download uses a compact square-proportioned arrow in both native and fallback reader bars. |
| Native bar toggle reliability | Both direct and delegated status-bar toggles destabilized the InkView panel and native chrome, so the native Bar control was removed; use the QML/library top bar instead. |

## 10. Model update strategy

`ListModel::setItems()` uses `beginResetModel()` / `endResetModel()`. This is correct for loading a new list but resets `ListView` scroll position.

To preserve scroll position during downloads, `ListModel::markDownloaded(id, downloaded)` was added. It:

1. Finds all visible rows for the book id.
2. Updates `downloaded` and the `OFFLINE` badge in place.
3. Emits `dataChanged()` for `DownloadedRole` and `BadgeRole`.
4. Leaves `visible_` unchanged because filters do not depend on download state.

`ListModel::markSyncPending(id, pending)` uses the same in-place approach for the Offline `SYNC` badge and `sync pending` meta suffix while Sync all advances through books.

This avoids disruptive model resets while still refreshing row badges.

## 11. Build and release process

Build:

```bash
./build.sh
sha256sum dist/komga-reader.app
```

Validation:

```bash
/usr/lib/qt6/bin/qmllint --unqualified disable -W 0 \
  qml/main.qml qml/ActionButton.qml qml/IconButton.qml qml/ListRow.qml \
  qml/AccountRow.qml qml/InputField.qml qml/FilterBar.qml \
  qml/ListScroller.qml
```

Host unit tests use a separate desktop-Qt CMake project and never touch the ARM build directory:

```bash
cmake -S tests/host -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

Releases:

- GitHub rejects raw `.app` assets, so release assets are zipped as `komga-reader-vX.Y.Z.zip`.
- Install instructions tell the user to unzip and copy `komga-reader.app` to `/mnt/ext1/applications/`.
- New tags should be annotated and pushed separately; `v1.2.3` is an existing lightweight-tag exception.
- The PAT is used transiently for pushes and API calls; it is never written into git config or source.

### 11.1 Release history

| Version | Commit | Summary |
|---|---|---|
| `v1.0.0` | earlier build | Initial tested native offline reader. |
| `v1.1.0` | `c1a8469` | Home, search/filter, Recent, On Deck, account manager improvements. |
| `v1.2.0` | `b9e4908` | Default accounts, delete confirmation, row downloads, next-book/Open series, numeric sorting, scroll controls, keyboard dismissal. |
| `v1.2.1` | `85c8542` | Edit Account opens setup form for configured accounts. |
| `v1.2.2` | `d3099b8` | Scroller buttons and download scroll-position fixes. |
| `v1.2.3` | `20b6db2` | Color preprocessing, HQ refresh, account/default fixes, progress-push fix. |
| `v1.2.4` | `ee95897` | Native InkView color rendering; published as a pre-release. |
| `v1.2.5` | release commit | Reader-chrome refinements, startup account persistence, and whole-codebase review hardening. |
| `v1.3.0` | release commit | Native/offline sync controls, clickable offline SYNC badge, cached-series navigation, queued downloads with Cancel, and Sync all for pending progress. |

`v1.2.0` has a tag but no public GitHub release page. `v1.2.1`, `v1.2.2`, and `v1.2.3` have stable release pages with zipped binaries. `v1.2.4` remains a pre-release page; `v1.3.0` is the current stable release.

## 12. Branching state at time of writing

- `main`: released line at `v1.3.0`, pushed with all review-branch and feature-branch work merged.
- `feature/reader-improvements`: completed queue and Sync all branch; merged into `main`.
- `test/inkview-reader`: superseded InkView prototype branch; removed locally.
- `cleanup/code-review`: former `v1.2.4` cleanup branch; removed locally.
- `review/codebase-hardening`: completed review branch; removed locally after fast-forwarding `main`.

## 13. Known limitations

- API keys remain plaintext on the device by design.
- Deleting an account leaves its cache directory on disk.
- Downloads are whole-book only and sequential within a queued job.
- The custom scrollbar approximates position rather than using platform-native Controls.
- Large libraries still depend on network speed and server response time.
- Status bar behavior depends on InkView firmware support.
- Host unit tests cover settings/account logic; InkView behavior still relies on ARM build success and manual device testing.

## 14. Verification checklist

Before release:

1. `qmllint` passes for every QML file in `qml/komga.qrc`.
2. Host CMake tests pass via `ctest --test-dir build-host --output-on-failure`.
3. `./build.sh` succeeds and produces a 32-bit ARM `komga-reader.app`.
4. `git diff --check` passes.
5. Binary SHA-256 is recorded in release notes.
6. Release asset is a zip containing only `komga-reader.app`.
7. Tag points at the intended commit.
8. No secrets are present in the working tree or commit history.

Device acceptance checks:

- Setup and TLS trust flow.
- Account add, edit, switch, set/clear startup, delete with confirmation.
- Libraries, series, books, Recent, On Deck, In progress, and Offline browsing.
- Search/filter with keyboard dismissal.
- Numeric series ordering.
- Open series and Open next.
- Whole-book download and offline read.
- Queue two downloads, verify the first finishes before the second, then cancel mid-queue.
- Sync all with several pending books, including one failure.
- Scrollbar and Up/Down behavior.
- Download does not reset scroll position.
- Download does not create reading progress.
- Progress sync with server-ahead protection.
- Reader controls and status bar toggle.
