.. _install_nrf_bm:

Installing the |BMlong|
#######################

.. contents::
   :local:
   :depth: 2

You can use Visual Studio Code and the `nRF Connect for Visual Studio Code`_ extension to install |BMshort|.

Install prerequisites
*********************

Install the following software tools:

* `nRF Connect for Desktop`_

  * Running `nRF Connect for Desktop`_ has the following additional requirements:

    * The |jlink_ver| of SEGGER J-Link.

      Download it from the `J-Link Software and Documentation Pack`_ page.

      * On Windows, the driver comes bundled with nRF Connect for Desktop.
      * On macOS and Linux, you must install the driver manually.

      .. note::

         On Ubuntu you might be required to install ``libfuse2``.
         To do so, open a terminal and execute: ``sudo apt install libfuse2``.

  * With the nRF54L15 DK connected to your PC, launch the Quick Start app from nRF Connect for Desktop.
    The kit will be detected by the app and it will guide you through initial configuration.
  * Complete the steps of the Quick Start wizard.
    Make sure to select :guilabel:`VS Code IDE` as the development environment and to install the `Serial Terminal app`_.

* |VSC|:

  * The latest version of |VSC| for your operating system from the `Visual Studio Code download page`_.
  * In |VSC|, the latest version of the `nRF Connect for VS Code Extension Pack`_.
    The |nRFVSC| comes with its own bundled version of some of the nRF Util commands.

.. _cloning_the_repositories_nrf_bm:

Getting the code
****************

Every |BMlong| release consists of:

* A combination of Git repositories at different versions and revisions, managed together by West.
* A pre-packaged SDK containing a source mirror of the Git repositories required to get started with |BMshort|.

.. tabs::

   .. group-tab:: Pre-packaged SDK & Toolchain

      Complete the following steps to get the |BMshort| code using the |nRFVSC|.

      1. If this is your first time installing the SDK, click :guilabel:`Install SDK` in the extension's :guilabel:`Welcome View`.

         If you have installed an SDK or a Toolchain before, click on :guilabel:`Manage SDKs` in the extension's :guilabel:`Welcome View`.
         Then, select :guilabel:`Install SDK` from the quick pick menu that appears.

      #. In the next page you will be prompted to **Select SDK type**, click :guilabel:`nRF Connect SDK Bare Metal`.

      #. In the next page you will be prompted to **Select an SDK version (or enter the branch, tag or commit SHA) to install...**, click :guilabel:`v0.9.0` marked on the right by the label :guilabel:`Pre-packaged SDKs & Toolchains`.

      #. In the next page you will be prompted to select a destination for the SDK. The default suggestion is recommended. Then press **Enter**.
         This will proceed by installing |BMshort| and the respective Toolchain it requires.

   .. group-tab:: GitHub

         1. Install the toolchain

            The |BMshort| toolchain includes tools and modules required to build the samples and applications on top of it.

            Use nRF Connect for VS Code to install the toolchain:

            .. note::
               These instructions are tested using |nRFVSC| version 2025.5.92.
               Newer versions of the extension might feature changes to the user interface.
               It is recommended to use the latest version of the extension.

            1. Open the nRF Connect extension in |VSC| by clicking its icon in the :guilabel:`Activity Bar`.
            #. If this is your first time installing the toolchain, click :guilabel:`Install Toolchain` in the extension's :guilabel:`Welcome View`.

               If you have installed a toolchain before, click on :guilabel:`Manage toolchains` in the extension's :guilabel:`Welcome View`.
               Then, select :guilabel:`Install Toolchain` from the quick pick menu that appears.

            #. The list of available stable toolchain versions appears in the |VSC|'s quick pick.
            #. Select the toolchain version to install.
               For this release of |BMshort|, use version |ncs_release| of the toolchain.

            .. note::
               Every |BMshort| release uses the toolchain of the |NCS| version that it is based on.

            The toolchain installation starts in the background, as can be seen in the notification that appears.
            If this is your first installation of the toolchain, wait for it to finish before moving to the next step of this procedure (getting the code).

            When you install the toolchain for the first time, the installed version is automatically selected for your project.

         #. Install the SDK

            Complete the following steps to get the |BMshort| code using the |nRFVSC|.

            1. In the extension's :guilabel:`Welcome View`, click :guilabel:`Manage SDKs`. A popup window will appear.

            #. Click :guilabel:`Install SDK`.

            #. In the next page you will be prompted to **Select SDK type**, click :guilabel:`nRF Connect SDK Bare Metal`.

            #. In the next page you will be prompted to **Select an SDK version (or enter the branch, tag or commit SHA) to install...**, click :guilabel:`v0.9.0` marked on the right by the label :guilabel:`GitHub`.

            #. In the next page you will be prompted to select a destination for the SDK. The default suggestion is recommended. Then press **Enter**.
               This will proceed by installing |BMshort|.

Your directory structure should now look similar to this:

.. code-block:: none

   ncs
   ├─── toolchains
   │  └─── <toolchain_version>
   └─── nrf-bm/<sdk_version>
      ├─── bootloader
      ├─── modules
      ├─── nrf
      ├─── nrf-bm
      ├─── nrfxlib
      ├─── test
      ├─── tools
      ├─── zephyr

Next steps
**********

You can now proceed to test the :ref:`samples` included in this version of |BMshort|.

The samples can be found in the :file:`nrf-bm/samples` folder, and are divided into subfolders:

* :file:`bluetooth` for the samples showcasing Bluetooth® LE functionalities using the SoftDevice.
  See :ref:`ble_samples`.
* :file:`peripherals` for the samples showcasing various peripheral functionalities that do not require the SoftDevice.
  See :ref:`peripheral_samples`.
* :file:`boot` for the samples showcasing the functionalities related to Device Firmware Update.
  See :ref:`dfu_samples`.

Each sample documentation contains full information on how to build, flash, and test the respective sample.
