# DenemeOS

C ile yazilmis, Limine bootloader kullanan 64-bit (x86_64) bir isletim sistemi.
Masaustu, pencere yoneticisi, baslat menusu, sag tik menuleri, gorev cubugu,
cop kutusu ve bir dizi yerlesik uygulama icerir.

En onemli ozellik: sistem sanal diske kurulur ve **kalicidir**. Kurulumdan
sonra F10 (veya Ayarlar > kaydet) ile dosya sistemi diske yazilir; VM kapatilip
yeniden acildiginda dosyalar, tema ve fare ayarlari korunur.

## Hizli baslangic

```sh
cd /home/denizlist/Belgeler/denemeos
./run-vbox-disk.sh              # disk imaji ile baslat (onerilen)
./run-vbox-disk.sh --headless   # ekransiz test
```

Script gerekirse kernel'i ve disk imajini kendisi olusturur, VirtualBox VM'ini
sifirdan kurar ve baslatir. Kapatmak icin:

```sh
VBoxManage controlvm DenemeOS poweroff
```

ISO ile (RAM modu, kalici degil):

```sh
./run-vbox.sh
```

## Derleme

```sh
make            # build/kernel.elf
make iso        # build/denemeos.iso
make disk       # build/disk.img + build/disk.vdi
make clean      # .o ve kernel.elf (disk imaji KORUNUR)
make distclean  # build/ tamamen silinir (disk imaji dahil)
```

Gereksinimler: `gcc, make, genisoimage, mtools, fdisk, VirtualBox` ve
`tools/limine` (Limine 12.9.6).

## Ilk kurulum

Ilk acilista **Kurulum** penceresi otomatik gelir. Enter ile ilerleyin:

1. Hos geldiniz
2. Hedef disk bilgisi
3. Onay
4. Kurulum (birkac saniye)
5. Bitti

Kurulum bittiginde dosya sistemi diske yazilir. Sonraki acilislarda sistem
dogrudan masaustu ile acilir. Kurulumu daha sonra F7 veya masaustundeki
**Kurulum** simgesiyle tekrar calistirabilirsiniz.

## Masaustu

Sol tarafta 9 simge: **Home, Terminal, Dosyalar, Editor, Hesap, Ayarlar,
Cop, Kurulum, Hakkinda**.

- **Tek tik** bir simgeyi acar; **sag tik** simge menusunu acar.
- **Masaustune sag tik**: yeni klasor/dosya, uygulamalar, ayarlar,
  diske kaydet, hakkinda.
- **Gorev cubugu (alt)**: acik pencereler, baslat dugmesi, saat ve
  diske kaydet dugmesi. Bos alana sag tik: gorev cubugu menusu.
- **Baslat menusu**: sol alttaki `DenemeOS` dugmesi veya F9.
  Ok tuslari + Enter ile klavyeden de gezilebilir.

## Kisayollar

| Tus | Islev |
|-----|-------|
| F1  | Terminal |
| F2  | Dosya Yoneticisi |
| F3  | Editor |
| F4  | Hesap Makinesi |
| F5  | Ayarlar |
| F6  | Cop Kutusu |
| F7  | Kurulum |
| F8  | Hakkinda |
| F9  | Baslat menusu |
| F10 | Diske kaydet |
| F11 | Pencereyi buyut / geri al |
| Esc | Menuyu kapat / pencereyi kapat (kurulumda geri) |

Pencere icinde:

- Baslik cubugundan surukle, sag ustteki `_` kucult, `[]` buyut, `x` kapat.
- Terminal / Editor / Dosyalar / Cop icinde **sag tik** baglam menusu.
- Editor: `Ctrl+S` kaydet.
- Ayarlar: `+` / `-` fare hassasiyeti, `1`-`3` tema, `s` diske kaydet.

## Yerlesik uygulamalar

- **Terminal** (`app_term.c`): `help, clear, ls, tree, cat, cd, pwd, mkdir,
  touch, rm, echo, write, uname, mem, df, reboot, halt`.
