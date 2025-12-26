Short Kodular block instructions — Auto-connect (no ListPicker)

1) Components you need:
- Non-visible: `BluetoothClient1`
- Visible: `LabelInfo` (Label), `ButtonConnect` (optional), `Clock1` (Timer, Interval 500 ms, Enabled = true/false as needed)

2) Screen.Initialize
- set `LabelInfo.Text` to "Mencari..."
- call `AutoConnect("ESP32-BT-Mobile")`  (replace with your device name or MAC)

3) Procedure AutoConnect(targetName)
- set `found` to `false`
- set `pairedList` to `BluetoothClient1.AddressesAndNames`
- for each `item` in `pairedList` do:
  - `address` = `select list item` (split `item` by space) index 1
  - `name` = remaining text after first space (device name)
  - if `(name = targetName) or (address = targetName)` then:
    - if `BluetoothClient1.Connect(address)` then:
      - set `LabelInfo.Text` = "BT connected: Yes"
      - set `found` = `true`
      - break loop
- if `not found` then set `LabelInfo.Text` = "tidak ditemukan"

4) Clock1.Timer (poll for incoming messages to detect ESP confirmation)
- if `BluetoothClient1.IsConnected` then
  - if `BluetoothClient1.BytesAvailableToReceive` > 0 then
    - `msg` = `BluetoothClient1.ReceiveText(-1)`
    - if `msg` contains `BT:CONNECTED` then set `LabelInfo.Text` = "BT connected: Yes"

5) ButtonConnect (optional, when you know address):
- if `BluetoothClient1.Connect("AA:BB:CC:DD:EE:FF")` then set `LabelInfo.Text` = "BT connected: Yes" else set `LabelInfo.Text` = "tidak ditemukan"

Notes & tips:
- `AddressesAndNames` returns paired devices only. If your phone/app hasn't paired the ESP hardware, pair it first via Android Bluetooth settings or use an extension that supports scanning / pairing.
- The ESP firmware sends a confirmation message `BT:CONNECTED` when a device connects—use this to reliably confirm the connection.
- Use a small timeout or disable `Clock1` once connected to avoid unnecessary polling.

Want me to draw the actual Kodular block PNG or provide the blocks as a .aia helper? I can also create a step-by-step screenshot you can import into your project UI.