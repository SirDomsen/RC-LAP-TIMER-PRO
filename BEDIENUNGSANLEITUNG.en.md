🇩🇪 [Deutsch](BEDIENUNGSANLEITUNG.md) | 🇬🇧 **English**

# RC Lap Timer Pro – User Manual

This manual explains the most important features in simple steps. The Lap Timer is operated through a web page in your browser; no additional app is required.

## 1. What the Lap Timer Does

A small transmitter on the RC car — the **transponder** — reports every pass over the start/finish line. The Lap Timer counts laps and displays the times live on your phone, tablet, or computer.

Additional **sector stations** can record split times along the track. Without these stations, the system still measures laps at the start/finish line; split times are simply not shown.

The interface has two views:

- **Driver View:** View live times, configure voice announcements for your own phone, and open statistics.
- **Race Director View:** Set up tracks and drivers, and start or stop practice and race sessions.

## 2. Connecting to the Lap Timer

### Connect Directly to the Lap Timer Wi-Fi

When the Lap Timer is not using a home Wi-Fi or mobile hotspot:

1. Open the Wi-Fi settings on your phone.
2. Connect to the Lap Timer's Wi-Fi. The default name starts with `RC-LapTimer-`, usually followed by four characters. A custom Wi-Fi name may have been configured.
3. The default password is `Rennstrecke123`, unless it was changed.
4. If your phone warns that this Wi-Fi has no internet access, stay connected anyway.
5. Open your browser and go to `http://192.168.0.8`. If the IP address was changed earlier or remembered by the Lap Timer, a different address may apply.

### Connect via Home Wi-Fi or Mobile Hotspot

When the Lap Timer is already connected to a home Wi-Fi or hotspot, your phone and the Lap Timer must be on the same network.

When connected to a home Wi-Fi, a temporary helper Wi-Fi named like `LT_WIFINAME-IPADDRESS` may appear. The IP address at the end of the name is the Lap Timer's address. This helper Wi-Fi disappears automatically after the web page is opened for the first time.

When connected via a mobile hotspot, a temporary Wi-Fi named like `LT_HOTSPOTNAME-IPADDRESS` may also appear. The IP address at the end of the name is the Lap Timer's address. If the helper Wi-Fi does not appear, look up the Lap Timer in your phone's hotspot client list and use the IP address shown there. Open that IP address in your browser.

### Changing the Wi-Fi Connection

When you are already connected to the Lap Timer:

1. Open **Settings** in the Race Director View.
2. Under **Wi-Fi Control Center**, enter the name and password of your home Wi-Fi or mobile hotspot.
3. Tap **To Home Wi-Fi** or **To Hotspot** and confirm the switch in the next dialog.
4. The current connection will drop. Connect your phone to the same Wi-Fi as the Lap Timer. On a home Wi-Fi, you will find the IP address at the end of the temporary `LT_WIFINAME-IPADDRESS` Wi-Fi name; open that address in your browser.

Use **To AP Mode** to return to the Lap Timer's direct Wi-Fi. Name, password, and address are shown before switching. Switching interrupts the current connection; your settings remain saved.

### Switching Between Driver and Race Director Views

- The standard address opens the **Driver View**.
- Tap the role badge at the top and confirm to switch to the **Race Director View**.
- In the Race Director View, the role badge switches back to the Driver View.
- The Race Director can open **QR Code** to display a scannable code so additional phones can open the Driver View quickly.

## 3. One-Time Setup: Drivers, Cars, and Track

### Assigning Transponders

Every transponder has its own ID. To make sure times are recorded for the right driver:

1. Open **Settings** as Race Director.
2. Find the row matching the transponder ID. If you don't know the ID, power on the transmitter and point it at the Lap Timer. The detected ID appears in the bar at the bottom.
3. Enter the driver name and car.
4. Repeat for all remaining cars and choose **Save & Close**.

Lap data is stored under the transponder ID, with driver name and car mapped to that ID. Do not use the same ID for multiple cars, otherwise their times cannot be separated in statistics. Up to 15 IDs can be configured.

### Selecting or Creating a Track

In the Race Director View, the active track is shown above the table.

- Select an existing track from the list.
- To add another track, choose **New Track**, enter a name, and confirm.
- All times and statistics are recorded under the currently selected track.
- The trash icon deletes a track along with its saved sessions and statistics. This cannot be undone.

