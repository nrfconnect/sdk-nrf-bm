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

  * After installation, type **Quick Start** in the :guilabel:`Search...` box.
  * In the results, select the :guilabel:`Quick Start` app. Click :guilabel:`Install`, then click :guilabel:`Open`.
  * The Quick Start app opens.
  * On the :guilabel:`Select a kit` page, select your :guilabel:`nRF54L15 DK`, then click :guilabel:`Continue`.
  * A wizard guides you through the initial steps for using your DK. Review the content, then click :guilabel:`Continue`.
  * At the :guilabel:`Develop` step, select :guilabel:`VS Code IDE`, then click :guilabel:`Continue`. The :guilabel:`Open VS Code` page appears.

  .. important::

     Click :guilabel:`Skip`. Do not click :guilabel:`Open VS Code with extension`.

  * At the :guilabel:`Apps` step, select :guilabel:`Serial Terminal`, click :guilabel:`Install`, then click :guilabel:`Skip`.
  * At the :guilabel:`Finish` step, click :guilabel:`Close`.

* |VSC|:

  * The latest version of |VSC| for your operating system from the `Visual Studio Code download page`_.
  * In |VSC|, the latest version of the `nRF Connect for VS Code Extension Pack`_.
    The |nRFVSC| comes with its own bundled version of some of the nRF Util commands.

.. _nrf_bm_installing_toolchain:

Install the toolchain
*********************

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

.. _cloning_the_repositories_nrf_bm:

Getting the code
****************

Every |BMlong| release consists of:

* A combination of Git repositories at different versions and revisions, managed together by West.
* An archive containing a source mirror of the Git repositories required to get started with |BMshort|.

.. tabs::

   .. group-tab:: SDK Archive

      Complete the following steps to get the |BMshort| code using the SDK archive.

      1. Download the archive from the following link:

         https://files.nordicsemi.com/artifactory/ncs-src-mirror/external/sdk-nrf-bm/v0.7.0/src.tar.gz

      #. Extract the archive to the recommended location.

         .. tabs::

            .. group-tab:: Windows

               * Ensure the folder :file:`C:/ncs/bm_v0.7.0` exists.
                 If it does not exist, create it in File Explorer or by running the following command in Command Prompt:

                  .. code-block:: console

                     mkdir C:\ncs\bm_v0.7.0

               * Right-click the downloaded :file:`src.tar.gz` file.
               * Select :guilabel:`Extract All...` and choose :file:`C:/ncs/bm_v0.7.0` as destination.

            .. group-tab:: Linux

               .. code-block:: console

                  mkdir -p ~/ncs/bm_v0.7.0
                  tar -xzf src.tar.gz -C ~/ncs/bm_v0.7.0

            .. group-tab:: macOS

               .. code-block:: console

                  sudo mkdir -p /opt/nordic/ncs/bm_v0.7.0
                  sudo tar -xzf src.tar.gz -C /opt/nordic/ncs/bm_v0.7.0

         .. note::
            The extraction can take several minutes.

      #. Open the nRF Connect extension in |VSC|.

      #. In the extension's :guilabel:`Welcome View`, click on :guilabel:`Manage toolchains` and select :guilabel:`Open terminal profile`.
         The nRF Connect terminal opens with the correct environment.

      #. Navigate to the extracted SDK folder.

         .. tabs::

            .. group-tab:: Windows

               .. code-block:: console

                  cd C:/ncs/bm_v0.7.0

            .. group-tab:: Linux

               .. code-block:: console

                  cd ~/ncs/bm_v0.7.0

            .. group-tab:: macOS

               .. code-block:: console

                  cd /opt/nordic/ncs/bm_v0.7.0

      #. Run the following command to export the Zephyr CMake package:

         .. code-block:: console

            west zephyr-export

      #. In the extension's :guilabel:`Welcome View`, click the refresh icon next to :guilabel:`Manage SDKs`.
         The SDK list will be updated.

   .. group-tab:: VS Code with Git

      .. important::
         This method is NOT supported as of version |release|.
         It will be supported at official launch of |BMshort|.

      .. .. important::
            Make sure that ``git`` is installed on your system before starting this procedure.

         Complete the following steps to clone the |BMshort| repositories.

         1. Open the nRF Connect extension in |VSC| by clicking its icon in the :guilabel:`Activity Bar`.
         #. In the extension's :guilabel:`Welcome View`, click on :guilabel:`Manage SDKs`.
            The list of actions appears in the |VSC|'s quick pick.
         #. Click :guilabel:`Install SDK`.
            The list of available stable SDK versions appears in the |VSC|'s quick pick.
         #. Select the SDK version to install.
            For this release of |BMshort|, use version |ncs_release| of the SDK.

            .. note::
               The SDK installation starts and it can take several minutes.

         #. Open command line and navigate to the SDK installation folder.
            The default location to install the SDK is :file:`C:/ncs/v3.0.1` on Windows, :file:`~/ncs/v3.0.1` on Linux, and :file:`/opt/nordic/ncs/v3.0.1` on macOS.
         #. Clone the `sdk-nrf-bm`_ repository.

            .. tabs::

               .. group-tab:: Windows

                  .. code-block:: console

                     cd C:/ncs/v3.0.1
                     git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm
                     cd nrf-bm
                     git checkout v0.7.0

               .. group-tab:: Linux

                  .. code-block:: console

                     cd ~/ncs/v3.0.1
                     git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm
                     cd nrf-bm
                     git checkout v0.7.0

               .. group-tab:: macOS

                  .. code-block:: console

                     cd /opt/nordic/ncs/v3.0.1
                     git clone https://github.com/nrfconnect/sdk-nrf-bm.git nrf-bm
                     cd nrf-bm
                     git checkout v0.7.0

         #. In |VSC|, click :guilabel:`Manage SDKs` -> :guilabel:`Manage West Workspace...` -> :guilabel:`Set West Manifest Repository`.
            From the list that appears, select the ``nrf-bm`` west manifest file.
         #. Then, click :guilabel:`Manage SDKs` -> :guilabel:`Manage West Workspace...` -> :guilabel:`West Update`.
            Your local repositories will be updated.


Your directory structure should now look similar to this:

.. code-block:: none

   ncs
   ├─── toolchains
   │  └─── <toolchain_version>
   └─── bm_<sdk_version>
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

The samples can be found in the :file:`nrf-bm/samples` folder, and are divided into two subfolders:

* :file:`bluetooth` for the samples showcasing Bluetooth® LE functionalities using the SoftDevice.
  See :ref:`ble_samples`.
* :file:`peripherals` for the samples showcasing various peripheral functionalities that do not require the SoftDevice.
  See :ref:`peripheral_samples`.

Each sample documentation contains full information on how to build, flash, and test the respective sample.
