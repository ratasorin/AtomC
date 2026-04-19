#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
	echo "Usage: $0 <input_file>"
	exit 1
fi

INPUT_FILE="$1"
if [[ ! -f "$INPUT_FILE" ]]; then
	echo "Error: input file not found: $INPUT_FILE"
	exit 1
fi

OUTPUT_BIN="./main.out"

gcc -std=c11 -Wall -Wextra -o "$OUTPUT_BIN" \
	main.c lexer.c utils.c parser.c

"$OUTPUT_BIN" "$INPUT_FILE"
