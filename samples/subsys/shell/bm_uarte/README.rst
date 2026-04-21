.. _shell_bm_uarte_sample:

UARTE shell sample
##################

.. contents::
   :local:
   :depth: 2

The Shell sample demonstrates how to initialize and use the shell subsystem with an NRFX UARTE based backend with |BMlong|.

Requirements
************

The sample supports the following development kits:

.. tabs::

   .. group-tab:: Simple board variants

      The following board variants do **not** have DFU capabilities:

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_non-mcuboot_variants_s145.txt

   .. group-tab:: MCUboot board variants

      The following board variants have DFU capabilities:

      .. include:: /includes/supported_boards_all_mcuboot_variants_s115.txt

      .. include:: /includes/supported_boards_all_mcuboot_variants_s145.txt

Overview
********

The sample initializes and starts an interactive shell, which can be terminated by using the "terminate" shell command.

Building and running
********************

This sample can be found under :file:`samples/subsys/shell/bm_uarte` in the |BMshort| folder structure.
For details on how to create, configure, and program a sample, see :ref:`getting_started_with_the_samples`.

Testing
=======

You can test this sample by performing the following steps:

1. Compile and program the application.
#. Observe that the ``bm-uarte:~$`` prompt is printed.
#. Type :command:`help` and press Enter.
#. Observe supported commands are printed.
#. Type :command:`terminate` and press Enter.
#. Observe that the ``goodbye`` message is printed.
#. Observe that the ``bm-uarte:~$`` prompt is printed again but the shell no longer responds to input.
