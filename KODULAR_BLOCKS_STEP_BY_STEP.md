Step-by-step Kodular block instructions — Auto-connect (no ListPicker)

1) Add components
- Non-visible: `BluetoothClient1`
- Visible: `LabelInfo` (Label), `ButtonConnect` (Button, optional), `Clock1` (Timer, set `Interval` 500)

2) Screen1.Initialize
  - Drag `when Screen1.Initialize` event.
  - Inside it: `set LabelInfo.Text to "Mencari..."`.
  - Then call your procedure: drag the `call AutoConnect` block and attach the text `"ESP32-BT-Mobile"` (or the MAC address) as the `targetName` argument.
    - If you don't see `call AutoConnect`, make sure you created the procedure: open the `Procedures` drawer, click `to procedure do`, name it `AutoConnect` and add a parameter `targetName`.
    - Alternatively, you can paste the AutoConnect blocks directly inside `Screen1.Initialize` instead of using a procedure.

3) Create a procedure `AutoConnect(targetName)`
- Blocks to use:
  - `set found to false` (create a local variable `found`)
  - `set pairedList to BluetoothClient1.AddressesAndNames` (this returns list of strings: "MAC Name")
  - `for each item in pairedList` do:
    - `set parts to split at first space item` (use text split block; or split by space then take first two)
    - `set address to select list item parts 1`
    - `set name to select list item parts 2` (or reconstruct the rest if name has spaces)
    - `if (name = targetName) or (address = targetName)` then:
      - `if BluetoothClient1.Connect(address) then`:
        - `set LabelInfo.Text to "BT connected: Yes"`
        - `set found to true`
        - `break` (use an extra variable and a nested logic to stop looping)
  - After loop: `if not found then set LabelInfo.Text to "tidak ditemukan"`

4) Clock1.Timer (poll incoming messages for confirmation)
- Use `when Clock1.Timer`:
  - `if BluetoothClient1.IsConnected then`:
    - `if BluetoothClient1.BytesAvailableToReceive > 0 then`:
      - `set msg to BluetoothClient1.ReceiveText(-1)`
      - `if msg contains "BT:CONNECTED" then set LabelInfo.Text to "BT connected: Yes"`
      - `if msg contains "BT:DISCONNECTED" then set LabelInfo.Text to "tidak ditemukan"`
- Optionally `set Clock1.Enabled to false` when connected to reduce polling.

5) ButtonConnect (if you prefer manual connect by MAC)
- `when ButtonConnect.Click`:
  - `if BluetoothClient1.Connect("AA:BB:CC:DD:EE:FF") then set LabelInfo.Text to "BT connected: Yes" else set LabelInfo.Text to "tidak ditemukan"`

6) Behavior notes
- `AddressesAndNames` lists paired devices only. If your ESP isn't paired, pair from Android settings first.
- ESP firmware sends `BT:CONNECTED` and `BT:DISCONNECTED` messages — use those to reliably confirm connection.
- Use short timeouts and disable `Clock1` after connection to save battery.

If you want, I can now:
- Produce PNG screenshots of each block group (Screen.Initialize, AutoConnect procedure, Clock Timer, ButtonConnect), or
- Produce a single-page PNG ready to include in your documentation.

Which would you prefer? (PNG per-block or single-page image?)