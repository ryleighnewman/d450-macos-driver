# Security

Please report security problems privately with "Report a vulnerability" on the Security tab, not in a public issue.

The installer changes one file, `/usr/libexec/cups/filter/rastertolabeltspl`, and keeps the original next to it as `rastertolabeltspl.intel`. The uninstaller puts the original back. The full source is in `rastertolabeltspl.c`, and `make` rebuilds the binary from it.
