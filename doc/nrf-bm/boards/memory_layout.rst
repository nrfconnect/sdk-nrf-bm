.. _board_memory_layouts:

Board memory layouts
####################

.. contents::
   :local:
   :depth: 3

This page documents the RRAM (flash) and SRAM partition layouts for supported ``bm_*`` board configurations. Use the three-level tabs (**DK** → **SoC** → **SoftDevice**) to view layouts for each combination.
Each diagram shows the partitions from low address (bottom) to high address (top), with hex addresses on the left and sizes on the right.

The diagrams are auto-generated from the board devicetree sources by :file:`doc/_scripts/gen_memory_layouts.py`.
Re-run that script when the devicetree files change:

.. code-block:: console

   python doc/_scripts/gen_memory_layouts.py

Partition descriptions
**********************

The diagrams use descriptive names. The table below maps each to the devicetree partition label and describes its role.

RRAM partitions
===============

.. list-table::
   :header-rows: 1
   :widths: 22 18 60

   * - Partition (diagram)
     - DTS label
     - Description
   * - MCUboot
     - ``boot``
     - MCUboot bootloader image. Present only in MCUboot configurations.
   * - Storage: Peer Manager
     - ``peer_manager``
     - Persistent storage for Bluetooth bonding and peer data.
   * - Storage: User data
     - ``storage0``
     - General-purpose non-volatile storage using BM ZMS.
   * - Application
     - ``slot0``
     - Main application firmware image.
   * - Firmware Loader
     - ``slot1``
     - Firmware loader / secondary slot for DFU. Present only in MCUboot configurations.
   * - SoftDevice
     - ``softdevice``
     - Nordic SoftDevice Bluetooth LE stack binary.
   * - Metadata
     - ``metadata``
     - MCUboot metadata region. Present only in MCUboot configurations.

SRAM partitions
===============

.. list-table::
   :header-rows: 1
   :widths: 22 18 60

   * - Partition (diagram)
     - DTS label
     - Description
   * - KMU reserved
     - —
     - 128 bytes reserved by the Key Management Unit at the start of SRAM (synthetic region, no partition label in DTS).
   * - SoftDevice Static RAM
     - ``softdevice_static_ram``
     - Statically allocated RAM for the SoftDevice. Size depends on the SoftDevice variant (S115 vs S145).
   * - SoftDevice Dynamic RAM
     - ``softdevice_dynamic_ram``
     - Dynamically allocated RAM for the SoftDevice (connections, buffers).
   * - Application RAM
     - ``app_ram``
     - RAM available for the application.
   * - Retained RAM
     - ``RetainedMem``
     - Retained memory region for MCUboot settings persistence across resets. Present only in MCUboot configurations.

Memory layout diagrams
**********************

Select **DK** → **SoC** → **SoftDevice** to view RRAM and SRAM layout diagrams.

.. tabs::

   .. group-tab:: nRF54L15-DK

      .. tabs::

         .. group-tab:: nRF54L15

            Total RRAM: 1524 KB | Total SRAM: 256 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54l15dk_nrf54l15_cpuapp_s115_softdevice.svg
                     :alt: nRF54L15 S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l15_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54L15 S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice.svg
                     :alt: nRF54L15 S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l15_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54L15 S145 + MCUboot memory layout

         .. group-tab:: nRF54L10

            Total RRAM: 1012 KB | Total SRAM: 192 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54l15dk_nrf54l10_cpuapp_s115_softdevice.svg
                     :alt: nRF54L10 S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l10_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54L10 S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54l15dk_nrf54l10_cpuapp_s145_softdevice.svg
                     :alt: nRF54L10 S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l10_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54L10 S145 + MCUboot memory layout

         .. group-tab:: nRF54L05

            Total RRAM: 500 KB | Total SRAM: 96 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54l15dk_nrf54l05_cpuapp_s115_softdevice.svg
                     :alt: nRF54L05 S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l05_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54L05 S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54l15dk_nrf54l05_cpuapp_s145_softdevice.svg
                     :alt: nRF54L05 S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54l15dk_nrf54l05_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54L05 S145 + MCUboot memory layout

   .. group-tab:: nRF54LM20-DK

      .. tabs::

         .. group-tab:: nRF54LM20A

            Total RRAM: 2048 KB | Total SRAM: 512 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s115_softdevice.svg
                     :alt: nRF54LM20A S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54LM20A S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s145_softdevice.svg
                     :alt: nRF54LM20A S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54lm20dk_nrf54lm20a_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54LM20A S145 + MCUboot memory layout

   .. group-tab:: nRF54LS05-DK

      .. tabs::

         .. group-tab:: nRF54LS05B

            Total RRAM: 508 KB | Total SRAM: 96 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54ls05dk_nrf54ls05b_cpuapp_s115_softdevice.svg
                     :alt: nRF54LS05B S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54ls05dk_nrf54ls05b_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54LS05B S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54ls05dk_nrf54ls05b_cpuapp_s145_softdevice.svg
                     :alt: nRF54LS05B S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54ls05dk_nrf54ls05b_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54LS05B S145 + MCUboot memory layout

   .. group-tab:: nRF54LV10-DK

      .. tabs::

         .. group-tab:: nRF54LV10A

            Total RRAM: 1012 KB | Total SRAM: 191 KB

            .. tabs::

               .. group-tab:: S115

                  .. rubric:: S115

                  .. image:: images/bm_nrf54lv10dk_nrf54lv10a_cpuapp_s115_softdevice.svg
                     :alt: nRF54LV10A S115 memory layout

                  .. rubric:: S115 + MCUboot

                  .. image:: images/bm_nrf54lv10dk_nrf54lv10a_cpuapp_s115_softdevice_mcuboot.svg
                     :alt: nRF54LV10A S115 + MCUboot memory layout

               .. group-tab:: S145

                  .. rubric:: S145

                  .. image:: images/bm_nrf54lv10dk_nrf54lv10a_cpuapp_s145_softdevice.svg
                     :alt: nRF54LV10A S145 memory layout

                  .. rubric:: S145 + MCUboot

                  .. image:: images/bm_nrf54lv10dk_nrf54lv10a_cpuapp_s145_softdevice_mcuboot.svg
                     :alt: nRF54LV10A S145 + MCUboot memory layout
