#!/usr/bin/env bash
# Backward compatibility wrapper for install.sh
exec "$(dirname "$0")/install.sh" "$@"
