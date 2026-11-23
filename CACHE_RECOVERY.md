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

On first launch the app tries to move the old `books/` folder into `accounts/default/books/`. If you created or switched to another account before that happened, your old downloads are still on disk but are not shown by the active account's **Offline** browser. The app only deletes cached books when you explicitly press **Delete**.

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

Look for `[accounts]` groups. Each group name is the account ID and contains `name=...`, `url=...`, and the TLS pin. `[general] activeAccount=...` tells you which account is currently selected.

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

The copied `manifest.json` files contain local page progress and any pending sync flag. If a manifest says `pending: true`, the next automatic or manual sync will push that page to Komga, with the existing server-ahead protection still applied.
