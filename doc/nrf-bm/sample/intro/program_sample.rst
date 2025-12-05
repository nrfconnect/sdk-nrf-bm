Programming
===========

This section describes how you program the DK with, with all the different firmware modules and the extra data that will be needed.
This description is done using nRF Connect for VS Code extension and we will assume that you are familiar with this (See `UI overview`_ for details).
Here is a short summary:


* `Applications View`_ - Decide what to program.

    1. Select which application should be the active one.
    #. In case the project have multiple build context, select the one that should be used.
    #. Within the build context you can select between programming the whole project (all images) by selecting the build folder, or just one of the images within the build context by selecting one of the domains within the build context.

* `Actions View`_ - Decide how to do the programming.

    * ``Flash`` This will flash the active context selected in Application View and only replacing the memory section specified by the active context.

    * ``Erase and Flash to Board`` This will first do a chip Erase All erasing all NVM including UICR and KMU content, before it flashes the active context.

* Decide which target (DK) to program.

    In case there is only one DK connected it will flash this one.
    In case you have more than one DK connected you will be promted and can select one or multiple of the DK to be flashed.


Special cases
*************

MCUboot
-------
When running samples with MCUboot and FW loader included, the project will have signing key(s) included.
See :ref:`ug_bootloader_kmu`, Automatic key programming section for more details.


Immutable Bootloader (IRoT) configuration
-----------------------------------------
When running sample with MCUboot set as IRoT you need to take apwcial care when programming.
See :ref:`ug_memorypartiton_irot` for more details.
