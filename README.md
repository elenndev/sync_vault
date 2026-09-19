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

## Installation

**Install**
```bash
make install
```
or
```bash
make install PREFIX=$HOME/.local
```

**Uninstall**
```bash
make uninstall
```
---

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
