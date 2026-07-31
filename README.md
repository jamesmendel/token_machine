## Token Machine
An ESP32-S3 based token counter based on RFID tags.

Features:
- 13.56 MHz RFID (MFRC522 reader)
- Color TFT LCD interface (320 x 240)
- Web admin panel

Built with platformio.

## Overview
### RFID Interface
The user presents their personal RFID tag to the system. The screen changes from "Waiting for tag..." to The user's info.
When the tag is presented, it referenced the tag UID to pull their name and current token count, which is presented.
The message: "Present tag to add tokens" is shown on this dashboard panel.
From here, successively removing and re-presenting the tag increments the user's token count.
There is a main and secondary activity timeout: 
- 5 seconds of no activity goes back to the idle screeen.
- 2 seconds of inactity starts a countdown "Logging out in x seconds."

### Web Interface
The device starts a hotspot with single-page admin panel. It implements the following functionality:
- Displays all registerd tags in tabular form showing: UID, Name, and token count (editable field which can be saved)
- Allows registration of new tags by presenting the tag and entering the user's Name.
- Allows reset (deletion) of tag entry.
The database is stored with esp32 perfs library.

This format allows referencing the tag database without the tag being preset. Of course, the data could also be updated on the RFID tag each time its presented, but the ESP32 is the master of the database state.

## Coding rules
Generally, the code is modularized at a subsystem level:
1. Main loop / tasks
2. RFID Interface
3. Web Interface
4. UI Interface