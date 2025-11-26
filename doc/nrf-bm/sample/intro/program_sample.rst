.. _sample_intro_program:

Programming a sample
####################

This section describes how you program the DK with all the different firmware modules and the extra data that is needed.
This description assumes you are using |nRFVSC|.
For details, see `UI overview`_.

* `Applications View`_ - Select what to program.

  1. Select which application should be the active one.
  #. In case the project has multiple build contexts, select the one that should be used.
  #. Within the build context, you can select between programming the whole project (all images) by selecting the build folder, or just one of the images within the build context by selecting one of the domains within the build folder.

* `Actions View`_ - Specify how to program.

  * :guilabel:`Flash` - This will flash the active context selected in the Application View and only replace the memory section specified by the active context.

  * :guilabel:`Erase and Flash to Board` - This will first do a chip Erase All erasing all NVM including UICR and KMU content, before it flashes the active context.

* Select which target (DK) to program.

  In case there is only one DK connected, only that one will be flashed.
  In case you have more than one DK connected, you will be promted and can select one or multiple DKs to be flashed.

Special cases
*************

MCUboot
=======

When running samples with MCUboot and firmware loader included, the project will have signing key(s) included.
It is required to provision the key(s) when programming the board for the first time.
This can be done by programming with the :guilabel:`Erase and Flash to Board` option.
See :ref:`ug_bootloader_kmu_autokeys` for more details.

Immutable Bootloader (IRoT) configuration
=========================================

When running a sample with MCUboot set as Immutable Root of Trust (IRoT), you must take special care when programming.
See :ref:`ug_memorypartiton_irot` for more details.
