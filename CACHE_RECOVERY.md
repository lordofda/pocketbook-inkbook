# Recovering downloaded books after the account-manager upgrade

The account-manager release changed the offline cache layout.

Old layout (before accounts):

```text
/mnt/ext1/.komga-reader/books/<hashed-book-id>/manifest.json
```

New layout (per account):

```text
/mnt/ext1/.komga-reader/accounts/<account-id>/books/<hashed-book-id>/manifest.json
```

On first launch the app moves the old `books/` folder into `accounts/default/books/` only when that target does not already exist. If migration was skipped, your old downloads are still on disk but are not shown by the active account's **Offline** browser. The app only deletes cached books when you explicitly press **Delete** or change/clear a configured server URL or API key. Caches written by interim builds under `accounts/<id>/books/books/<hashed-book-id>/` are still recognized.

## Find the files

Connect the PocketBook by USB or use a file manager with hidden files enabled, then look under:

```text
/mnt/ext1/.komga-reader/
```

Check these directories:

```text
/mnt/ext1/.komga-reader/books/
/mnt/ext1/.komga-reader/accounts/default/books/
/mnt/ext1/.komga-reader/accounts/<other-account-id>/books/
```

To map an account name to its `<account-id>`, open:

```text
/mnt/ext1/.komga-reader/settings.ini
```

Qt writes account sections as `[accounts/<account-id>]`. Each section contains `name=...`, `url=...`, `apiKey=...`, `fingerprint=...` (the TLS pin), and `pem=...`. General values are written under `[%General]`: `startupAccount=...` is the account opened at launch, and `activeAccount=...` is the currently selected account.

## Recover manually

1. Decide which account should own the old books.
2. Copy the book folders (each one containing `manifest.json` and page image files) from the old location into that account's `books/` directory, for example:

   ```text
   /mnt/ext1/.komga-reader/accounts/default/books/
   ```

3. Keep each hashed book folder intact; do not split `manifest.json` away from its page files.
4. If both `/mnt/ext1/.komga-reader/books/` and `/mnt/ext1/.komga-reader/accounts/default/books/` exist, compare them and keep the more complete/newer copy.
5. Launch the app, open **Accounts**, press **Use** on the target account, then open **Offline**. The recovered books should be listed.

If you have shell access on the device, the equivalent is:

```sh
mkdir -p /mnt/ext1/.komga-reader/accounts/<account-id>/books
cp -a /mnt/ext1/.komga-reader/books/. /mnt/ext1/.komga-reader/accounts/<account-id>/books/
```

Replace `<account-id>` with the ID from `settings.ini`.

## Clean up the duplicate

Only after verifying the books appear in the app, remove the old directory to free space:

```sh
rm -rf /mnt/ext1/.komga-reader/books
```

You can also delete it from a PC file manager. Do not delete `/mnt/ext1/.komga-reader/accounts/` unless you also want to discard all account settings and caches.

## Note about progress

The copied `manifest.json` files contain local page progress, any pending sync flag, and (in newer caches) a series ID. If a manifest says `pending: true`, opening that book online starts a sync; leaving the reader starts an immediate sync for pending progress, and failed writes retry with bounded backoff after the reader closes. The offline `SYNC` badge and reader Sync button also push pending progress manually. There is no startup-wide pending-progress scan, automatic sync still skips offline-opened books, and the existing server-ahead protection still applies.
