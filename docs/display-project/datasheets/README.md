# Display Project Datasheets

Datasheets now live in the shared project library: [`docs/datasheets/`](../../datasheets/README.md).

Add display-related documents there with `--used-in crowpanel-43-bringup` or
`--used-in front-display-board`, e.g.:

```powershell
python scripts/datasheet.py add ESP32-S3 --url <URL> --tags mcu,display --used-in crowpanel-43-bringup
```

Still wanted: board wiki export/PDF print, ESP32-S3 module datasheet,
display/touch controller references, and pin maps used during bring-up.