- **Dosya Yoneticisi** (`app_files.c`): gezinme, klasor/dosya olusturma,
  silme (cop kutusuna), sag tik menu.
- **Editor** (`app_edit.c`): satir tabanli metin duzenleyici, Ctrl+S,
  sag tik menu, yeni dosya. Baska dosya acilirken kaydedilmemis metin
  otomatik kaydedilir; yeni dosyalar mevcut bir dosyanin uzerine yazmaz,
  gerekirse `untitled2.txt` gibi benzersiz ad alir.
- **Hesap Makinesi** (`app_calc.c`): dort islem, klavye destegi.
- **Ayarlar** (`app_set.c`): 3 tema, fare hassasiyeti; degisiklikler
  otomatik olarak diske yazilir.
- **Cop Kutusu** (`app_trash.c`): geri yukle, kalici sil, bosalt.
- **Kurulum** (`app_inst.c`): adim adim kurulum sihirbazi.
- **Hakkinda** (`app_about.c`): sistem bilgileri.

## Kalicilik / disk duzeni

Disk imaji 96 MiB'tir:

| Alan | Icerik |
|------|--------|
| LBA 0 | MBR + Limine BIOS stage 1 |
| LBA 2048..131071 | FAT32 bolum 1 (kernel + Limine) |
| LBA 131072..135167 | DenemeOS snapshot alani (2 MiB) |

Dosya sistemi RAM uzerinde calisir; **F10** veya **Ayarlar > s** ile tum
agac (dosyalar, tema, fare ayari) CRC32 korumali bir snapshot olarak FAT32
bolumunun disindaki alana yazilir. Acilista snapshot dogrulanir ve geri
yuklenir. Bozuksa varsayilan dosya sistemi ile acilir.

ATA PIO surucusu birincil IDE kanalini kullanir; bu yuzden VM'de disk
**IDE PIIX4** olarak baglanir (SATA degil). ISO modunda CD IDE 0/0'da
oldugundan disk bulunamaz ve sistem RAM modunda calisir.

## Kaynak agaci

```
src/
  boot/kernel.c          cekirdek girisi, Limine istekleri, baslatma sirasi
  core/                  string, console, serial, bellek ayirici
  drivers/               keyboard, mouse, rtc, ata (PIO)
  fs/                    fs (RAM dosya sistemi), storage (snapshot)
  gui/                   gfx, ui, menu, wm, desktop
  apps/                  app_term, app_files, app_edit, app_calc,
                         app_set, app_trash, app_inst, app_about
include/                 tum basliklar (flat)
iso/                     Limine yapilandirmasi ve EFI dosyalari
tools/limine             onceden derlenmis Limine 12.9.6
```

## Teknik notlar

- Grafik 800x600x32. Cizim cift tampon + kirli dikdortgen (dirty rect)
  ile yapilir; yalnizca degisen bolge ekrana kopyalanir (titremesiz).
- Imlec arka plani saklanarak cizilir; siyah ok, beyaz cerceve.
- Kernel IDT kullanmaz; IRQ'lar kapalidir, klavye/fare PS/2 kontrolcusunden
  yoklanir. Klavye ve fare baytlari 0x64 durum kayitcisinin AUX bitiyle
  ayrilir.
- Fare 100 ornek/sn, varsayilan hassasiyet %100, kismi birikim ve hafif
  ivme kullanir; basma/birakma olaylari her poll'de kilitlenir, hizli
  tiklama kaybolmaz.
- Silme/tasima islemleri terminalin calisma dizinini (ve ust dizinlerini)
  korur; boylece cwd kokten kopup snapshot disinda kalmaz.
- Eski VM'ler `unregistervm --delete` ile silinmez: o komut VM'e bagli
  `build/disk.vdi` dosyasini da siliyordu. Scriptler diski once ayirir,
  sonra VM'i `--delete` olmadan kaldirir.
