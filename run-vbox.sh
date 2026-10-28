#!/bin/bash
# DenemeOS - ISO ile VirtualBox VM'i olustur ve baslat.
#
#   ./run-vbox.sh              # GUI
#   ./run-vbox.sh --headless   # ekransiz
#
# Disk imaji yolu (run-vbox-disk.sh) daha guvenilirdir.

set -u

BASE="$(cd "$(dirname "$0")" && pwd)"
VM="DenemeOS-ISO"
ISO="$BASE/build/denemeos.iso"
KERNEL="$BASE/build/kernel.elf"
TYPE="gui"
[ "${1:-}" = "--headless" ] && TYPE="headless"

say() { echo "[denemeos] $*"; }
die() { echo "[denemeos] HATA: $*" >&2; exit 1; }

command -v VBoxManage >/dev/null 2>&1 || die "VBoxManage bulunamadi."
cd "$BASE" || die "proje dizinine girilemedi"

MACHINE_DIR="$(VBoxManage list systemproperties 2>/dev/null \
    | sed -n 's/^Default machine folder:[[:space:]]*//p')"
[ -n "$MACHINE_DIR" ] || MACHINE_DIR="$HOME/VirtualBox VMs"

vm_state() {
    VBoxManage showvminfo "$1" --machinereadable 2>/dev/null \
        | sed -n 's/^VMState="\(.*\)"$/\1/p'
}

# eski VM'leri --delete OLMADAN kaldir (ISO dosyasi silinmesin)
for name in $(VBoxManage list vms 2>/dev/null | sed -n 's/^"\([^"]*\)".*/\1/p'); do
    case "$name" in
        DenemeOS-ISO|DenemeOS_ISO*|DenemeOS_*|TestISO) ;;
        *) continue ;;
    esac

    say "Eski VM kaldiriliyor: $name"
    VBoxManage controlvm "$name" poweroff >/dev/null 2>&1 || true
    for i in $(seq 1 40); do
        s="$(vm_state "$name")"
        [ -z "$s" ] && break
        [ "$s" = "poweroff" ] && break
        sleep 0.25
    done
    VBoxManage storageattach "$name" --storagectl "IDE" --port 0 --device 0 \
        --type dvddrive --medium none >/dev/null 2>&1 || true
    for i in $(seq 1 20); do
        VBoxManage unregistervm "$name" >/dev/null 2>&1 && break
        sleep 0.25
    done
    rm -rf "${MACHINE_DIR:?}/$name" 2>/dev/null || true
done

# ISO'yu (gerekirse) derle
if [ ! -f "$ISO" ] || ! cmp -s "$KERNEL" "$BASE/build/.iso-kernel"; then
    say "ISO derleniyor..."
    make iso || die "ISO olusturulamadi (make iso)"
fi
[ -f "$ISO" ] || die "build/denemeos.iso bulunamadi"

if VBoxManage list vms 2>/dev/null | grep -q "\"$VM\""; then
    VM="DenemeOS-ISO_$$"
fi

say "VM olusturuluyor..."
VBoxManage createvm --name "$VM" --ostype "Other_64" --register >/dev/null \
    || die "createvm basarisiz"
VBoxManage modifyvm "$VM" --memory 512 --vram 32 --cpus 1 \
    --boot1 dvd --boot2 none --boot3 none --boot4 none \
    --audio-driver none --nic1 none --mouse ps2 --keyboard ps2 >/dev/null \
    || die "modifyvm basarisiz"
VBoxManage storagectl "$VM" --name "IDE" --add ide --controller PIIX4 >/dev/null \
    || die "storagectl basarisiz"
VBoxManage storageattach "$VM" --storagectl "IDE" --port 0 --device 0 \
    --type dvddrive --medium "$ISO" \
    || die "ISO baglanamadi"

say "VM hazir. Baslatiliyor ($TYPE)..."
VBoxManage startvm "$VM" --type "$TYPE" || die "VM baslatilamadi"
say "Tamam. VM adi: $VM"
