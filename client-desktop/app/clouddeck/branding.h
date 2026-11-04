// CloudDeck-Client branding. Kept in one header so upstream merges only touch the
// few call sites that include it (see docs/adr/0005-branding-and-identity.md).
#pragma once

// QSettings / config-dir namespace. Must differ from stock Moonlight so both can
// be installed side by side without sharing hosts, pairing certs or settings.
#define CDC_ORG_NAME    "CloudDeck-Client"
#define CDC_ORG_DOMAIN  "nyyks.github.io"
#define CDC_APP_NAME    "CloudDeck Client"

// Reverse-DNS application ID (Linux desktop file, Wayland app_id, X11 WM_CLASS).
#define CDC_APP_ID      "io.github.nyyks.CloudDeckClient"
