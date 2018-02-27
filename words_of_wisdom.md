# Words of Wisdom

## Problems when trying to run - can't connect to XDS110

The solution is to install the "XDS Emulation Software Package":

[link](http://processors.wiki.ti.com/index.php/XDS_Emulation_Software_Package)

If the newest one fails to download, the previous also works.
Note: you will need to register with 10minutemail to download.

## Opening a Newly cloned Repository

Start Code Composer Studio, and select the `code` directory as your workspace.
On the first time it starts, go to `Project -> Import CCS Projects`, and import
that projects it finds in this directory.

## Changing the PINs used by SPI

Go to `CC1350_LAUNCHXL.h`, and change `CC1350_LAUNCHXL_SPI0_*` as needed.
Some of the SPI pins are initialized by `BoardGpioInitTable` in `CC1350_LAUNCHXL.c`,
but this initialization seems to have no effect.
