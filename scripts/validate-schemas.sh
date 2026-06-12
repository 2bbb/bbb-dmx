#!/usr/bin/env bash
set -euo pipefail

AJV="${AJV:-npx --yes ajv-cli@5.0.0}"

for schema in schemas/*.schema.json; do
    ${AJV} compile --spec=draft2020 --strict=true -s "${schema}"
done
