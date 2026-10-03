# sync-vault
I use this to sync my encrypted vault backups.
Currently using Google Drive as the storage provider, but I plan to add support for others.

## Overview
`sync-vault` handles the full lifecycle of your encrypted vault backups:

**Upload flow:**

1. Compress and encrypt the local vault folder
2. Upload the encrypted archive to Google Drive
3. Clean up the temporary archive

**Restore flow:**

1. Download the chosen backup from Google Drive
2. Decrypt and extract it back into the local vault
3. Clean up the temporary archive

Encryption is handled by [crip-crypt](https://github.com/elenndev/crip-crypt).

## Debug mode
Any command can be run with the `--debug` flag to get extra logs about what the tool is doing:

```bash
sync-vault <command> --debug
```

When enabled, sync-vault prints detailed messages about each step being executed, which is useful for troubleshooting or understanding the internal flow.

## Dependencies
**System libraries:**

```bash
sudo apt update
sudo apt install build-essential \
                 libcurl4-openssl-dev \
                 libmicrohttpd-dev \
                 libjson-c-dev \
                 bear \
                 jq
```

**crip-crypt:**

This tool depends on [crip-crypt](https://github.com/elenndev/crip-crypt) for compressing, encrypting, and decrypting the vault before upload/download.

```bash
cargo install --git https://github.com/elenndev/crip-crypt.git
cargo uninstall crip-crypt
```

## Development
When developing `sync-vault`, use [Bear](https://github.com/rizsotto/Bear) to generate `compile_commands.json` for `clangd`.

If the project uses `make`, run:

```bash
bear -- make
```

## Install
Build and install the binary to `/usr/local/bin`:

```bash
sudo make install
```

After that, `sync-vault` is available from any directory:

```bash
sync-vault
```

**Installing without `sudo`:**

To install into `~/.local/bin` instead (make sure it's in your `PATH`):

```bash
make install PREFIX=$HOME/.local
```

If `~/.local/bin` is not in your `PATH`, add this to your `~/.bashrc`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Then reload it:

```bash
source ~/.bashrc
```

## Uninstall
Remove the installed binary:

```bash
sudo make uninstall
```

If you installed with a custom `PREFIX`, pass it along:

```bash
make uninstall PREFIX=$HOME/.local
```
