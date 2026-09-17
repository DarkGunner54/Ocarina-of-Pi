# User-supplied ROM directory

Do not add ROMs or extracted game assets to Git.

The installer copies a legally owned, compatible OoT ROM into the installed
game directory as:

```text
/opt/ocarina-of-pi/roms/baserom.z64
```

The zeldaret extraction/build workflow will consume this only after the native
port's asset loader is implemented.
