#!/usr/bin/env bash
# Encode UI sounds to 16 kHz stereo s16le PCM and embed as C headers.
# Requires: ffmpeg, xxd
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ASSETS="$ROOT/assets/audio"
OUT="$ROOT/src/audio_data"

mkdir -p "$ASSETS" "$OUT"

encode_one() {
  local name="$1"
  local src=""
  for ext in wav mp3 aiff flac m4a; do
    if [[ -f "$ASSETS/${name}.${ext}" ]]; then
      src="$ASSETS/${name}.${ext}"
      break
    fi
  done
  if [[ -z "$src" ]]; then
    echo "error: missing $ASSETS/${name}.{wav,mp3,...}" >&2
    exit 1
  fi

  local raw="$ASSETS/${name}.raw"
  echo "encoding $src -> $raw"
  ffmpeg -y -i "$src" \
    -f s16le -acodec pcm_s16le -ar 16000 -ac 2 \
    "$raw" </dev/null

  local bytes
  bytes=$(wc -c < "$raw" | tr -d ' ')
  # 500 ms @ 16 kHz stereo s16le = 16000 * 0.5 * 2 * 2 = 32000
  if (( bytes > 32000 )); then
    echo "warning: ${name}.raw is ${bytes} bytes (>500 ms at 16 kHz stereo)" >&2
  fi

  local header="$OUT/sound_${name}.h"
  local tmp
  tmp="$(mktemp)"
  # Run xxd from assets/audio so symbols are predictable: name_raw / name_raw_len
  (
    cd "$ASSETS"
    xxd -i "${name}.raw" > "$tmp"
  )
  sed "s/${name}_raw/assets_audio_${name}_raw/g" "$tmp" > "$header"
  rm -f "$tmp"
  echo "writing $header (${bytes} bytes PCM)"
}

encode_one login
encode_one logout
encode_one token
echo "done. Rebuild firmware to pick up new sounds."
