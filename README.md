osmo-ccid-firmware - CCID implementation (not just) for firmware
================================================================

This repository contains a C-language implementation of the USB CCID
(Smart Card Reader) device class.  The code is written in a portable
fashin and can be found in the `ccid_common` sub-directory.

The code can be built to run as an userspace program on Linux,
implementing a USB Gadget using the FunctionFS interface.  For this
version, see the `ccid_host` subdirectory.

The CCID code can also be built into a firmware for the
[sysmoOCTSIM](https://www.sysmocom.de/products/lab/sysmooctsim/index.html)
8-slot high-performance USB smart card reader.

Mailing List
------------

Discussions related to osmo-ccid-firmware are happening on the
slightly unrelated simtrace@lists.osmocom.org mailing list, please see
<https://lists.osmocom.org/mailman/listinfo/simtrace> for subscription
options and the list archive.

Please observe the [Osmocom Mailing List
Rules](https://osmocom.org/projects/cellular-infrastructure/wiki/Mailing_List_Rules)
when posting.

Contributing
------------

Our coding standards are described at
<https://osmocom.org/projects/cellular-infrastructure/wiki/Coding_standards>

We us a gerrit based patch submission/review process for managing
contributions.  Please see
<https://osmocom.org/projects/cellular-infrastructure/wiki/Gerrit> for
more details

The current patch queue for osmo-ccid-firmware can be seen at
<https://gerrit.osmocom.org/#/q/project:osmo-ccid-firmware+status:open>