### Sector Stations

In **Settings**, the Race Director can configure which sector stations are used. `SZ1` through `SZ3` are intermediate checkpoints; the start/finish line records the lap. Only powered-on and reachable stations report split times.

## 4. Starting Practice or a Race

The Race Director starts a session using the buttons in the Race Director View. Free Practice is active by default right after powering on the Lap Timer.

Starting a session resets the live table for the new run. Previously saved statistics remain intact.

| Mode | What Happens |
|---|---|
| **Free Practice** | Laps are counted continuously without a time or lap limit. |
| **Qualifying** | Set the duration in minutes and choose **Start Quali**. Fastest lap times determine the best grid position. |
| **Race (Laps)** | Set the lap count and choose **Start Laps**. A synchronized countdown runs before the start. |
| **Race (Time)** | Set the duration in minutes and choose **Start Time**. A synchronized countdown runs before the start. |
| **Drag Race** | Choose **Start Drag Race**. Timing starts when crossing the base station. At least one sector station is required for the finish; the last sector station acts as the finish line. |
| **Ghost Battle** | Choose a saved reference time as the target and start the comparison for all drivers. See [Ghosthunt and Ghost Battle](#7-ghosthunt-and-ghost-battle) for details. |

**Stop Session** ends the current run and returns to Free Practice.

### Full Reset

**Full Reset** clears current laps and sector times. In the confirmation dialog, resetting daily best times is pre-selected and can be unchecked there. The all-time track record is preserved.

A reset is not the same as deleting a track: it does not delete saved race history. Tracks including their history can be deleted via the trash icon in the track selector.

## 5. Understanding the Live Display

The table updates while driving. Depending on the race mode, columns include:

- **Pos:** current ranking
- **Driver / Car:** name and vehicle assigned to the transponder ID
- **Lap:** counted laps
- **SZ1–SZ3:** split times at the configured sector stations
- **SZ4:** time at the start/finish line
- **Last Time:** most recently recorded lap
- **Best Time:** driver's best lap in the current session
- **Gap:** time difference to the leader or best time

A **sector** is a section of the track. A split time shows how long the car took to reach that checkpoint.

For a high-visibility phone display, tap **Dashboard**. It shows key live data in large text on a dark background. Tap **Back** to return to the standard view.

### Voice Announcements and Sounds

- Use **Audio: ON/OFF** to toggle voice announcements on your phone.
- In the Driver View, you can choose which car's times are announced. This setting applies only to your phone.
- In the Race Director **Settings**, you can select which cars are announced on that device.
- If an iPhone or iPad stays silent at first, tap **iOS Audio** to unlock audio playback.
- The Race Director can also use **Arcade Audio** to start or stop sounds and music on the base station's hardware speaker — a tribute to 1980s arcade classics!

## 6. Statistics

Open **Statistics** to view saved results. Tabs organize data by records, drivers, and session types. Opening a driver profile or session reveals individual laps and split times.

- **Track Record:** the fastest saved lap on a track.
- **Daily Best:** the fastest lap of the current day.
- **Top 5:** shows the top all-time records and daily bests.

Statistics are stored on the Lap Timer. You can create a full backup in **System Maintenance** (see [Section 9](#9-system-maintenance)).

If a low-storage warning appears, do not delete tracks or statistics you want to keep. Deleting a track also removes its saved sessions.

## 7. Ghosthunt and Ghost Battle

A **Ghost** is a saved lap time that your current lap is compared against. No physical second car is driving.

1. Open **Ghosthunt** in the Driver View or **Ghost Battle** in the Race Director View. Ghost Battle is the multiplayer version of Ghosthunt.
2. Choose your comparison target: e.g., Track Record, Daily Best, Personal Best, Leading Car, or a custom target time.
3. Start the comparison. The display shows your current lap and the time delta to the target, including at sector stations.

The Driver View starts a personal Ghosthunt on that device. The Race Director can instead start a Ghost Battle for the whole session. During a shared Ghost Battle, a driver can still pick a personal target and return to the shared target at any time via **Use Admin Ghost**.

## 8. Troubleshooting

### No Laps Are Shown

- Check that the car has a powered-on transponder.
- Check in **Settings** that the transponder ID is assigned to the right driver.
- Verify that the web page still shows **LIVE** and that your phone is connected to the same Wi-Fi as the Lap Timer.
- Without sector stations there are no split times, but the start/finish line should still count laps.

### Split Times Are Missing

Check that the corresponding sector station is powered on, within range, and enabled in **Settings**. Without a working station, that checkpoint cannot be measured.

Sector station status is shown at the top:

- **Red:** station unreachable.
- **Green:** station reachable; sending in general broadcast mode.
- **Purple:** station using direct radio link to the base.
- **Orange:** station battery is low.

### Phone Cannot Find the Web Page

Connect your phone to the same Wi-Fi as the Lap Timer. On the Lap Timer's direct Wi-Fi, keep the phone connected even if it warns "No Internet". Scan the QR code or check whether the IP address changed.

### No Voice Announcements

Check **Audio: ON/OFF**, the selected car filter, and — on iPhone or iPad — the **iOS Audio** button. Voice output runs locally on each phone and may be muted independently of other devices.

## 9. System Maintenance

System Maintenance is intended for updates and backups.

> **Important:** Updates can interrupt timing. Stop any active session first. Only upload files built for your specific device and version. If you are unsure which file you need, ask the person maintaining the hardware.

### Updating the Web Interface

The web interface consists of the web pages and their assets, updated separately from the firmware.

1. Open **Web Update** from the Race Director View.
2. First create a storage backup using **Backup (.zip)** and save the ZIP file safely on your computer.
3. Under **Upload Files**, select the provided web files. Only use a ZIP archive if it is explicitly intended as an update package.
4. You can select individual files or an entire folder. Automatic compression for web files is already enabled.
5. Start the upload and wait for the success message.

After uploading, return to the Race Director View and reload the page. When restoring a ZIP backup, the page inspects its contents and asks for confirmation before restoring. Files with matching names will be overwritten, so always back up first.

### Updating Firmware (`.bin`)

A firmware update changes the base station's program. The firmware file ends in `.bin` and must match your exact base station model and version.

1. Open **Web Update** and scroll to **Firmware OTA Update (.bin)**.
2. Select the matching `.bin` file.
3. Verify the filename and choose **Flash Firmware Now**.
4. Confirm the prompt. Do not disconnect Wi-Fi or power and do not close the page during the transfer.
5. Once finished, the base station reboots automatically and the web page will briefly disconnect.

Only use a matching firmware file. If unsure, ask the person maintaining the device before flashing.

### Updating Sector Stations

**SZ OTA Maintenance Mode** is required to update sector station firmware over the air; it does not work in mobile hotspot mode. If triggered accidentally, power-cycle the affected sector stations so they boot normally again.

### Creating or Restoring a Full Backup

Using **Backup (.zip)** in the file manager downloads a ZIP archive of the files on the base station. Keep it in a safe place and do not share it publicly, as it may contain Wi-Fi credentials and device settings.

To restore, choose **Restore Backup (.zip)**, select the saved ZIP file, and check the confirmation prompt. Existing files with the same name will be overwritten. Because the full backup also includes web interface files, restoring an older backup will revert the web interface as well; if you only want to restore statistics, unpack the ZIP and upload just the desired statistics files individually.

This ZIP file is a full filesystem backup. Offline Statistics can also import it as a data source, reading only statistics files without modifying the base station. See [Section 10](#10-offline-statistics).

## 10. Offline Statistics

Offline Statistics lets you browse saved results without being connected to the Lap Timer. Open [offline_stats/index.html](LT_Base/offline_stats/index.html) from the project folder in a browser (or use the standalone single-file `offline_stats_standalone.html` from Releases).

1. Choose **Select ZIP/JSON**.
2. Load a previously created full Lap Timer backup (`.zip`) or a supported statistics backup (`.json`).
3. The statistics are displayed in your browser. The offline view does not modify any data on the base station.
4. Use **Save / Download Stats ZIP** to export the merged statistics database so you can re-import it later or open it in another browser.

Offline Statistics stores its database in your browser profile on this device. Clearing browser data or switching profiles removes or separates that local database, so keep the exported **Stats ZIP** as a portable backup.

The **Stats ZIP** contains only statistics data — it is not the same as the full **Backup (.zip)** from System Maintenance and cannot be used to update firmware or the web interface.
