#!/bin/bash
# DenemeOS - disk imajini (yeniden) olustur, VirtualBox VM'sini sifirdan kur ve baslat.
#
#   ./run-vbox-disk.sh              # GUI
#   ./run-vbox-disk.sh --headless   # ekransiz (test icin)
#
# Onemli: eski VM "unregistervm --delete" ile silinince VirtualBox, VM'e
# bagli olan disk.vdi dosyasini da SILIYORDU. Bir sonraki calistirmada
# "disk.vdi bulunamadi" / "boot failure" hatasi bu yuzden olusuyordu.
# Bu script once diski ayirir, sonra VM'i --delete OLMADAN kaldirir.

set -u

BASE="$(cd "$(dirname "$0")" && pwd)"
VM="DenemeOS"
VDI="$BASE/build/disk.vdi"
KERNEL="$BASE/build/kernel.elf"
TYPE="gui"
[ "${1:-}" = "--headless" ] && TYPE="headless"

say() { echo "[denemeos] $*"; }
die() { echo "[denemeos] HATA: $*" >&2; exit 1; }

command -v VBoxManage >/dev/null 2>&1 || die "VBoxManage bulunamadi. VirtualBox kurulu mu?"
cd "$BASE" || die "proje dizinine girilemedi: $BASE"

MACHINE_DIR="$(VBoxManage list systemproperties 2>/dev/null \
    | sed -n 's/^Default machine folder:[[:space:]]*//p')"
[ -n "$MACHINE_DIR" ] || MACHINE_DIR="$HOME/VirtualBox VMs"

vm_state() {
    VBoxManage showvminfo "$1" --machinereadable 2>/dev/null \
        | sed -n 's/^VMState="\(.*\)"$/\1/p'
}

# --- 1) eski VM'leri guvenli sekilde kaldir -------------------------------
for name in $(VBoxManage list vms 2>/dev/null | sed -n 's/^"\([^"]*\)".*/\1/p'); do
    case "$name" in
        DenemeOS|DenemeOS_*|DenemeOS-*|TestISO) ;;
        *) continue ;;
    esac

    say "Eski VM kaldiriliyor: $name"
    VBoxManage controlvm "$name" poweroff >/dev/null 2>&1 || true

    # poweroff asenkron: oturum kilidi birakilana kadar bekle
    for i in $(seq 1 40); do
        s="$(vm_state "$name")"
        [ -z "$s" ] && break
        [ "$s" = "poweroff" ] && break
        sleep 0.25
    done

    # diski VM'den ayir (--delete'in dosyayi silmesini engeller)
    VBoxManage storageattach "$name" --storagectl "SATA" --port 0 --device 0 \
        --type hdd --medium none >/dev/null 2>&1 || true
    VBoxManage storageattach "$name" --storagectl "IDE" --port 0 --device 0 \
        --type hdd --medium none >/dev/null 2>&1 || true
    VBoxManage storageattach "$name" --storagectl "IDE" --port 0 --device 0 \
        --type dvddrive --medium none >/dev/null 2>&1 || true

    for i in $(seq 1 20); do
        VBoxManage unregistervm "$name" >/dev/null 2>&1 && break
        sleep 0.25
    done

    rm -rf "${MACHINE_DIR:?}/$name" 2>/dev/null || true
done

# --- 2) disk.vdi'ye isaret eden olu medya kayitlarini temizle -------------
# (VM silindikten sonra kalan kayitlar, yeniden olusturulan VDI'nin
#  "inaccessible" gorunmesine ve power-up'in abort olmasina yol acabiliyor)
VBoxManage list hdds 2>/dev/null | awk -v p="$VDI" '
    /^UUID:/     { u = $2 }
    /^Location:/ { if (index($0, p) > 0) print u }
' | while read -r uuid; do
    [ -n "${uuid:-}" ] && VBoxManage closemedium disk "$uuid" >/dev/null 2>&1 || true
done

# --- 3) kernel ------------------------------------------------------------
# Her zaman artimli derle: kaynak degistiyse kernel tazelenir, degismediyse hizli.
say "Kernel derleniyor..."
make || die "derleme basarisiz (make)"

# --- 4) disk imaji --------------------------------------------------------
if [ ! -f "$VDI" ] || ! cmp -s "$KERNEL" "$BASE/build/.disk-kernel"; then
    say "Disk imaji olusturuluyor..."
    make disk || die "disk imaji olusturulamadi (make disk)"
fi
[ -f "$VDI" ] || die "build/disk.vdi bulunamadi"

# VDI gercekten okunabiliyor mu? (yarim kalmis/bozuk imajlari yakala)
if ! VBoxManage showmediuminfo disk "$VDI" 2>/dev/null | grep -q '^Capacity:[[:space:]]*[1-9]'; then
    say "VDI okunamadi, yeniden olusturuluyor..."
    rm -f "$VDI" "$BASE/build/.disk-kernel"
    make disk || die "disk imaji yeniden olusturulamadi"
fi

# --- 5) yeni VM -----------------------------------------------------------
if VBoxManage list vms 2>/dev/null | grep -q "\"$VM\""; then
    VM="DenemeOS_$$"
    say "Eski isim hala kayitli, yeni isim: $VM"
fi

say "VM olusturuluyor..."
VBoxManage createvm --name "$VM" --ostype "Other_64" --register >/dev/null \
    || die "createvm basarisiz"

VBoxManage modifyvm "$VM" --memory 512 --vram 32 --cpus 1 \
    --boot1 disk --boot2 none --boot3 none --boot4 none \
    --audio-driver none --nic1 none --mouse ps2 --keyboard ps2 >/dev/null \
    || die "modifyvm basarisiz"

# ATA PIO surucumuz birincil IDE kanalini (PIIX4) kullaniyor.
VBoxManage storagectl "$VM" --name "IDE" --add ide --controller PIIX4 >/dev/null \
    || die "storagectl basarisiz"

VBoxManage storageattach "$VM" --storagectl "IDE" --port 0 --device 0 \
    --type hdd --medium "$VDI" \
    || die "disk baglanamadi: $VDI"

VBoxManage showvminfo "$VM" --machinereadable 2>/dev/null | grep -qE '^"?IDE-0-0"=' \
    || die "disk dogrulanamadi (IDE port 0 bos)"

say "VM hazir. Baslatiliyor ($TYPE)..."
VBoxManage startvm "$VM" --type "$TYPE" || {
    say "GUI baslatilamadi, headless deneniyor..."
    VBoxManage startvm "$VM" --type headless || die "VM baslatilamadi"
}

say "Tamam. VM adi: $VM"
say "Kapatmak icin: VBoxManage controlvm \"$VM\" poweroff"
