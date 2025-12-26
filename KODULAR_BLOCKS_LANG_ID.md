Panduan langkah-demi-langkah Kodular — Auto-connect (tanpa ListPicker)

1) Tambah komponen
- Non-visible: `BluetoothClient1`
- Visible: `LabelInfo` (Label), `ButtonConnect` (Button, opsional), `Clock1` (Timer, set `Interval` 500)

2) Screen1.Initialize
- Seret blok `when Screen1.Initialize`.
- Di dalamnya: `set LabelInfo.Text to "Mencari..."`.
- Kemudian panggil prosedur AutoConnect: seret blok `call AutoConnect` dan isi argumen `"ESP32-BT-Mobile"` (atau masukkan alamat MAC).
  - Jika Anda tidak melihat blok `call AutoConnect`, pastikan Anda membuat prosedur: buka drawer `Procedures`, klik `to procedure do`, beri nama `AutoConnect` dan tambahkan parameter `targetName`.
  - Alternatif: letakkan semua blok AutoConnect langsung di `Screen1.Initialize` tanpa membuat prosedur.

3) Buat prosedur `AutoConnect(targetName)`
- Blok yang digunakan:
  - `set found to false` (buat variabel lokal `found`)
  - `set pairedList to BluetoothClient1.AddressesAndNames` (mengembalikan daftar string: "MAC Name")
  - `for each item in pairedList` do:
    - `set parts to split at first space item` (gunakan blok split teks; atau split berdasarkan spasi lalu ambil dua bagian pertama)
    - `set address to select list item parts 1`
    - `set name to select list item parts 2` (atau gabungkan sisa teks jika nama mengandung spasi)
    - `if (name = targetName) or (address = targetName)` then:
      - `if BluetoothClient1.Connect(address) then`:
        - `set LabelInfo.Text to "BT connected: Yes"`
        - `set found to true`
        - `break` (gunakan variabel tambahan atau logika bertingkat untuk menghentikan loop)
  - Setelah loop: `if not found then set LabelInfo.Text to "tidak ditemukan"`

4) Clock1.Timer (cek pesan masuk untuk konfirmasi)
- Gunakan `when Clock1.Timer`:
  - `if BluetoothClient1.IsConnected then`:
    - `if BluetoothClient1.BytesAvailableToReceive > 0 then`:
      - `set msg to BluetoothClient1.ReceiveText(-1)`
      - `if msg contains "BT:CONNECTED" then set LabelInfo.Text to "BT connected: Yes"`
      - `if msg contains "BT:DISCONNECTED" then set LabelInfo.Text to "tidak ditemukan"`
- Opsi: `set Clock1.Enabled to false` setelah koneksi terkonfirmasi untuk mengurangi polling.

5) ButtonConnect (jika ingin koneksi manual dengan MAC)
- `when ButtonConnect.Click`:
  - `if BluetoothClient1.Connect("AA:BB:CC:DD:EE:FF") then set LabelInfo.Text to "BT connected: Yes" else set LabelInfo.Text to "tidak ditemukan"`

6) Catatan perilaku
- `AddressesAndNames` hanya menampilkan perangkat yang sudah dipasangkan. Jika ESP Anda belum dipasangkan, lakukan pairing lewat pengaturan Bluetooth Android terlebih dahulu.
- Firmware ESP mengirimkan pesan `BT:CONNECTED` dan `BT:DISCONNECTED` — gunakan pesan ini untuk konfirmasi koneksi yang lebih andal.
- Gunakan timeout pendek dan nonaktifkan `Clock1` setelah terhubung untuk menghemat daya.

Butuh saya buatkan juga screenshot blok per langkah (PNG)? Saya bisa buat per-block atau satu halaman, mana yang Anda mau?