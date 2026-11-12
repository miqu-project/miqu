#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

if [ -z "${JAVA_HOME:-}" ] || [ ! -x "${JAVA_HOME}/bin/java" ]; then
    DEFAULT_JBR="$HOME/.local/programs/android-studio/jbr"
    if [ -x "$DEFAULT_JBR/bin/java" ]; then
        export JAVA_HOME="$DEFAULT_JBR"
    elif command -v java >/dev/null 2>&1; then
        export JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")"
    else
        echo "Error: Java not found. Please set JAVA_HOME or install Android Studio JBR at $DEFAULT_JBR" >&2
        exit 1
    fi
fi

export PATH="$JAVA_HOME/bin:$PATH"

echo "Using Java at: $(command -v java)"
echo "Building and installing..."
./gradlew installDebug "$@"
