#!/usr/bin/env bash
set -euo pipefail

EGOS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OSI_DIR="$(cd "$EGOS_DIR/.." && pwd)"
QEMU_BIN="${QEMU_BIN:-/usr/bin/qemu-system-riscv32}"

source "$OSI_DIR/env.sh"

make -C "$EGOS_DIR" install >/dev/null

tmpdir="$(mktemp -d)"
fifo="$tmpdir/qemu.in"
log="$tmpdir/qemu.log"
body="$tmpdir/body.txt"
headers="$tmpdir/headers.txt"
mkfifo "$fifo"

cleanup() {
    if [[ -n "${qemu_pid:-}" ]] && kill -0 "$qemu_pid" 2>/dev/null; then
        printf '\001x' >&3 || true
        sleep 1
        kill "$qemu_pid" 2>/dev/null || true
        wait "$qemu_pid" 2>/dev/null || true
    fi
    exec 3>&- || true
    rm -rf "$tmpdir"
}
trap cleanup EXIT

(
    exec "$QEMU_BIN" \
        -M virt -smp 1 -m 8M \
        -bios "$EGOS_DIR/tools/egos.bin" \
        -nographic \
        -drive if=pflash,format=raw,unit=1,file="$EGOS_DIR/tools/qemuROM.bin" \
        -device sdhci-pci,addr=0x1 \
        -device sd-card,drive=MMC \
        -drive if=none,file="$EGOS_DIR/tools/disk.img",format=raw,id=MMC \
        -device e1000,netdev=E1000,addr=0x3,mac=52:54:00:00:00:01 \
        -netdev user,id=E1000,hostfwd=tcp::8080-:80 \
        <"$fifo" >"$log" 2>&1
) &
qemu_pid=$!

exec 3>"$fifo"

for _ in $(seq 1 60); do
    if grep -q "Welcome to the egos-2k+ shell" "$log"; then
        break
    fi
    sleep 1
done

if ! grep -q "Welcome to the egos-2k+ shell" "$log"; then
    echo "QEMU did not reach the shell prompt"
    cat "$log"
    exit 1
fi

printf 'http\r' >&3
sleep 1

curl -sS --max-time 5 -D "$headers" -o "$body" http://127.0.0.1:8080/

echo "HTTP headers:"
cat "$headers"
echo
echo "HTTP body:"
cat "$body"
echo

if ! grep -q "HTTP/1.0 200 OK" "$headers"; then
    echo "Expected HTTP 200 response"
    exit 1
fi

if ! grep -q "EGOS socket server skeleton" "$body"; then
    echo "Expected demo response body"
    exit 1
fi

echo "Demo passed: socket-backed HTTP path is working."
